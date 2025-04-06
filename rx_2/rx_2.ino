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

Payload receivedData;
const int deviceID = 2; // device ID

RF24 wireless(7, 8); // Set accordingly to branching
const byte addresses[][6] = {"00001","00002"};

void setup() {
  Serial.begin(9600);
  wireless.begin();
  wireless.setChannel(125);
  wireless.openWritingPipe(addresses[1]);
  wireless.openReadingPipe(1, addresses[0]); // Address of transmitter and data pipe 
  wireless.setPALevel(RF24_PA_LOW);
  wireless.startListening();
}

void loop() {
  if (wireless.available()) {
    wireless.read(&receivedData, sizeof(receiveData));

    bool shouldAct =  receivedData.commandFlags & (1 << deviceID);  // This checks if bit of my device is set to 1 
    
    Serial.print("Device ");
    Serial.print (deviceID);
    Serial.print(" received data. Flag: ");
    Serial.print(receivedData.commandFlags, BIN);

    if (shouldAct) {
      Serial.print(" → My bit is 1! Doing action. Data = ");
      Serial.println(receivedData.someData);   
    } else {
      Serial.println(" → My bit is 0. Ignoring.");
    }
  }
  
}