/**
* Master code to test the skeleton of the code (communication and threads)
*/

#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#define ID_SLAVE 1 //Unique and in [0, NBR_SLAVES[ (defined in the master code)
#define CE_PIN 9
#define CSN_PIN 10
// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);

/*
* reference values of the rx and tx channels
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

  delay(1500); //juste pour que le terminal ne commence pas à lire avant et qu'il y ait des trucs bizarres dedans, jsp si c'est ok normalement
  Serial.print("address to send: ");
  print64Hex(addresses[1] + ID_SLAVE);
  Serial.print("address to receive: ");
  print64Hex(addresses[0] + ID_SLAVE);
  
  radio.begin();
  radio.setChannel(125);
  radio.openWritingPipe(addresses[1]+ID_SLAVE);
  radio.openReadingPipe(1, addresses[0]+ID_SLAVE);
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening();

  // Copied from MulticeiverDemo code, need adaptation
  //According to the datasheet, the auto-retry features's delay value should
  // be "skewed" to allow the RX node to receive 1 transmission at a time.
  // So, use varying delay between retry attempts and 15 (at most) retry attempts
  //radio.setRetries(((role * 3) % 12) + 3, 15);  // maximum value is 15 for both args
}

void loop() {
  //delay(1000);
}

void print64Hex(uint64_t val) {
  uint32_t high = (uint32_t)(val >> 32);  // Poids fort
  uint32_t low = (uint32_t)(val & 0xFFFFFFFF); // Poids faible

  Serial.print("0x");
  Serial.print(high, HEX);
  Serial.println(low, HEX);
}
