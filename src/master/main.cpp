#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>  // add the FreeRTOS functions for Semaphores (or Flags).
#include <task.h>    // add the FreeRTOS functions for Tasks
//#include <Adafruit_AW9523.h>
#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#include <master_pins.h>
#include <util.h>
#include <communication.h>
#include <button.h>
#include <mp3.h>

#define PERIOD_BUTTON 100 // 100ms
#define PERIOD_COMM 100 // 100ms
#define DEFAULT_PLAY_TIME 2000 // 2s
#define ALL_MODULES ((1 << NBR_SLAVES)-1) // Setting 1 to all slaves

// the possible state of the master, the modes should be just after the STOPGAME
enum State{SETUP, STOPGAME, GAMEMODE1, GAMEMODE2, GAMEMODE3, NBR_GAMEMODES}; // Added different game modes as states

struct PlayerStruct{
  uint8_t modules; //mask
  uint8_t nbrOfModules; //calcul dynamique avec modules
  uint16_t score;
};
struct ModuleStruct{
  Player playerOfModule;
  uint8_t buttonsToPress; //mask
  bool rightButtonsPressed; //en mode le/les bons boutons qui devaient être appuyés ont tous été appuyés
};
PlayerStruct players[MAX_PLAYERS];
ModuleStruct modules[NBR_SLAVES];

void initPlayers(PlayerStruct* players, ModuleStruct* modules);
void sendCommand(MasterCommand command, uint8_t receivers);
void sendScore(uint8_t playerId, uint16_t score);
bool readFromSlave(PayloadFromSlaveStruct& payload);
uint8_t assignButtons(PlayerStruct* players, ModuleStruct* modules, uint8_t nbrButtons);

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);

Button* StartButton; // Defined the start and setup button and states
Button* SetUpButton;
MP3Module* mp3Module;
State actualState = STOPGAME;

//semaphore for serial communication
SemaphoreHandle_t xSerialSemaphore;
//semaphore for radio communication
SemaphoreHandle_t xRadioSemaphore;

// define the tasks
void TaskHandleButtons(void *pvParameters);
void TaskReadFromSlaves(void *pvParameters);
void TaskSendButtons(void *pvParameters);


void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and other 32u4 based boards.
  }

  // Crée des mutex (sémaphores binaires) pour Serial et Radio
  if ( xSerialSemaphore == NULL ){  // Check to confirm that the Serial Semaphore has not already been created.
    xSerialSemaphore = xSemaphoreCreateMutex();  // Create a mutex semaphore we will use to manage the Serial Port
    if ( ( xSerialSemaphore ) != NULL )
      xSemaphoreGive( ( xSerialSemaphore ) );  // Make the Serial Port available for use, by "Giving" the Semaphore.
  }
  if ( xRadioSemaphore == NULL ){  // Check to confirm that the Radio Semaphore has not already been created.
    xRadioSemaphore = xSemaphoreCreateMutex();  // Create a mutex semaphore we will use to manage the Radio
    if ( ( xRadioSemaphore ) != NULL )
      xSemaphoreGive( ( xRadioSemaphore ) );  // Make the Radio available for use, by "Giving" the Semaphore.
  }

  // Inizialization of buttons and potentiometers
  StartButton = new Button(START_BUTTON_PIN, START_LED_PIN);
  SetUpButton = new Button (SETUP_BUTTON_PIN, SETUP_LED_PIN);

  //Initalize the mp3 player
  mp3Module = new MP3Module(MP3_RX_PIN, MP3_TX_PIN);

  //Initialize the potentiometers
  pinMode(POTENTIOMETER_MODE_PIN, INPUT);
  pinMode(POTENTIOMETER_DIFFICULTY_PIN, INPUT);

  // initialize the transceiver on the SPI bus
  if (!radio.begin()) {
    if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("radio hardware is not responding!!"));
      xSemaphoreGive(xSerialSemaphore);
    }
  }
  // Initialize the radio
  radio.setChannel(125);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setRetries(5, 15);
  radio.setAutoAck(true);
  radio.enableDynamicPayloads(); //as we have different payload size
  for (uint8_t i = 0; i < NBR_SLAVES; ++i){
    radio.openReadingPipe(i+1, addresses[1]+i); //use pipes 1-5 for reception, pipe 0 is reserved for transmission in this code
  }
  radio.startListening();  // put radio in RX mode
  if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    Serial.print(F("Addresses to receive: "));
    for (uint8_t i = 0; i < NBR_SLAVES; ++i){
      Serial.print(F("  Pipe "));
      Serial.print(i + 1);
      Serial.print(F(" → Adresse: 0x"));
      print64Hex(addresses[1]+i);  // affiche l'adresse en hexadécimal
    }
    xSemaphoreGive(xSerialSemaphore);
  }

  initPlayers(players, modules);

  if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    Serial.print(F("Je suis le master"));
    xSemaphoreGive(xSerialSemaphore);
  }

  // Create the tasks
  // The tasks are created in the setup function, and they will run in parallel
  if (xTaskCreate(TaskHandleButtons, "TaskHandleButtons", 128, NULL, 3, NULL) != pdPASS) {
    if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("Erreur : création tâche échouée !"));
      xSemaphoreGive(xSerialSemaphore);
    }
  }

  if (xTaskCreate(TaskReadFromSlaves, "TaskReadFromSlaves", 128, NULL, 1, NULL) != pdPASS) {
    if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("Erreur : création tâche échouée !"));
      xSemaphoreGive(xSerialSemaphore);
    }
  }

  if (xTaskCreate(TaskSendButtons, "TaskSendButtons", 128, NULL, 1, NULL) != pdPASS) {
    if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("Erreur : création tâche échouée !"));
      xSemaphoreGive(xSerialSemaphore);
    }
  }

  vTaskStartScheduler();
}

void loop() {
  // Nothing to do here, all the work is done in the tasks
}

/*--------------------------------------------------------------------
  ----------------------------TASKS-----------------------------------
  --------------------------------------------------------------------*/

void TaskHandleButtons(void *pvParameters) {
  (void) pvParameters; // suppress unused parameter warning
  if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    Serial.println(F("Tache boutons lancée"));
    xSemaphoreGive(xSerialSemaphore);
  }

  //DEBUG savoir combien j'utilise de stack
  UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
  if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {
    Serial.print(F("Stack remaining (TaskHandleButtons): "));
    Serial.println(watermark);
    xSemaphoreGive(xSerialSemaphore);
  }

  for (;;) {
    // update the state of the buttons
    StartButton->updateState();
    SetUpButton->updateState();

    // Change the state of the module based on the state of the buttons
    switch(StartButton->getState()){
      case JUST_PRESSED: { // Start the game
        SetUpButton->turnOffLed();
        StartButton->turnOnLed();

        if(!isAtLeastOnePlayerPresent()){
          if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
            Serial.println(F("Pick a player first!!!"));
            xSemaphoreGive(xSerialSemaphore);
          }
          actualState = STOPGAME;
          sendCommand(CMD_STOP_GAME, ALL_MODULES); // Telling all the slaves to enter game mode
        }else{
          // Set the game mode based on the potentiometer value to be able to do it easily without needing to reasign the modules
          uint8_t gameMode = readGameModeFromPot();
          if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
            Serial.print(F("We are in game mode: "));
            Serial.println(gameMode);
            Serial.print(F("Modules of players: "));
            for (uint8_t i = 0; i < MAX_PLAYERS; ++i) {
              Serial.print(players[i].modules, BIN);
              Serial.print(F(" "));
            }
            xSemaphoreGive(xSerialSemaphore);
          }
          gameMode = 1; // TODO: remove this line, for testing purposes only
          actualState = static_cast<State>(STOPGAME + gameMode);
          //init scores
          for (uint8_t i = 0; i < MAX_PLAYERS; ++i) {
            players[i].score = 0;
          }
          sendCommand(CMD_START_GAME, ALL_MODULES); // Telling all the slaves to enter game mode
        }

        break;
      }
      case JUST_RELEASED: { //Pause the game
        StartButton->turnOffLed();
        actualState = STOPGAME;
        sendCommand(CMD_STOP_GAME, ALL_MODULES); // Telling all the slaves to enter game mode
        if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
            Serial.print(F("Game paused"));
            xSemaphoreGive(xSerialSemaphore);
        }
        break;
      }
      case NOT_PRESSED:{
        // If the game is not running, they may press on the setup button to assign the modules to the players
        if(SetUpButton->getState() == JUST_PRESSED){
          SetUpButton->turnOnLed();
          actualState = SETUP;
          initPlayers(players, modules); //reset the players and modules
          sendCommand(CMD_SETUP, ALL_MODULES); // Telling all the slaves to enter setup mode
          if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
            Serial.print(F("Enter setup mode"));
            xSemaphoreGive(xSerialSemaphore);
          }
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

    vTaskDelay(PERIOD_BUTTON / portTICK_PERIOD_MS);
  }
}

void TaskReadFromSlaves(void *pvParameters) {
  (void) pvParameters; // suppress unused parameter warning
  if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    Serial.println(F("Tache lecture lancée"));
    xSemaphoreGive(xSerialSemaphore);
  }

  PayloadFromSlaveStruct payload;
  //DEBUG savoir combien j'utilise de stack
  UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
  if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {
    Serial.print(F("Stack remaining (TaskReadFromSlaves): "));
    Serial.println(watermark);
    xSemaphoreGive(xSerialSemaphore);
  }

  for (;;) {
    if (getPayloadFromSlaves(payload)) {
      handlePayloadFromSlave(payload);
    }
    vTaskDelay(PERIOD_COMM / portTICK_PERIOD_MS);
  }
}

/*--------------------------------------------------------------------
  ------------------------------FUNCTIONS-----------------------------
  --------------------------------------------------------------------*/

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

bool isAtLeastOnePlayerPresent() {
  for (uint8_t module = 0; module < NBR_SLAVES; ++module) {
    if (modules[module].playerOfModule != NONE) {
      return true;
    }
  }
  return false;
}

void assignPlayerToModule(const PayloadFromSlaveStruct& payload) {
  Player lastPlayer = modules[payload.slaveId].playerOfModule;
  Player newPlayer = payload.playerId;
  modules[payload.slaveId].playerOfModule = newPlayer;
  if(lastPlayer != NONE) {
    // If the module was already assigned to a player, remove it from the previous player
    players[lastPlayer].modules &= ~(1 << payload.slaveId); // Remove the module from the previous player
    players[lastPlayer].nbrOfModules--;
  }
  if(newPlayer != NONE) {
    // Assign the module to the new player
    players[newPlayer].modules |= (1 << payload.slaveId); // Add the module to the new player
    players[newPlayer].nbrOfModules++;
  }
}

uint8_t readGameModeFromPot() {
  const uint8_t FIRST_GAMEMODE = GAMEMODE1;
  const uint8_t LAST_GAMEMODE = NBR_GAMEMODES - 1;

  uint8_t mode = map(analogRead(POTENTIOMETER_MODE_PIN), 0, 1023, FIRST_GAMEMODE, LAST_GAMEMODE);
  return constrain(mode, FIRST_GAMEMODE, LAST_GAMEMODE);
}

bool sendPayloadToSlave(PayloadFromMasterStruct& payload, uint8_t slave){
  bool report = false;
  unsigned long start_timer = 0;
  unsigned long end_timer = 0;

  if(xSemaphoreTake(xRadioSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    radio.stopListening();
    radio.openWritingPipe(addresses[0]+slave);
    start_timer = micros();                  // start the timer
    report = radio.write(&payload, sizeof(payload));  // transmit & save the report
    end_timer = micros();                    // end the timer
    radio.startListening();  // put radio in RX mode
    xSemaphoreGive(xRadioSemaphore);
  }
  
  // Print the payload on the serial monitor
  // Affichage complet
  if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    Serial.println(F("\n==========NEW TRANSMISSION=========="));
    Serial.print(F("Slave index: "));
    Serial.println(slave);
    Serial.print(F("Writing pipe address: 0x"));
    print64Hex(addresses[1]+slave);
    printPayloadFromMasterStruct(payload);
    if (report) {
      Serial.print(F("✅ Transmission successful in "));
      Serial.print(end_timer - start_timer);
      Serial.println(F(" µs"));
    } else {
      Serial.println(F("❌ Transmission failed"));
    }
    xSemaphoreGive(xSerialSemaphore);
  }
}

void sendCommand(MasterCommand command, uint8_t receiversBitmask){
  PayloadFromMasterStruct payloadFromMaster;
  payloadFromMaster.command = command;
  payloadFromMaster.buttonsToPress = 0; // Not used in this case
  payloadFromMaster.score = SCORE_FAILED; // Not used in this case
  for (uint8_t slave = 0; slave < NBR_SLAVES; ++slave){
    if(receiversBitmask & (1 << slave)){
      sendPayloadToSlave(payloadFromMaster, slave);
    }
  }
}

void sendScore(uint8_t playerId, uint16_t score){
  PayloadFromMasterStruct payloadFromMaster;
  payloadFromMaster.command = CMD_SCORE;
  payloadFromMaster.buttonsToPress = 0;
  payloadFromMaster.score = score; //TODO peutêtre modifier pour envoyer le score exact du joueur

  uint8_t receiversBitmask = players[playerId].modules; // Get the bitmask of available modules for the player
  for (uint8_t slave = 0; slave < NBR_SLAVES; ++slave){
    if(receiversBitmask & (1 << slave)){
      sendPayloadToSlave(payloadFromMaster, slave);   
    }
  }
}

void sendScoreToSlave(uint8_t slave, uint16_t score){
  PayloadFromMasterStruct payloadFromMaster;
  payloadFromMaster.command = CMD_SCORE;
  payloadFromMaster.buttonsToPress = 0;
  payloadFromMaster.score = score; //TODO peutêtre modifier pour envoyer le score exact du joueur

  sendPayloadToSlave(payloadFromMaster, slave);
}

bool getPayloadFromSlaves(PayloadFromSlaveStruct& payload){
  uint8_t pipe;
  bool newMessage = false;

  if (xSemaphoreTake(xRadioSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    if (radio.available(&pipe)) {
      uint8_t bytes = radio.getDynamicPayloadSize();

      // Vérifie que la taille reçue correspond à celle attendue
      if (bytes == sizeof(PayloadFromSlaveStruct)) {
        radio.read(&payload, bytes);
        newMessage = true; // Set the flag to true if a new message is received
      } else {
        // If the size does not match, flush the RX buffer
        if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
          Serial.println(F("Payload size mismatch. Flushing RX buffer."));
          xSemaphoreGive(xSerialSemaphore);
        }
        radio.flush_rx();
      }
    }
    xSemaphoreGive(xRadioSemaphore);
  }

  //Print the payload on the serial monitor
  // This is not necessary in the final version, but can be useful for debugging
  if(newMessage) {  // timeout 10 ticks
    if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {
      Serial.println(F("\n==========NEW RECEPTION=========="));
      Serial.print(F("Reading pipe index: "));
      Serial.println(pipe);
      Serial.println(F("✅ New message received"));
      printPayloadFromSlaveStruct(payload);
      xSemaphoreGive(xSerialSemaphore);
    }
  }

  return newMessage; // Return true if a new message was received
}

void handlePayloadFromSlave(const PayloadFromSlaveStruct& payload) {
  switch (actualState) {
    case SETUP: {
      assignPlayerToModule(payload);
      break;
    }
    case STOPGAME: {
      // Do nothing
      break;
    }
    case GAMEMODE1: { // if the right button is pressed, the player scores
      if(payload.buttonsPressed==RIGHT_BUTTONS_PRESSED) {
        sendScore(payload.playerId, SCORE_SUCCESS);
        players[payload.playerId].score++;
      } else {
        sendScore(payload.playerId, SCORE_FAILED);
      }
      break;
    }
    case GAMEMODE2: { // Only the first player scores
      if(payload.buttonsPressed==RIGHT_BUTTONS_PRESSED) {
        sendScore(payload.playerId, SCORE_SUCCESS);
        players[payload.playerId].score++;
        for(uint8_t module = 0; module < NB_COLORS; ++module) {
          if (modules[module].playerOfModule != payload.playerId && modules[module].playerOfModule != NONE) {
            sendScoreToSlave(module, SCORE_FAILED);
            break;
          }
        }
      }
    }
    case GAMEMODE3: { // 2 buttons, All players score
      if(!payload.buttonsPressed){
        sendScore(payload.playerId, SCORE_FAILED);
        Serial.print("❌ Player ");
        Serial.print(payload.playerId);
        Serial.println(" pressed the wrong button.");
      }else{
        modules[payload.slaveId].rightButtonsPressed = true; // Set the flag to true
      }
      break;
    }
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