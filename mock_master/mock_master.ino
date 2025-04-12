/**
* Master code to test the skeleton of the code (communication and threads)
*/

#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#define NBR_SLAVES 3 //MAX 6 or else we can no longer use the multiceiver function
#define MAX_PLAYERS 4
#define CE_PIN 9
#define CSN_PIN 10

enum Players{NONE, PLAYER1, PLAYER2, PLAYER3, PLAYER4};

struct Module{
  Players player;
  uint8_t ButtonsToPress;
  uint16_t score;
};

Module modules[NBR_SLAVES];
uint8_t nbrOFModulesPerPlayer[MAX_PLAYERS + 1];

struct PayloadFromMasterStruct{
  uint8_t ButtonsToPress;
  uint16_t score;
};
struct PayloadFromSlaveStruct{
  uint8_t ButtonsPressed;
};

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);

/*
* reference values of the tx and rx channels
* they will be dynamically attributed for each slave, max 6 slaves
* exemple: slave 3 will use the adresses 5A36484133 and 5448344433
*/
const uint64_t addresses[2] = { 0x5A36484130LL,
                                0x5448344430LL};

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    // some boards need to wait to ensure access to serial over USB
  }

  radio.begin();
  radio.setChannel(125);
  radio.setPALevel(RF24_PA_LOW);
  switchToListening();

  initModules(modules);
}

void loop() {
  //print64Hex(addresses[1]);
  //print64Hex(addresses[1]+1);
  //delay(1000);
}

void print64Hex(uint64_t val) {
  uint32_t high = (uint32_t)(val >> 32);  // Poids fort
  uint32_t low = (uint32_t)(val & 0xFFFFFFFF); // Poids faible

  Serial.print(high, HEX);
  Serial.println(low, HEX);
}

void switchToListening(){
  for (uint8_t i = 0; i < NBR_SLAVES; ++i){
    radio.openReadingPipe(i, addresses[1]+i);
  }
  radio.startListening();  // put radio in RX mode
}

void sendMessage(uint8_t receivers){
  for (uint8_t i = 0; i < NBR_SLAVES; ++i){
    if((receivers & (1<<i))==0){
      continue; // we do not want to send to this receiver
    }

  }

}

void initModules(Module* modules) {
  for (uint8_t i = 0; i < NBR_SLAVES; i++) {
    modules[i].player = NONE;
    modules[i].score = 0;
    modules[i].ButtonsToPress = 0;
  }
  nbrOFModulesPerPlayer[NONE] = NBR_SLAVES;
  
}