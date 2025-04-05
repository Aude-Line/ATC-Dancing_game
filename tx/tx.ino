/**
* Source code:
* https://www.italiantechproject.it/tutorial-arduino/wireless-nrf21l01
*/
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
 
RF24 wireless(10, 9);
const byte address[6] = "00001";
const char msg[] = "Salut Oceane";
 
void setup() {
  Serial.begin(9600);
  wireless.begin();
  wireless.setChannel(125);
  wireless.openWritingPipe(address);
  wireless.setPALevel(RF24_PA_LOW);
  wireless.stopListening();
}
 
void loop() {
  bool result = wireless.write(&msg, sizeof(msg));
  if (result) {
    Serial.println("Comunicazione inviata correttamente");
  } else {
    Serial.println("Errore: messaggio non ricevuto");
  }
  delay(1000);
}
