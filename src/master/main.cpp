#include <Arduino.h>
#include <Adafruit_AW9523.h>
#include <SPI.h>
#include "printf.h"
#include "RF24.h"
//#include "WT2605C_Player.h"
#include <SoftwareSerial.h>


#include <master_pins.h>
#include <util.h>
#include <communication.h>
#include <button.h>

// or the state, the modes should be just after the STOPGAME
enum State{SETUP, STOPGAME, GAMEMODE1, GAMEMODE2, GAMEMODE3};
enum DifficultyLevel {EASY, MEDIUM, HARD};

struct PlayerStruct{
  uint8_t modules; //mask
  uint8_t nbrOfModules; //calcul dynamique avec modules
  uint16_t score;
};
struct ModuleStruct{
  int8_t playerOfModule;
  uint8_t buttonsToPress; //mask
};
PlayerStruct players[MAX_PLAYERS];
ModuleStruct modules[NBR_SLAVES];

struct Difficulty {
  uint16_t ledDelay[2]; // Delay for LED
  uint16_t pressTime;   // Time allowed to press the button
  float speedFactor; // Factor to change speed based on difficulty
};

Difficulty difficultySettings[] {
  {{6000, 7000}, 5000, 0.05}, // EASY
  {{4000, 5000}, 3000, 0.1}, // MEDIUM
  {{3000, 4000}, 1000, 0.2} // HARD
};

void initPlayers(PlayerStruct* players, ModuleStruct* modules);
void sendCommand(MasterCommand command, uint8_t receivers);
void sendScore(uint8_t playerId, uint16_t score);
bool readFromSlave(PayloadFromSlaveStruct& payload);
uint8_t assignButtons(PlayerStruct* players, ModuleStruct* modules, uint8_t nbrButtons);
void assignModules(PlayerStruct* players, ModuleStruct* modules, PayloadFromSlaveStruct* AllPayloadFromSlaves);

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);
unsigned long lastSendTime = 0;

// MP3
SoftwareSerial SSerial(3, 2); //use D2,D3 to simulate RX,TX
//WT2605C<SoftwareSerial> Mp3Player;

State actualState = STOPGAME; // Added a similar state as in slave to switch 
Button* StartButton; // Defined the start and setup button and states
Button* SetUpButton;
PayloadFromSlaveStruct AllPayloadFromSlaves[NBR_SLAVES];
uint8_t fromSlaveID[NBR_SLAVES];
uint16_t scores[MAX_PLAYERS] = {0};
uint8_t receivers;
PayloadFromSlaveStruct payloadFromSlave;

bool gamemode1CommandSent = false;
bool waitingForResponse = false;

unsigned long currentMillis = millis();
unsigned long previousMillis = currentMillis;

void setup() {
  Serial.begin(9600);
  Serial.println("Je suis le master !");

  // initialize the transceiver on the SPI bus
  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {}  // hold in infinite loop
  }
  // Inizialization of buttons and potentiometers
  StartButton = new Button(START_BUTTON_PIN, START_LED_PIN);
  SetUpButton = new Button (SETUP_BUTTON_PIN, SETUP_LED_PIN);

  pinMode(POTENTIOMETER_MODE_PIN, INPUT);
  pinMode(POTENTIOMETER_DIFFICULTY_PIN, INPUT);

  radio.setChannel(125);
  radio.setPALevel(RF24_PA_LOW);
  radio.setRetries(5, 15);
  radio.setAutoAck(true);
  radio.enableDynamicPayloads(); //as we have different payload size
  Serial.print(F("Addresses to receive: "));
  for (uint8_t i = 0; i < NBR_SLAVES; ++i){
    Serial.print(F("  Pipe "));
    Serial.print(i + 1);
    Serial.print(F(" → Adresse: 0x"));
    print64Hex(addresses[1]+i);  // affiche l'adresse en hexadécimal
    radio.openReadingPipe(i+1, addresses[1]+i); //use pipes 1-5 for reception, pipe 0 is reserved for transmission in this code
  }
  radio.startListening();  // put radio in RX mode
  lastSendTime = millis();

  initPlayers(players, modules);
}

void loop() {
  bool newPayloadReceived = readFromSlave(payloadFromSlave); // update the payloads from slaves, récuperer le payload

  ButtonState stateStartButton = StartButton->state();
  switch (stateStartButton){
    case JUST_PRESSED:{
      SetUpButton->turnOffLed();
      StartButton->turnOnLed();

      // the game was not running, but now it is starting
      Serial.println( "Start Pressed! Start game");
      // If the game is in setup mode, we need to assign the modules to the players
      if(actualState == SETUP){
        Serial.println( "Start Pressed when in setup mode! Assigning modules");
        assignModules(players, modules, AllPayloadFromSlaves);
      }
      // Set the game mode based on the potentiometer value to be able to do it easily without needing to reasign the modules
      //uint8_t gameMode = map(analogRead(POTENTIOMETER_MODE_PIN), 0, 1023, 1, 2);
      uint8_t gameMode = 2;
      actualState = static_cast<State>(STOPGAME + gameMode);
      previousMillis = millis(); //reset the timer

      //envoi à tout les slaves d'éteindre leurs leds, reset le score et se mette en mode game
      uint8_t receivers = (1 << NBR_SLAVES)-1; // Setting 1 to all slaves
      sendCommand(CMD_START_GAME,receivers); // Telling all the slaves to enter game mode
      Serial.println ("Start command sent. Waiting for players...");
      break;
    }
    case JUST_RELEASED:{
      // the game was running, now it is stopping
      StartButton->turnOffLed();
      Serial.println( "Start Released! Stop game");
      actualState = STOPGAME;
      //envoi à tout les slaves d'éteindre leurs leds, encore afficher le score (avec gagnant?)
      uint8_t receivers = (1 << NBR_SLAVES)-1; // Setting 1 to all slaves
      sendCommand(CMD_STOP_GAME,receivers); // Telling all the slaves to enter game mode
      Serial.println ("Stop command sent. Waiting for players...");
      break;
    }
    case NOT_PRESSED:{
      // If the game is not running, they may press on the setup button to assign the modules to the players
      if(SetUpButton->state() == JUST_PRESSED){
        SetUpButton->turnOnLed();
        actualState = SETUP;
        for (uint8_t i = 0; i < NBR_SLAVES; i++){
          AllPayloadFromSlaves[i].playerId = NONE;
        }
        uint8_t receivers = (1 << NBR_SLAVES)-1; // Setting 1 to all slaves
        sendCommand(CMD_SETUP,receivers); // Telling all the slaves to enter setup mode
        Serial.println ("Setup command sent. Waiting for players...");
      }
      break;
    }
    case PRESSED:{
      break;
    }
    default:{
      actualState = STOPGAME;
      break;
    }
  }

  switch(actualState){
    case SETUP:{
      if(newPayloadReceived){
        AllPayloadFromSlaves[payloadFromSlave.slaveId] = payloadFromSlave;
      }
      //les valeurs sont atribuées en sortant du setup mode
      break;
    }
    case STOPGAME: {
      // If the game is stopped, we wait, the signal was already send to the modules
      break;
    }
    case GAMEMODE1: {


      // Determine current difficulty level based on potentiometer input
      uint8_t difficultyIndex = map(analogRead(POTENTIOMETER_DIFFICULTY_PIN), 0, 1023, 0, 2);
      DifficultyLevel currentDifficulty = static_cast<DifficultyLevel>(difficultyIndex);
      Serial.println("Difficulty level chosen:");
      Serial.println(currentDifficulty);
      long gamemode1Delay = random(
          difficultySettings[currentDifficulty].ledDelay[0],
          difficultySettings[currentDifficulty].ledDelay[1] + 1);
      
      
      currentMillis = millis();
      if(currentMillis - previousMillis >= gamemode1Delay){
        // Send the command to the slaves
        receivers = assignButtons(players, modules, 2);
        sendCommand(CMD_BUTTONS, receivers);
        previousMillis = currentMillis;
      }else if(newPayloadReceived){
        // Read the payload from the slaves
        if(payloadFromSlave.rightButtonsPressed) {
          sendScore(payloadFromSlave.playerId, SCORE_SUCCESS);
          Serial.print("✅ Player ");
          Serial.print(payloadFromSlave.playerId);
          Serial.print(" scored!");
        } else {
          sendScore(payloadFromSlave.playerId, SCORE_FAILED);
          Serial.print("❌ Player ");
          Serial.print(payloadFromSlave.playerId);
          Serial.println(" pressed the wrong button.");
        }
      }

      break;}

    case GAMEMODE2: {
      // Get current difficulty level
      uint8_t difficultyIndex = map(analogRead(POTENTIOMETER_DIFFICULTY_PIN),0,1023,0,2);
      DifficultyLevel currentDifficulty = static_cast<DifficultyLevel>(difficultyIndex);
      
      // Define variables for controlling the speed cycle and button timing
      static float speedModifier = difficultySettings[currentDifficulty].speedFactor;
      static unsigned long cycleStartTime = millis();
      static bool speedingUp = true; // Flag to toggle speeding up and slowing down

      // Get the current time
      unsigned long currentMillis = millis();

      // Calculate the delay based on difficulty
      uint16_t currentDelay = difficultySettings[currentDifficulty].ledDelay[0] * speedModifier;

      if (currentMillis - previousMillis >= currentDelay) {
        receivers = assignButtons(players, modules, 1);
        sendCommand(CMD_BUTTONS, receivers);

        Serial.println ("Button light-up command sent to slaves");
        Serial.print("Receivers bitmask: ");
        Serial.println(receivers, BIN);

        previousMillis = currentMillis;

        // Change speed
        if (speedingUp) {
          speedModifier *=0.9;
          if (speedModifier <= 0.5) speedingUp = false;
        } else {
          speedModifier *= 1.05;
          if (speedModifier >= difficultySettings[currentDifficulty].speedFactor) speedingUp = true;

        }
        
      }else if(newPayloadReceived){
        // Read the payload from the slaves
        if(payloadFromSlave.rightButtonsPressed) {
          sendScore(payloadFromSlave.playerId, SCORE_SUCCESS);
          Serial.print("✅ Player ");
          Serial.print(payloadFromSlave.playerId);
          Serial.print(" scored!");
        } else {
          sendScore(payloadFromSlave.playerId, SCORE_FAILED);
          Serial.print("❌ Player ");
          Serial.print(payloadFromSlave.playerId);
          Serial.println(" pressed the wrong button.");
        }
      }
      break;}

    case GAMEMODE3:{
      long timeToPress = 2000; // 2 seconds
      currentMillis = millis();
      if(currentMillis - previousMillis >= timeToPress){
        // Send the command to the slaves
        receivers = assignButtons(players, modules, 2);
        sendCommand(CMD_BUTTONS, receivers);
        previousMillis = currentMillis;
      }else if(newPayloadReceived){
        // Read the payload from the slaves
        if(payloadFromSlave.rightButtonsPressed) {
          sendScore(payloadFromSlave.playerId, SCORE_SUCCESS);
          Serial.print("✅ Player ");
          Serial.print(payloadFromSlave.playerId);
          Serial.print(" scored!");
        } else {
          sendScore(payloadFromSlave.playerId, SCORE_FAILED);
          Serial.print("❌ Player ");
          Serial.print(payloadFromSlave.playerId);
          Serial.println(" pressed the wrong button.");
        }
      }

      break;
    }
    
  }
  
}

void sendScore(uint8_t playerId, uint16_t score){
  PayloadFromMasterStruct payloadFromMaster;
  radio.stopListening();
  //creation du payload
  payloadFromMaster.command = CMD_SCORE;
  payloadFromMaster.buttonsToPress = 0;
  payloadFromMaster.score = score;

  uint8_t receivers = players[playerId].modules; // Get the bitmask of available modules for the player

  radio.stopListening();
  for (uint8_t slave = 0; slave < NBR_SLAVES; ++slave){
    if(receivers & (1 << slave)){
      //début de la com
      radio.openWritingPipe(addresses[0]+slave);
      unsigned long start_timer = micros();                  // start the timer
      bool report = radio.write(&payloadFromMaster, sizeof(payloadFromMaster));  // transmit & save the report
      unsigned long end_timer = micros();                    // end the timer

      // Affichage complet
      Serial.println(F("\n==========NEW TRANSMISSION=========="));
      Serial.print(F("Slave index: "));
      Serial.println(slave);
      Serial.print(F("Writing pipe address: 0x"));
      print64Hex(addresses[1]+slave);

      printPayloadFromMasterStruct(payloadFromMaster);
      if (report) {
        Serial.print(F("✅ Transmission successful in "));
        Serial.print(end_timer - start_timer);
        Serial.println(F(" µs"));
      } else {
        Serial.println(F("❌ Transmission failed"));
      }
    }
  }
  radio.startListening();  // put radio in RX mode
}

//valid for command and buttons
void sendCommand(MasterCommand command, uint8_t bitmask){
  PayloadFromMasterStruct payloadFromMaster;
  for (uint8_t slave = 0; slave < NBR_SLAVES; ++slave){
    if(bitmask & (1 << slave)){
      //creation du payload
      payloadFromMaster.command = command;
      payloadFromMaster.buttonsToPress = modules[slave].buttonsToPress;
      payloadFromMaster.score = SCORE_FAILED; // Not used in this case

      //début de la com
      radio.stopListening();
      radio.openWritingPipe(addresses[0]+slave);
      unsigned long start_timer = micros();                  // start the timer
      bool report = radio.write(&payloadFromMaster, sizeof(payloadFromMaster));  // transmit & save the report
      unsigned long end_timer = micros();                    // end the timer
      radio.startListening();  // put radio in RX mode

      // Affichage complet
      Serial.println(F("\n==========NEW TRANSMISSION=========="));
      Serial.print(F("Slave index: "));
      Serial.println(slave);
      Serial.print(F("Writing pipe address: 0x"));
      print64Hex(addresses[1]+slave);

      printPayloadFromMasterStruct(payloadFromMaster);
      if (report) {
        Serial.print(F("✅ Transmission successful in "));
        Serial.print(end_timer - start_timer);
        Serial.println(F(" µs"));
      } else {
        Serial.println(F("❌ Transmission failed"));
      }
    }
  }
}

bool readFromSlave(PayloadFromSlaveStruct& payload){
  uint8_t pipe;

  if (radio.available(&pipe)) {
    uint8_t bytes = radio.getDynamicPayloadSize();

    // Vérifie que la taille reçue correspond à celle attendue
    if (bytes == sizeof(PayloadFromSlaveStruct)) {
      radio.read(&payload, bytes);

      Serial.println(F("\n==========NEW RECEPTION=========="));
      printPayloadFromSlaveStruct(payload);

      return true;
    } else {
      Serial.println(F("Payload size mismatch. Flushing RX buffer."));
      radio.flush_rx();
    }
  }

  return false;
}

void initPlayers(PlayerStruct* players, ModuleStruct* modules){
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    players[i].modules = 0;
    players[i].nbrOfModules=0; //calcul dynamique avec modules
    players[i].score=0;
  }
  for (uint8_t i = 0; i < NBR_SLAVES; i++){
    modules[i].playerOfModule = NONE; //NONE
    modules[i].buttonsToPress = 0;
  }
}

// Function to assign random buttons to modules and players and return the receivers bitmask
uint8_t assignButtons(PlayerStruct* players, ModuleStruct* modules, uint8_t nbrButtons){
  uint8_t bitmask = 0;
  //tout éteindre
  for (uint8_t i = 0; i < NBR_SLAVES; i++){
    modules[i].buttonsToPress = 0;
  }

  //choisir quelles couleurs allumer
  Color colorsToLight[nbrButtons];
  for(uint8_t i=0; i< nbrButtons; i++){
    bool colorAttributed = false;
    Color color;
    do{
      color = static_cast<Color>(random(0, NB_COLORS)); // Randomly select a color
      colorAttributed = true; // Assume the color is not already in use
      for (uint8_t j = 0; j < i; j++){
        if (colorsToLight[j] == color){ // Check if the color is already in use
          colorAttributed = false; // If it is, set the flag to false and break the loop
          break;

        }
      }
    } while (!colorAttributed); // Keep trying until a color is found that is not already in the array
    colorsToLight[i]=color;
  }

  // Assign a module to each color for players with modules
  if (nbrButtons > 0 && nbrButtons <= NB_COLORS) {
    for (uint8_t player = 0; player < NB_COLORS; player++) {
      if (players[player].nbrOfModules > 0) {  // Only process players who have modules
        for (uint8_t button = 0; button < nbrButtons; button++) {
          // Find all available modules for this player
          uint8_t availableModules = players[player].modules;  // Get the bitmask of available modules
          uint8_t validModuleCount = 0;  // To count the valid modules
          
          // Count the number of modules available for the player
          for (uint8_t i = 0; i < NBR_SLAVES; i++) {
            if ((availableModules & (1 << i)) != 0) {  // Check if module i is available for the player
              validModuleCount++;
            }
          }

          // If there are valid modules, select one randomly
          if (validModuleCount > 0) {
            uint8_t randomModuleIndex = random(0, validModuleCount);  // Randomly select a module index
            uint8_t selectedModule = 0;  // This will hold the selected module id

            // Iterate through the available modules and find the randomly selected one
            for (uint8_t i = 0; i < NBR_SLAVES; i++) {
              if ((availableModules & (1 << i)) != 0) {  // If the module is available for the player
                if (randomModuleIndex == 0) {
                  selectedModule = i;  // Found the randomly selected module
                  break;
                }
                randomModuleIndex--;  // Decrement the counter
              }
            }

            // Now that we have selected a module, assign the button to it
            modules[selectedModule].buttonsToPress |= (1 << colorsToLight[button]);  // Set the bit for this button in the module's buttonsToPress bitmask
            bitmask |= (1 << selectedModule);  // Set the bit for this player in the receivers bitmask (only if they have modules)
          }
        }
      }
    }
  } else {
    bitmask = (1 << NBR_SLAVES) - 1;  // If no buttons, set all players as receivers
  }

  Serial.println(bitmask,BIN);  // Print the receivers bitmask for debugging
  return bitmask;  // Return the receivers bitmask
}

void assignModules(PlayerStruct* players, ModuleStruct* modules, PayloadFromSlaveStruct* AllPayloadFromSlaves){
  // Let's clear previous assignments
  initPlayers(players, modules);

  // Assign modules to players
  for (uint8_t i = 0; i< NBR_SLAVES; i++){
    Player idPlayer = AllPayloadFromSlaves[i].playerId;
    if (idPlayer != NONE && idPlayer < MAX_PLAYERS){
      modules[i].playerOfModule = idPlayer;
      players[idPlayer].modules |= (1 << i); //Bitmask update.
      players[idPlayer].nbrOfModules++;
    }
  }

  // Debug print
  Serial.println(F("\n==========ASSIGNMENTS=========="));
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    Serial.print(F("Player "));
    Serial.print(i);
    Serial.print(F(" has modules: 0b"));
    Serial.println(players[i].modules, BIN);
    Serial.print(F("Number of modules: "));
    Serial.println(players[i].nbrOfModules);
  }
}