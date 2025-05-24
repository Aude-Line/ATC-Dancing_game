#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>  // add the FreeRTOS functions for Semaphores (or Flags).
#include <task.h>    // add the FreeRTOS functions for Tasks
#include <Adafruit_AW9523.h>
#include <SPI.h>
#include "printf.h"
#include "RF24.h"
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#include <slave_pins.h>
#include <util.h>
#include <communication.h>
#include <button.h>

#ifndef SLAVE_ID
#define SLAVE_ID -1
#endif

#define PERIOD_BUTTON 100 // 100ms
#define PERIOD_COMM 50 // 50ms

enum GameState{STOPGAME, SETUP, GAME}; // Added different game modes as states

void turnOffLeds();
void resetModule();
bool sendPayloadToMaster(PayloadFromSlaveStruct& payload);
bool getPayloadFromMaster(PayloadFromMasterStruct& payload);
void handlePayloadFromMaster(const PayloadFromMasterStruct& payloadFromMaster);
void setUpActions();
void gameActions();

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);
unsigned long lastSendTime = 0;

// instantiate an object for the AW9523 GPIO expander
Adafruit_AW9523 aw;
Adafruit_7segment matrix = Adafruit_7segment();

// store the buttons
Button* buttons[NB_COLORS];

GameState actualState = STOPGAME;
uint16_t score = 0;
Player idPlayer = NONE;

//semaphore for serial communication
SemaphoreHandle_t xSerialSemaphore;
//semaphore for radio communication
SemaphoreHandle_t xRadioSemaphore;

// define the tasks
void TaskReadFromMaster(void *pvParameters);
void TaskHandleButtons(void *pvParameters);

void setup() {
  //Connect to the serial
  Serial.begin(115200);
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

  //Connect to to the aw GPIO expander
  if (!aw.begin(0x58)) {
    if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("AW9523 not found? Check wiring!"));
      xSemaphoreGive(xSerialSemaphore);
    }
  }

  //Connect to the 7 segment display
  if (!matrix.begin(0x70)) {
    if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("7 digit display not found? Check wiring!"));
      xSemaphoreGive(xSerialSemaphore);
    }
  }
  matrix.setBrightness(15);

  //Inititalize the pins (buttons, leds, buzzer)
  buttons[RED] = new Button(RED_BUTTON_PIN, RED_LED_PIN, &aw);
  buttons[GREEN] = new Button(GREEN_BUTTON_PIN, GREEN_LED_PIN, &aw);
  buttons[BLUE] = new Button(BLUE_BUTTON_PIN, BLUE_LED_PIN, &aw);
  buttons[YELLOW] = new Button(YELLOW_BUTTON_PIN, YELLOW_LED_PIN, &aw);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize the radio
  Serial.print(F("address to send: "));
  print64Hex(addresses[1] + SLAVE_ID);
  Serial.print(F("address to receive: "));
  print64Hex(addresses[0] + SLAVE_ID);
  // initialize the transceiver on the SPI bus
  if (!radio.begin()) {
    if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("radio hardware is not responding!!"));
      xSemaphoreGive(xSerialSemaphore);
    }
  }
  radio.setChannel(125);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setRetries(5, 15);
  radio.setAutoAck(true);
  radio.enableDynamicPayloads(); //as we have different payload size
  radio.openWritingPipe(addresses[1]+SLAVE_ID);
  radio.openReadingPipe(1, addresses[0]+SLAVE_ID);
  radio.startListening(); // Always in listening mode (except when sending

  // Start with a clean module
  resetModule();

  if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    Serial.print(F("Je suis le slave ID : "));
    Serial.println(SLAVE_ID);
    xSemaphoreGive(xSerialSemaphore);
  }

  // Create the tasks
  // The tasks are created in the setup function, and they will run in parallel
  if (xTaskCreate(TaskReadFromMaster, "TaskReadFromMaster", 160, NULL, 2, NULL) != pdPASS) {
    if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("Erreur : création tâche échouée !"));
      xSemaphoreGive(xSerialSemaphore);
    }
  }
  
  if (xTaskCreate(TaskHandleButtons, "TaskHandleButtons", 160, NULL, 1, NULL) != pdPASS) {
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

void TaskReadFromMaster(void *pvParameters) {
  (void) pvParameters; // suppress unused parameter warning
  if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    Serial.println(F("Tache lecture lancée"));
    xSemaphoreGive(xSerialSemaphore);
  }

  PayloadFromMasterStruct payload;

  for (;;) {
    if (getPayloadFromMaster(payload)) {
      handlePayloadFromMaster(payload);
    }
    // Print Debug
    /*
    if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {
      UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
      Serial.print(F("[TaskReadFromMaster] Stack remaining: "));
      Serial.println(watermark);
      xSemaphoreGive(xSerialSemaphore);
    }
    */
    vTaskDelay(PERIOD_COMM / portTICK_PERIOD_MS);
  }
}

//this function will also send messages to the master
void TaskHandleButtons(void *pvParameters) {
  (void) pvParameters; // suppress unused parameter warning
  if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    Serial.println(F("Tache boutons lancée"));
    xSemaphoreGive(xSerialSemaphore);
  }

  for (;;) {
    // update the state of the buttons
    for (uint8_t button = 0; button < NB_COLORS; ++button) {
      buttons[button]->updateState();
    }

    // Act based on the state of the module
    switch(actualState){
      case SETUP: {
        setUpActions();
        break;
      }
      case GAME: {
        gameActions();
        break;
      }
      case STOPGAME: {
        break;
      }
      default: {
        break;
      }
    }

    // Print Debug
    /*
    if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {
      UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
      Serial.print(F("[TaskHandleButtons] Stack remaining: "));
      Serial.println(watermark);
      xSemaphoreGive(xSerialSemaphore);
    }
    */
    
    vTaskDelay(PERIOD_BUTTON / portTICK_PERIOD_MS);
  }
}

/*--------------------------------------------------------------------
  ------------------------------FUNCTIONS-----------------------------
  --------------------------------------------------------------------*/

void turnOffLeds(){
  for (uint8_t button=0; button<NB_COLORS; ++button){
    buttons[button]->turnOffLed();
  }
}

void turnOnLeds(){
  for (uint8_t button=0; button<NB_COLORS; ++button){
    buttons[button]->turnOnLed();
  }
}

void resetModule(){
  idPlayer = NONE;
  turnOffLeds();
  score = 0;
  if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    Serial.println(F("===================Reset module==================="));
    xSemaphoreGive(xSerialSemaphore);
  }
}

bool sendPayloadToMaster(SlaveButtonsState buttonsPressed=BUTTONS_RELEASED) {
  bool send = false;

  if (xSemaphoreTake(xRadioSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    PayloadFromSlaveStruct payload;
    payload.slaveId = SLAVE_ID;
    payload.playerId = idPlayer;
    payload.buttonsPressed = buttonsPressed;
  
    //début com 
    radio.stopListening();
    unsigned long start_timer = micros();                  // start the timer
    bool report = radio.write(&payload, sizeof(payload));  // transmit & save the report
    unsigned long end_timer = micros();                    // end the timer
    radio.startListening();  // put radio in RX mode
    xSemaphoreGive(xRadioSemaphore);
    send = report;

    // Print the payload on the serial monitor for debugging
    if(xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("\n==NEW TRANSMISSION=="));
      printPayloadFromSlaveStruct(payload);
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
  return send;
}

bool getPayloadFromMaster(PayloadFromMasterStruct& payloadFromMaster) {
  bool result = false;

  if (xSemaphoreTake(xRadioSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
    if (radio.available()) {
      uint8_t bytes = radio.getDynamicPayloadSize();
      radio.read(&payloadFromMaster, bytes);
      result = true;
    }
    xSemaphoreGive(xRadioSemaphore);
  }

  // Print the payload on the serial monitor for debugging
  
  if(result){
    if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
      Serial.println(F("\n==NEW RECEPTION=="));
      printPayloadFromMasterStruct(payloadFromMaster);
      xSemaphoreGive(xSerialSemaphore);
    }
  }
  
  return result;
}

void handlePayloadFromMaster(const PayloadFromMasterStruct& payloadFromMaster) {
  switch (payloadFromMaster.command) {
    case CMD_SETUP: {
      idPlayer = NONE;
      actualState = SETUP;
      resetModule();
      break;
    }
    case CMD_BUTTONS: {
      for (uint8_t button = 0; button < NB_COLORS; ++button) {
        if (payloadFromMaster.buttonsToPress & (1 << button)) {
          buttons[button]->turnOnLed();
        } else {
          buttons[button]->turnOffLed();
        }
      }
      break;
    }
    case CMD_SCORE: {
      if (payloadFromMaster.score == SCORE_FAILED) {
        tone(BUZZER_PIN, SOUND_FREQUENCY_BAD, SOUND_DURATION_LONG);
        turnOffLeds();
      } else if (payloadFromMaster.score == SCORE_SUCCESS) {
        score += 1;
        tone(BUZZER_PIN, SOUND_FREQUENCY_GREAT, SOUND_DURATION_LONG);
        turnOffLeds();
        matrix.print(score);
        matrix.writeDisplay();
      } else if (payloadFromMaster.score == SCORE_NEUTRAL) {
        //ne rien faire
        //tone(BUZZER_PIN, SOUND_FREQUENCY_NEUTRAL, SOUND_DURATION_SHORT); (deuxième bip de confirmation du master)
      }
      break;
    }
    case CMD_START_GAME: {
      actualState = GAME;
      turnOffLeds();
      score = 0;
      matrix.print(score);
      matrix.writeDisplay();
      break;
    }
    case CMD_STOP_GAME: {
      actualState = STOPGAME;
      turnOffLeds();
      break;
    }
    case CMD_END_PLAY_TIME: {
      actualState = STOPGAME;
      turnOnLeds();
      break;
    }
    case CMD_TURN_OFF_LEDS: {
      turnOffLeds();
      break;
    }
    default: {
      actualState = STOPGAME;
      resetModule();
      break;
    }
  }
}

void setUpActions(){
  for(uint8_t button = 0; button < NB_COLORS; button++){
    if(buttons[button]->getState() == JUST_PRESSED){
      tone(BUZZER_PIN, SOUND_FREQUENCY_NEUTRAL, SOUND_DURATION_SHORT);
      if(buttons[button]->isLedOn()){
        buttons[button]->turnOffLed();
        idPlayer = NONE;
      }else{
        turnOffLeds();
        buttons[button]->turnOnLed();
        idPlayer = static_cast<Player>(button);
        if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
          Serial.print(F("Player selected: "));
          Serial.println(idPlayer);
          xSemaphoreGive(xSerialSemaphore);
        }
      }
      bool sendSuccess = sendPayloadToMaster();
      if(!sendSuccess){
        if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
          Serial.println(F("❌ Transmission failed"));
          xSemaphoreGive(xSerialSemaphore);
        }
      }
    }
  }
}

void gameActions(){
  SlaveButtonsState rightButtonsPressed = RIGHT_BUTTONS_PRESSED;
  bool shouldSend = false;
  for(uint8_t button = 0; button < NB_COLORS; button++){
    if(buttons[button]->getState() == JUST_PRESSED){
      tone(BUZZER_PIN, SOUND_FREQUENCY_NEUTRAL, SOUND_DURATION_SHORT);
      shouldSend = true;
      if(buttons[button]->isLedOn()==false){
        rightButtonsPressed = WRONG_BUTTONS_PRESSED;
      }
    }
    if(buttons[button]->getState() == JUST_RELEASED){
      shouldSend = true;
      rightButtonsPressed = BUTTONS_RELEASED;
    }
  }
  if(rightButtonsPressed==RIGHT_BUTTONS_PRESSED && shouldSend){
    for(uint8_t button = 0; button < NB_COLORS; button++){
      if(buttons[button]->isLedOn()){
        if(buttons[button]->getState() == NOT_PRESSED || buttons[button]->getState() == JUST_RELEASED){
          //un bouton qui devait être appuyé ne l'est pas
          shouldSend = false;
        }
      }
    }
  }
  if(shouldSend){
    bool sendSuccess = sendPayloadToMaster(rightButtonsPressed);
    if(!sendSuccess){
      if (xSemaphoreTake(xSerialSemaphore, (TickType_t)10) == pdTRUE) {  // timeout 10 ticks
        Serial.println(F("❌ Transmission failed"));
        xSemaphoreGive(xSerialSemaphore);
      }
    }
  }
}