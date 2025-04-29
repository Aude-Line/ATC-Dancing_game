#include <Arduino.h>
#include <Adafruit_AW9523.h>
#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#include <master_pins.h>
#include <util.h>
#include <communication.h>
#include <button.h>

enum State{SETUP, STOPGAME, GAMEMODE1, GAMEMODE2};
enum DifficultyLevel {
  EASY,
  MEDIUM,
  HARD
};

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
  uint16_t pressTime;          // Time allowed to press the button
};

Difficulty difficultySettings[] {
  {{5000, 6000}, 5000}, // EASY
  {{4000, 5000}, 3000}, // MEDIUM
  {{3000, 4000}, 1000} // HARD
};

void initPlayers(PlayerStruct* players, ModuleStruct* modules);
void sendMessage(MasterCommand command, uint8_t receivers);
bool readFromSlave(PayloadFromSlaveStruct& payload);
uint8_t assignButtons(PlayerStruct* players, ModuleStruct* modules, uint8_t nbrButtons=0);

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);
unsigned long lastSendTime = 0;

State actualState = STOPGAME; // Added a similar state as in slave to switch 
Button* StartButton; // Defined the start and setup button and states
Button* SetUpButton;
PayloadFromSlaveStruct AllPayloadFromSlave[NBR_SLAVES];
uint8_t fromSlaveID[NBR_SLAVES];


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
  PayloadFromSlaveStruct payloadFromSlave;
  bool newPayloadReceived = readFromSlave(payloadFromSlave); // update the payloads from slaves, récuperer le payload

  unsigned long currentTime = millis();   // Récupère le temps actuel
  // Vérifier si 2 secondes se sont écoulées
  if (currentTime - lastSendTime >= 2000) {  
    // Mettre à jour le dernier temps d'envoi
    lastSendTime = currentTime;

    // Générer une commande aléatoire entre 0 et 4 (selon les valeurs possibles de l'énum)
    //MasterCommand command = static_cast<MasterCommand>(random(0, 1));  // Random entre 0 et 4 (STOP_GAME à MISSED_BUTTONS)
    MasterCommand command = CMD_SETUP;  // Pour tester, on envoie toujours la même commande
    // Générer un masque de récepteurs aléatoire
    uint8_t receivers = random(0, (1 << NBR_SLAVES));  // Masque binaire avec NBR_SLAVES bits

    Serial.println(F("\n==========VALS COMMAND AND RECEIVERS=========="));
    // Afficher les valeurs dans le terminal pour vérifier
    Serial.print(F("Random command: "));
    Serial.println(command);
    Serial.print(F("Random receivers mask: "));
    Serial.println(receivers, BIN);  // Affiche en binaire

    // Appeler la fonction d'envoi avec la commande et les récepteurs générés
    sendMessage(command, receivers);
  }

  // The actual game logic
  if (SetUpButton->isPressed()){
    // If the setupbutton has been pressed enter the setupmode
    actualState = SETUP;
    uint8_t receivers = (1 << NBR_SLAVES)-1; // Setting 1 to all slaves
    sendMessage(CMD_SETUP,receivers); // Telling all the slaves to enter setup mode
    Serial.println ("Setup command sent. Waiting for players...");
  }

  switch(actualState){
    case SETUP:{
      if(newPayloadReceived){
        AllPayloadFromSlave[payloadFromSlave.slaveId] = payloadFromSlave;
      }
      //si payload appeler fonction assignModules

      // Wait for start button to move forward
      if (StartButton ->isPressed()){
        Serial.println( "Start Pressed! Assigning modules");
        uint8_t gameMode = map(analogRead(POTENTIOMETER_MODE_PIN), 0, 1023, 1, 2);
        actualState = static_cast<State>(STOPGAME + gameMode); // Set the game mode based on the potentiometer value
        
        // Let's clear previous assignments
        initPlayers(players, modules);

        // Assign modules to players
        for (uint8_t i = 0; i< NBR_SLAVES; i++){
          Player idPlayer = AllPayloadFromSlave[i].playerId;
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
        break;
      }
      break;

    }
    case STOPGAME: {}
    case GAMEMODE1: {
      static bool gamemode1CommandSent = false;
      unsigned long currentMillis = millis();
      uint16_t gamemode1Delay = 0;
      static unsigned long gamemode1StartTime = 0;
      uint8_t difficultyIndex =map(analogRead(POTENTIOMETER_DIFFICULTY_PIN), 0, 1023, 0, 2);
      DifficultyLevel currentDifficulty = static_cast<DifficultyLevel>(difficultyIndex);


      if (!gamemode1CommandSent){
        // Reset Receivers
        gamemode1Delay= random (difficultySettings[currentDifficulty].ledDelay[0],difficultySettings[currentDifficulty].ledDelay[1]+1);
        gamemode1StartTime = currentMillis;
        gamemode1CommandSent = true;
        Serial.print("Scheduled GAMEMODE1 command in ");
        Serial.print(gamemode1Delay);
        Serial.println(" ms");
      } else if (currentMillis - gamemode1StartTime >= gamemode1Delay) {
        // Time to send command
        uint8_t receivers = 0;
        receivers = assignButtons(players, modules,1);
        sendMessage(CMD_BUTTONS, receivers);
        
        Serial.println("GAMEMODE1 command sent to randomly selected modules.");
        Serial.print("Receivers bitmask: ");
        Serial.println(receivers, BIN);
        gamemode1CommandSent = false; // Reset for the next cycle
      }
      break;
    }
    case GAMEMODE2: {}
    default: {}
  }
}

void sendMessage(MasterCommand command, uint8_t receivers){
  PayloadFromMasterStruct payloadFromMaster;
  radio.stopListening();
  for (uint8_t slave = 0; slave < NBR_SLAVES; ++slave){
    if(receivers & (1 << slave)){
      //creation du payload
      payloadFromMaster.command = command;
      payloadFromMaster.buttonsToPress = modules[slave].buttonsToPress;
      if(modules[slave].playerOfModule != NONE){
        payloadFromMaster.score = players[modules[slave].playerOfModule].score;
      }else{
        payloadFromMaster.score = 0;
      }

      /*
      // Création d'un payload aléatoire pour tester
      payloadFromMaster.command = command;
      payloadFromMaster.buttonsToPress = random(0, 16);  // Valeur aléatoire entre 0 et 15 pour les 4 boutons
      payloadFromMaster.score = random(0, 2);
      */


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

bool readFromSlave(PayloadFromSlaveStruct& payload) {
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
    modules[i].playerOfModule = -1; //NONE
    modules[i].buttonsToPress = 0;
  }
}

// Function to assign random buttons to modules and players and return the receivers bitmask
uint8_t assignButtons(PlayerStruct* players, ModuleStruct* modules, uint8_t nbrButtons=0){
  uint8_t receivers = 0;
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

  //atribuer un module à chaque couleur
  if(nbrButtons > 0 and nbrButtons <= NB_COLORS){
    for(uint8_t player = 0; player <NB_COLORS; player++){
      for(uint8_t button = 0; button < nbrButtons; button++){
        uint8_t randomModule = random(0, players[player].nbrOfModules);
        uint8_t idModule = 0;
        while(randomModule > -1){
          while((players[player].modules & (1 << idModule)) == 0){
            idModule++;
          }
          randomModule--;
        }
        modules[idModule].buttonsToPress |= (1 << colorsToLight[button]); // Set the bit for this button in the module's buttonsToPress bitmask
        receivers |= (1 << idModule); // Set the bit for this module in the receivers bitmask
      }
    }
  }else{
    receivers = (1 << NBR_SLAVES)-1; // Setting 1 to all slaves
  }

  return receivers; // Return the receivers bitmask
}