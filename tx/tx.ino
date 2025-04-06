/**
* Source code:
* https://www.italiantechproject.it/tutorial-arduino/wireless-nrf21l01
*/
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
 
struct Payload {
  uint8_t commandFlags;
  int Data;
};


RF24 wireless(10, 9);
const byte addresses[][6] = {"00001", "00002","00003"};

Payload datatoSend;
const int deviceID = 0;

 
void setup() {
  Serial.begin(9600);
  wireless.begin();
  wireless.setChannel(125);
  wireless.openWritingPipe(addresses[0]); // Address of Transmitter which will send data
  wireless.openReadingPipe(1, addresses[1]);
  wireless.setPALevel(RF24_PA_LOW);
  wireless.stopListening();
}
 
 
void loop() {
  datatoSend.commandFlags = B00000100; // the slave1 will not respond but slave2 yes
  datatoSend.Data = random(0,100);

  Serial.print("Sending: ");
  Serial.print("Flags = ");
  Serial.print(dataToSend.commandFlags, BIN);
  Serial.print(", Data = ");
  Serial.println(dataToSend.Data);
  wireless.write(&datatoSend, sizeof(datatoSend));

  radio.write(&dataToSend, sizeof(dataToSend));

  delay(2000); // Wait 2 seconds before sending again
  
}
