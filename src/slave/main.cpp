#include <Arduino.h>
#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#include <slave_pins.h>
#include <commun.h>

#ifndef SLAVE_ID
#define SLAVE_ID -1
#endif

enum State{STOPGAME, SETUP, GAME};

void sendMessage(bool buttonsPressed);
void read();

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);
unsigned long lastSendTime = 0;

uint16_t time = 0;
Player idPlayer = static_cast<Player>(SLAVE_ID);

void setup() {
  Serial.begin(9600);
  Serial.print("Je suis le slave ID : ");
  Serial.println(SLAVE_ID);

  Serial.print("address to send: ");
  print64Hex(addresses[1] + SLAVE_ID);
  Serial.print("address to receive: ");
  print64Hex(addresses[0] + SLAVE_ID);
  
  // initialize the transceiver on the SPI bus
  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {}  // hold in infinite loop
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
  read();
  unsigned long currentTime = millis();   // Récupère le temps actuel
  if (currentTime - lastSendTime >= time) { 
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
    sendMessage(trueButtons);
  }
}

void sendMessage(bool buttonsPressed){
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

void read(){
  PayloadFromMasterStruct payloadFromMaster;
  if (radio.available()) {              // is there a payload? get the pipe number that received it
    uint8_t bytes = radio.getDynamicPayloadSize();  // get the size of the payload
    radio.read(&payloadFromMaster, bytes);             // fetch payload from FIFO

    Serial.println(F("\n==========NEW RECEPTION=========="));
    printPayloadFromMasterStruct(payloadFromMaster);
  }
}