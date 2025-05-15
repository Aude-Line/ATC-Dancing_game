#include <Arduino.h>
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

enum GameState{STOPGAME, SETUP, GAME}; // Added different game modes as states

void resetModule();
void sendMessageToMaster(bool buttonsPressed, Player playerId);
void readFromMaster();
void turnOffLeds();

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);
unsigned long lastSendTime = 0;

// instantiate an object for the AW9523 GPIO expander
Adafruit_AW9523 aw;
Adafruit_7segment matrix = Adafruit_7segment();

Button* buttons[4];

GameState actualState = STOPGAME;
uint16_t score = 0;
Player idPlayer = NONE;
bool shouldSend = false;
bool rightButtonsPressed = false;

void setup() {
  Serial.begin(9600);
  Serial.print("Je suis le slave ID : ");
  Serial.println(SLAVE_ID);

  if (!aw.begin(0x58)) {
    Serial.println("AW9523 not found? Check wiring!");
    while (1) delay(10);  // halt forever
  }

  if (!matrix.begin(0x70)) {
    Serial.println("7 digit display not found? Check wiring!");
    while (1) delay(10);  // halt forever
  }
  //initialisation après être sur que le module est connecté
  //Should the buttons and leds be initialized?
  buttons[RED] = new Button(RED_BUTTON_PIN, RED_LED_PIN, &aw);
  buttons[GREEN] = new Button(GREEN_BUTTON_PIN, GREEN_LED_PIN, &aw);
  buttons[BLUE] = new Button(BLUE_BUTTON_PIN, BLUE_LED_PIN, &aw);
  buttons[YELLOW] = new Button(YELLOW_BUTTON_PIN, YELLOW_LED_PIN, &aw);

  pinMode(BUZZER_PIN, OUTPUT);
  analogWrite(BUZZER_PIN, 3); // Set the buzzer to off

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
  radio.setRetries(5, 15);
  radio.setAutoAck(true);
  radio.enableDynamicPayloads(); //as we have different payload size
  radio.openWritingPipe(addresses[1]+SLAVE_ID);
  radio.openReadingPipe(1, addresses[0]+SLAVE_ID);
  radio.startListening();
  matrix.setBrightness(7);
  resetModule();

}

void loop() {
  //Serial.println(F("\n==========LECTURE=========="));
  readFromMaster();

  //Serial.println(F("\n==========CHECK SON ETAT=========="));
  switch(actualState){
    case SETUP: {
      for(uint8_t button = 0; button < NB_COLORS; button++){
        if(buttons[button]->state() == JUST_PRESSED){
          tone(BUZZER_PIN, SOUND_FREQUENCY_NEUTRAL, SOUND_DURATION_SHORT);
          if(buttons[button]->isLedOn()){
            buttons[button]->turnOffLed();
            idPlayer = NONE;
          }else{
            turnOffLeds(); //éteindre les autres LEDS, peut être mieux optimisé
            buttons[button]->turnOnLed();
            idPlayer = (Player)button; //idPlayer = button
            Serial.print(F(" Setting ID player: "));
            Serial.println(idPlayer);
          }
          shouldSend = true;
          Serial.print(F(" set to ID player: "));
          Serial.println(idPlayer);
        }
      }

      break;
    }
    case GAME: {
      rightButtonsPressed = true;
      for(uint8_t button = 0; button < NB_COLORS; button++){
        if(buttons[button]->state() == JUST_PRESSED){ //à partir de maintenent, le nouvel à ce bouton dira PRESSED
          Serial.print(F("New button pressed: "));
          shouldSend = true;
          //check la led du nouveau bouton
          if(buttons[button]->isLedOn()){
            Serial.print(F("Right button: "));
            rightButtonsPressed = true;
          }else{
            Serial.print(F("Wrong button: "));
            rightButtonsPressed = false;
          }
          tone(BUZZER_PIN, SOUND_FREQUENCY_NEUTRAL, SOUND_DURATION_SHORT);
        }
      }
      if(rightButtonsPressed){
        //voir si il manque un bouton, si oui ne pas envoyer de message
        for(uint8_t button = 0; button < NB_COLORS; button++){
          if(buttons[button]->isLedOn() && buttons[button]->state() != PRESSED){
            shouldSend = false;
          }
        }
      }

      //TODO un message quand le bouton est relaché pour avoir les 2 joueurs qui doievent appuyé en même temps

      break;
    }
    case STOPGAME: {
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
    //début com
    Serial.println(F("\n==NEW TRANSMISSION==\n BUTTONS PRESSED"));
    Serial.println(idPlayer);

    sendMessageToMaster(rightButtonsPressed,idPlayer);
    shouldSend = false;
  }
}

void turnOffLeds(){
  for (uint8_t button=0; button<NB_COLORS; ++button){
    buttons[button]->turnOffLed();
  }
}

void resetModule(){
  Serial.println("Reset");
  turnOffLeds();
  idPlayer = NONE;
  score = 0;
}

void sendMessageToMaster(bool rightButtonsPressed, Player idPlayer){
  PayloadFromSlaveStruct payloadFromSlave;
  payloadFromSlave.slaveId = SLAVE_ID;
  Serial.println("dans la function player ID:");
  Serial.println(idPlayer);

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

// Function to read the payload from the master if there is one
void readFromMaster(){
  if (radio.available()) {  
    PayloadFromMasterStruct payloadFromMaster;
    // is there a payload? get the pipe number that received it
    uint8_t bytes = radio.getDynamicPayloadSize();  // get the size of the payload
    radio.read(&payloadFromMaster, bytes);             // fetch payload from FIFO

    Serial.println(F("\n==NEW RECEPTION=="));
    printPayloadFromMasterStruct(payloadFromMaster);

    // Change mode based on the command received
    // Turn on/off LEDs based on the received command
    // Update the score based on the received command
    Serial.println("Payload from master:");
    Serial.println(payloadFromMaster.command);

    switch (payloadFromMaster.command){
      case CMD_SETUP:{
        actualState = SETUP;
        resetModule(); //POURQUOI il était enlevé?
        break;
      }
      case CMD_BUTTONS:{
        for (uint8_t button=0; button<NB_COLORS; ++button){
          if(payloadFromMaster.buttonsToPress & (1 << button)){
            buttons[button]->turnOnLed();
            Serial.println("Turning on LED");
          }else{
            buttons[button]->turnOffLed();
          }
        }
        break;
      }
      case CMD_SCORE:{ //si mauvais bouton ou pas de bouton appuyé, le master envoye SCORE_FAILED
        Serial.println("READING SCORE");
        if(payloadFromMaster.score == SCORE_FAILED){
          Serial.println(F("Wrong button pressed"));
          tone(BUZZER_PIN, SOUND_FREQUENCY_BAD, SOUND_DURATION_LONG);
        }else if (payloadFromMaster.score == SCORE_SUCCESS){
          score += 1;
          Serial.print(F("Score updated: "));
          Serial.println(score);
          tone(BUZZER_PIN, SOUND_FREQUENCY_GREAT, SOUND_DURATION_LONG);
        }
        turnOffLeds(); //éteindre les leds
        matrix.print(score);
        matrix.writeDisplay();
        break;
      }
      case CMD_START_GAME:{
        actualState = GAME;
        turnOffLeds();
        score = 0;
        matrix.print(score);
        matrix.writeDisplay();
        Serial.println("Ready to start the game!");
        break;
      }
      case CMD_STOP_GAME:{ //continue d'afficher le score
        Serial.println("Stopping Game");
        actualState = STOPGAME;
        turnOffLeds();
        break;
      }
      default:{
        Serial.print("Je suis entré dans le default case...");
        actualState = STOPGAME;
        resetModule();
        break;
      }
    }
  }
}