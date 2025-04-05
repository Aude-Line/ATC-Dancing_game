/**
* Source code:
* https://www.italiantechproject.it/tutorial-arduino/wireless-nrf21l01
*/
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
 
RF24 wireless(10, 9);
const byte address[6] = "00001";
 
void setup() {
  Serial.begin(9600);
  wireless.begin();
  wireless.setChannel(125);
  wireless.openReadingPipe(1, address);
  wireless.setPALevel(RF24_PA_LOW);
  wireless.startListening();
}
 
void loop() {
  char msg[32] = {0};
  if (wireless.available()) {
    wireless.read(&msg, sizeof(msg));
    Serial.println(msg);
  }
  delay(10);
}
