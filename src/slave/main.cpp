#include <Arduino.h>
#include <Adafruit_AW9523.h>
#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#include <slave_pins.h>
#include <communication.h>
#include <button.h>

#ifndef SLAVE_ID
#define SLAVE_ID -1
#endif

enum State{STOPGAME, SETUP, GAME};

void sendMessageToMaster(bool buttonsPressed);
void readFromMaster();

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);
unsigned long lastSendTime = 0;

// instantiate an object for the AW9523 GPIO expander
Adafruit_AW9523 aw;

Button* buttons[4];

State actualState = STOPGAME;
uint16_t score = 0;
Player idPlayer = NONE; // Pour l'instant à attribuer mieux après

uint16_t time = 0; // Variable pour le temps d'attente aléatoire entre les envois

void setup() {
  Serial.begin(9600);
  Serial.print("Je suis le slave ID : ");
  Serial.println(SLAVE_ID);

  if (!aw.begin(0x58)) {
    Serial.println("AW9523 not found? Check wiring!");
    while (1) delay(10);  // halt forever
  }
  //initialisation après être sur que le module est connecté
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

  lastSendTime = millis();
  time = random(1000, 3000); //to test a random response time
}

void loop() {
  readFromMaster();
  unsigned long currentTime = millis();   // valeur tests d'envoi
  if (currentTime - lastSendTime >= time) {  //valeur test d'envoi
    time = random(1000, 3000); 
    // Mettre à jour le dernier temps d'envoi
    lastSendTime = currentTime;

    bool trueButtons = random(0, 2);  // Random entre 0 et 1

    Serial.println(F("\n==========TRUE BUTTONS AND PLAYER ID=========="));
    // Afficher les valeurs dans le terminal pour vérifier
    Serial.print(F("Random trueButtons: "));
    Serial.println(trueButtons);
    Serial.print(F("Player ID: "));
    Serial.println(idPlayer);  // Affiche en binaire

    // Appeler la fonction d'envoi avec la commande et les récepteurs générés
    sendMessageToMaster(trueButtons);
  }
}

void sendMessageToMaster(bool buttonsPressed){
  PayloadFromSlaveStruct payloadFromSlave;
  
  payloadFromSlave.idPlayer = idPlayer;
  payloadFromSlave.buttonsPressed = buttonsPressed;

  //début com 
  radio.stopListening();
  unsigned long start_timer = micros();                  // start the timer
  bool report = radio.write(&payloadFromSlave, sizeof(payloadFromSlave));  // transmit & save the report
  unsigned long end_timer = micros();                    // end the timer
  radio.startListening();  // put radio in RX mode

  Serial.println(F("\n==========NEW TRANSMISSION=========="));
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

    Serial.println(F("\n==========NEW RECEPTION=========="));
    printPayloadFromMasterStruct(payloadFromMaster);

    // Change mode based on the command received
    // Turn on/off LEDs based on the received command
    // Update score
    // Play sound if needed
    switch (payloadFromMaster.command){
      case CMD_SETUP:
        actualState = SETUP;
        /* code */
        break;
      case CMD_BUTTONS:
        actualState = GAME;
        /* code */
        break;
      case CMD_SCORE:
        actualState = GAME;
        /* code */
        break;
      case CMD_MISSED_BUTTONS:
        actualState = GAME;
        /* code */
        break;
      default: //CMD_STOP_GAME
        actualState = STOPGAME;
        break;
    }
  }
}