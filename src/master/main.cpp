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

#define PERIOD_BUTTON 500 // 100ms
#define PERIOD_COMM 500 // 100ms
#define DEFAULT_PLAY_TIME 2000 // 2s
#define MIN_PLAY_TIME 500 // 1s
#define MAX_PLAY_TIME 8000 // 5s
#define ALL_MODULES ((1 << NBR_SLAVES)-1) // Setting 1 to all slaves

// the possible state of the master, the modes should be just after the STOPGAME
enum State : uint8_t{SETUP, STOPGAME, GAMEMODE1, GAMEMODE2, GAMEMODE3, GAMEMODE4, NBR_GAMEMODES}; // Added different game modes as states
const uint8_t FIRST_GAMEMODE = GAMEMODE1 - STOPGAME;
const uint8_t LAST_GAMEMODE = NBR_GAMEMODES - STOPGAME - 1;

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
uint8_t modulesUsed = 0; //used to know which modules are used

void initPlayers(PlayerStruct* players, ModuleStruct* modules);
bool isAtLeastOnePlayerPresent();
void assignPlayerToModule(const PayloadFromSlaveStruct& payload);
uint8_t readGameModeFromPot();
uint16_t readNormalSpeedFromPot();
bool sendPayloadToSlave(PayloadFromMasterStruct& payload, uint8_t slave);
void sendCommand(MasterCommand command, uint8_t receiversBitmask);
void sendScore(uint8_t playerId, uint16_t score);
void sendScoreToSlave(uint8_t slave, uint16_t score);
bool getPayloadFromSlaves(PayloadFromSlaveStruct& payload);
void handlePayloadFromSlave(const PayloadFromSlaveStruct& payload);
void addRandomColor(uint8_t &existingColors);
int8_t getRandomModule(uint8_t nbModules, uint8_t mask);
void assignColorsToPlayer(uint8_t nbColors, bool fixedColors = true);

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);

Button StartButton(START_BUTTON_PIN, START_LED_PIN);
Button SetUpButton(SETUP_BUTTON_PIN, SETUP_LED_PIN);
State actualState = STOPGAME;

//semaphore for serial communication
SemaphoreHandle_t xSerialSemaphore;
//semaphore for radio communication
SemaphoreHandle_t xRadioSemaphore;

// define the tasks
void TaskHandleButtons(void *pvParameters);
void TaskReadFromSlaves(void *pvParameters);
void TaskAssignButtons(void *pvParameters);


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

  //Initalize the mp3 player
  //mp3Module = new MP3Module(MP3_RX_PIN, MP3_TX_PIN);

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
    Serial.println(F("Je suis le master"));
    xSemaphoreGive(xSerialSemaphore);
  }

  // Create the tasks
  // The tasks are created in the setup function, and they will run in parallel
  if (xTaskCreate(TaskHandleButtons, "TaskHandleButtons", 160, NULL, 3, NULL) != pdPASS) {
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
  /*
  if (xTaskCreate(TaskAssignButtons, "TaskAssignButtons", 256, NULL, 1, NULL) != pdPASS) {
    if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("Erreur : création tâche échouée !"));
      xSemaphoreGive(xSerialSemaphore);
    }
  }
    */

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
    StartButton.updateState();
    SetUpButton.updateState();

    // Change the state of the module based on the state of the buttons
    switch(StartButton.getState()){
      case JUST_PRESSED: { // Start the game
        SetUpButton.turnOffLed();
        StartButton.turnOnLed();

        if(!isAtLeastOnePlayerPresent()){
          if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
            Serial.println(F("Pick a player first!!!"));
            Serial.print(F("We should be in game mode: "));
            Serial.println(readGameModeFromPot());
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
          gameMode = GAMEMODE1; // TODO: remove this line, for testing purposes only
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
        StartButton.turnOffLed();
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
        if(SetUpButton.getState() == JUST_PRESSED){
          SetUpButton.turnOnLed();
          actualState = SETUP;
          initPlayers(players, modules); //reset the players and modules
          sendCommand(CMD_SETUP, ALL_MODULES); // Telling all the slaves to enter setup mode
          if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
            Serial.println(F("Enter setup mode"));
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
    if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {
      UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
      Serial.print(F("[TaskHandleButtons] Stack remaining: "));
      Serial.println(watermark);
      xSemaphoreGive(xSerialSemaphore);
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

void TaskAssignButtons(void *pvParameters) {
  (void) pvParameters; // suppress unused parameter warning
  if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    Serial.println(F("Tache assignation boutons lancée"));
    xSemaphoreGive(xSerialSemaphore);
  }

  PayloadFromSlaveStruct payload;
  uint16_t waitTime = DEFAULT_PLAY_TIME; // 2s (temps d'attente par défaut)
  //DEBUG savoir combien j'utilise de stack
  if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {
    UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
    Serial.print(F("Stack remaining (TaskAssignButtons): "));
    Serial.println(watermark);
    xSemaphoreGive(xSerialSemaphore);
  }

  for(;;){
    if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {
      UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
      Serial.print(F("Stack remaining (TaskAssignButtons): "));
      Serial.println(watermark);
      Serial.println(F("Iteration boutons"));
      Serial.print(F("Speed: "));
      Serial.println(waitTime);
      xSemaphoreGive(xSerialSemaphore);
    }
    if(actualState == GAMEMODE1 || actualState == GAMEMODE2 || actualState == GAMEMODE3){
      waitTime = readNormalSpeedFromPot(); // attente avant la prochaine itération
      //TODO ajouter la difficulté
      assignColorsToPlayer(1);
    }else if(actualState == GAMEMODE4){
      waitTime = readNormalSpeedFromPot(); // attente avant la prochaine itération
      //TODO ajouter la difficulté
      assignColorsToPlayer(2);
    }else{
      waitTime = DEFAULT_PLAY_TIME; // attente avant la prochaine itération
    }
    vTaskDelay(waitTime /portTICK_PERIOD_MS);
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
    modules[i].rightButtonsPressed = false; //en mode le/les bons boutons qui devaient être appuyés ont tous été appuyés
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
    modulesUsed |= (1 << payload.slaveId); // Mark the module as used
    players[newPlayer].modules |= (1 << payload.slaveId); // Add the module to the new player
    players[newPlayer].nbrOfModules++;
  }else{
    // If the module is not assigned to any player, remove it from the used modules
    modulesUsed &= ~(1 << payload.slaveId); // Mark the module as unused
  }
}

uint8_t readGameModeFromPot() {
  uint8_t mode = map(analogRead(POTENTIOMETER_MODE_PIN), 0, 1023, FIRST_GAMEMODE, LAST_GAMEMODE);
  return constrain(mode, FIRST_GAMEMODE, LAST_GAMEMODE);
}

uint16_t readNormalSpeedFromPot() {
  uint16_t speed = map(analogRead(POTENTIOMETER_NORMAL_SPEED_PIN), 0, 1023, MIN_PLAY_TIME, MAX_PLAY_TIME);
  return constrain(speed, MIN_PLAY_TIME, MAX_PLAY_TIME);
}

bool sendPayloadToSlave(PayloadFromMasterStruct& payload, uint8_t slave){
  bool report = false;
  unsigned long start_timer = 0;
  unsigned long end_timer = 0;

  if(xSemaphoreTake(xRadioSemaphore, (TickType_t)20) == pdTRUE) {  // timeout 10 ticks
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
  return report;
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
  uint8_t pipe = 0;
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
      }else if(payload.buttonsPressed==WRONG_BUTTONS_PRESSED) {
        sendScore(payload.playerId, SCORE_FAILED);
      }
      break;
    }
    case GAMEMODE2: { // Only the first player scores
      if(payload.buttonsPressed==WRONG_BUTTONS_PRESSED) {
        sendScore(payload.playerId, SCORE_FAILED);
        break;
      }
      if(payload.buttonsPressed==RIGHT_BUTTONS_PRESSED) {
        sendScore(payload.playerId, SCORE_SUCCESS);
        players[payload.playerId].score++;
        for(uint8_t module = 0; module < NBR_SLAVES; ++module) {
          if (modules[module].playerOfModule != payload.playerId && modules[module].playerOfModule != NONE) {
            sendScoreToSlave(module, SCORE_FAILED);
          }
        }
      }
      break;
    }
    case GAMEMODE3: { // all players need to press their buttons (collaborative game)
      //TODO wrong buttons are not taken into account
      if(payload.buttonsPressed==RIGHT_BUTTONS_PRESSED) {
        modules[payload.slaveId].rightButtonsPressed = true;
      }else{
        modules[payload.slaveId].rightButtonsPressed = false;
      }

      // Check if all the modules have been pressed
      bool allRight = true;
      for(uint8_t module = 0; module < NBR_SLAVES; ++module) {
        if(modules[module].rightButtonsPressed == false && modules[module].playerOfModule != NONE) {
          allRight = false;
        }
      }
      if(allRight) {
        for(uint8_t player = 0; player < NB_COLORS; ++player) {
          if(players[player].nbrOfModules > 0) {
            sendScore(player, SCORE_SUCCESS);
            players[player].score++;
          }
        }
      }
      break;
    }
    case GAMEMODE4: { // 2 buttons, may be on different modules
      if(payload.buttonsPressed==WRONG_BUTTONS_PRESSED) {
        sendScore(payload.playerId, SCORE_FAILED);
        break;
      }else if(payload.buttonsPressed==RIGHT_BUTTONS_PRESSED) {
        modules[payload.slaveId].rightButtonsPressed = true;
      }else{
        modules[payload.slaveId].rightButtonsPressed = false;
      }

      // Check if all the modules of the player have been pressed
      bool allRight = true;
      for(uint8_t module = 0; module < NBR_SLAVES; ++module) {
        if(modules[module].playerOfModule == payload.playerId) {
          if(modules[module].rightButtonsPressed == false) {
            allRight = false;
          }
        }
      }
      if(allRight){
        sendScore(payload.playerId, SCORE_SUCCESS);
        players[payload.playerId].score++;
      }
      break;
    }
    default: {
      // Do nothing
      break;
    }
  }
}

void addRandomColor(uint8_t &existingColors) {
  // Compter combien de couleurs sont déjà utilisées
  uint8_t count = 0;
  for (uint8_t i = 0; i < NB_COLORS; i++) {
    if (existingColors & (1 << i)) {
        count++;
    }
  }
  // Si toutes les couleurs sont déjà utilisées, ne rien faire
  if (count >= NB_COLORS) return;

  // Boucle jusqu'à trouver une couleur pas encore utilisée
  while (true) {
    uint8_t randColor = random(0, NB_COLORS);
    if ((existingColors & (1 << randColor)) == 0) {
        existingColors |= (1 << randColor);
        break;
    }
  }
}

//Faire une fonction qui choisi aléatoirement un module d'un masque
int8_t getRandomModule(uint8_t nbModules, uint8_t mask) {
  if (nbModules == 0) return -1; // No module available

  uint8_t randIndex = random(0, nbModules);
  for (int8_t i = 0; i < NBR_SLAVES; i++) {
    if (mask & (1 << i)) {
      if (randIndex == 0) {
        return i;
      }
      randIndex--;
    }
  }
  return -1; // Should never reach here
}

void assignColorsToPlayer(uint8_t nbColors, bool fixedColors) {
  //réinitialiser les modules
  for(uint8_t module = 0; module < NBR_SLAVES; ++module) {
    modules[module].buttonsToPress = 0;
    modules[module].rightButtonsPressed = false;
  }

  uint8_t colorsToLight = 0;
  if(fixedColors) {
    for(uint8_t i = 0; i < nbColors; i++) {
      addRandomColor(colorsToLight);
    }
  }

  // Assigner les couleurs aux joueurs
  for (uint8_t player = 0; player < MAX_PLAYERS; player++) {
    if (players[player].nbrOfModules > 0) {
      if(fixedColors) {
        //parcourir toutes les couleurs possibles
        for(uint8_t color = 0; color < NB_COLORS; color++) {
          if(colorsToLight & (1 << color)) {
            // Si la couleur doit être alluméee, l'assigner à un module du joueur
            int8_t module = getRandomModule(players[player].nbrOfModules, players[player].modules);
            if (module != -1) {
              modules[module].buttonsToPress |= (1 << color); // Assigner la couleur au module
            }
          }
        }
      }else{
        // Assigner une couleur aléatoire à un module du joueur jusqu'à ce qu'il y ait nbColors assignés
        // Attention à ne pas assigner plusieurs fois la même combianison couleur/module
        uint8_t countColors = 0;
        do{
          int8_t module = getRandomModule(players[player].nbrOfModules, players[player].modules);
          if (module != -1) {
            uint8_t color = random(0, NB_COLORS);
            if ((modules[module].buttonsToPress & (1 << color)) == 0) { // Si la couleur n'est pas déjà assignée au module
              modules[module].buttonsToPress |= (1 << color); // Assigner la couleur au module
              countColors++;
            }
          }
        }while(countColors < nbColors);
      }
    }
  }

  // Envoi des couleurs aux modules
  for (uint8_t module = 0; module < NBR_SLAVES; ++module) {
    if(modules[module].playerOfModule == NONE) {
      continue; // Skip if the module is not assigned to any player
    }
    PayloadFromMasterStruct payload;
    payload.command = CMD_BUTTONS;
    payload.buttonsToPress = modules[module].buttonsToPress;
    payload.score = 0; // Not used in this case
    // Envoi de la commande au module
    sendPayloadToSlave(payload, module);
  }
}