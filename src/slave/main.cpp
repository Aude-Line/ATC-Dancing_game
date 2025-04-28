#include <Arduino.h>
#include <Adafruit_AW9523.h>
#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#include <slave_pins.h>
#include <util.h>
#include <communication.h>
#include <button.h>

#ifndef SLAVE_ID
#define SLAVE_ID -1
#endif

enum GameState{STOPGAME, SETUP,GAME}; // Added different game modes as states

void resetModule();
void sendMessageToMaster(bool buttonsPressed);
void readFromMaster();
void turnOffLeds();

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);
unsigned long lastSendTime = 0;

// instantiate an object for the AW9523 GPIO expander
Adafruit_AW9523 aw;

Button* buttons[4];

GameState actualState = STOPGAME;
uint16_t score = 0;
Player idPlayer = NONE; // Pour l'instant à attribuer mieux après

void setup() {
  Serial.begin(9600);
  Serial.print("Je suis le slave ID : ");
  Serial.println(SLAVE_ID);

  if (!aw.begin(0x58)) {
    Serial.println("AW9523 not found? Check wiring!");
    while (1) delay(10);  // halt forever
  }
  //initialisation après être sur que le module est connecté
  //Should the buttons and leds be initialized?
  buttons[RED] = new Button(RED_BUTTON_PIN, RED_LED_PIN, &aw);
  buttons[GREEN] = new Button(GREEN_BUTTON_PIN, GREEN_LED_PIN, &aw);
  buttons[BLUE] = new Button(BLUE_BUTTON_PIN, BLUE_LED_PIN, &aw);
  buttons[YELLOW] = new Button(YELLOW_BUTTON_PIN, YELLOW_LED_PIN, &aw);

  pinMode(BUZZER_PIN, OUTPUT);

  Serial.print("address to send: ");
  print64Hex(addresses[1] + SLAVE_ID);
  Serial.print("address to receive: ");
  print64Hex(addresses[0] + SLAVE_ID);
  // initialize the transceiver on the SPI bus
  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) delay(10);  // halt forever
  }
  radio.setChannel(125);
  radio.setPALevel(RF24_PA_LOW);
  radio.enableDynamicPayloads(); //as we have different payload size
  radio.openWritingPipe(addresses[1]+SLAVE_ID);
  radio.openReadingPipe(1, addresses[0]+SLAVE_ID);
  radio.startListening();
}

void loop() {
  //Serial.println(F("\n==========LECTURE=========="));
  readFromMaster();

  //Serial.println(F("\n==========CHECK SON ETAT=========="));
  bool shouldSend = false;
  bool rightButtonsPressed = false;
  switch(actualState){
    case SETUP: {
      for(uint8_t button = 0; button < NB_COLORS; button++){
        if(buttons[button]->isPressed()){
          if(buttons[button]->isLedOn()){
            buttons[button]->turnOffLed();
            idPlayer = NONE;
          }else{
            turnOffLeds(); //éteindre les autres LEDS, peut être mieux optimisé
            buttons[button]->turnOnLed();
            idPlayer = (Player)button; //idPlayer = button
            Serial.print(F("ID player: "));
            Serial.println(idPlayer);
          }
          shouldSend = true;
        }
      }
      
      break;
    }
    case GAME: {
      uint8_t nbrOfNotPressedButtons = 0;
      bool atLeastOneButtonPressed = false;
      for(uint8_t button = 0; button < NB_COLORS; button++){
        if(buttons[button]->isPressed()){
          atLeastOneButtonPressed = true;
          if(buttons[button]->isLedOn()){
            rightButtonsPressed = true;
            buttons[button]->turnOffLed();
            Serial.print(F("Right button pressed: "));
            Serial.println(button);
          }else{ //un bouton qui ne devait pas être appuyé a été appuyé, envoit au master
            rightButtonsPressed = false;
            shouldSend = true;
            Serial.print(F("Wrong button pressed: "));
            Serial.println(button);
          }
        }else if(buttons[button]->isLedOn()){ //un bouton qui doit être appuyé n'a pas encore été appuyé
          nbrOfNotPressedButtons++;
        }
      }
      if(nbrOfNotPressedButtons == 0 and atLeastOneButtonPressed){ //tous les boutons qui devaient être appuyés ont été appuyés
        shouldSend = true;
      }
      break;
    }
    case STOPGAME: {
      Serial.println(F("Game is stopped."));
      shouldSend = false;
      break;
    }
    default: {
      shouldSend = false;
      break;
    }
  }

  //Serial.print(F("\n==========ENVOI SI BESOIN=========="));
  if(shouldSend){
    sendMessageToMaster(rightButtonsPressed);
  }
}

void turnOffLeds(){
  for (uint8_t button=0; button<NB_COLORS; ++button){
    buttons[button]->turnOffLed();
  }
}

void resetModule(){
  turnOffLeds();
  idPlayer = NONE;
  score = 0;
}

void sendMessageToMaster(bool rightButtonsPressed){
  PayloadFromSlaveStruct payloadFromSlave;
  payloadFromSlave.slaveId = SLAVE_ID;
  payloadFromSlave.playerId = idPlayer;
  payloadFromSlave.rightButtonsPressed = rightButtonsPressed;

  //début com 
  radio.stopListening();
  unsigned long start_timer = micros();                  // start the timer
  bool report = radio.write(&payloadFromSlave, sizeof(payloadFromSlave));  // transmit & save the report
  unsigned long end_timer = micros();                    // end the timer
  radio.startListening();  // put radio in RX mode

  Serial.println(F("\n==NEW TRANSMISSION=="));
  printPayloadFromSlaveStruct(payloadFromSlave);
  if (report) {
    Serial.print(F("✅ Transmission successful in "));
    Serial.print(end_timer - start_timer);
    Serial.println(F(" µs"));
  } else {
    Serial.println(F("❌ Transmission failed"));
  }

}

void readFromMaster(){
  PayloadFromMasterStruct payloadFromMaster;
  if (radio.available()) {              // is there a payload? get the pipe number that received it
    uint8_t bytes = radio.getDynamicPayloadSize();  // get the size of the payload
    radio.read(&payloadFromMaster, bytes);             // fetch payload from FIFO

    Serial.println(F("\n==NEW RECEPTION=="));
    printPayloadFromMasterStruct(payloadFromMaster);

    // Change mode based on the command received
    // Turn on/off LEDs based on the received command
    // Update score
    // Play sound if needed
    switch (payloadFromMaster.command){
      case CMD_SETUP:
        actualState = SETUP;
        resetModule();
        break;
      case CMD_BUTTONS:
        actualState = GAME;
        for (uint8_t button=0; button<NB_COLORS; ++button){
          if(payloadFromMaster.buttonsToPress & (1 << button)){
            buttons[button]->turnOnLed();
          }else{
            buttons[button]->turnOffLed();
          }
        }
        break;
      case CMD_SCORE: //si mauvais bouton ou pas de bouton appuyé, le master envoye SCORE_FAILED
        actualState = GAME;
        if(payloadFromMaster.score == SCORE_FAILED){
          turnOffLeds(); //éteindre les autres led si mauvais bouton appuyé
          Serial.println(F("Wrong button pressed"));
          tone(BUZZER_PIN, SOUND_FREQUENCY_BAD, SOUND_DURATION_LONG);
        }else{
          score += 1;
          Serial.print(F("Score updated: "));
          Serial.println(score);
          tone(BUZZER_PIN, SOUND_FREQUENCY_GREAT, SOUND_DURATION_LONG);
        }
        break;
      default: //CMD_STOP_GAME
        actualState = STOPGAME;
        resetModule();
        break;
    }
  }
}