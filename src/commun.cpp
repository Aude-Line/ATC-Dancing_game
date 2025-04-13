#include "commun.h"

const uint64_t addresses[2] = {
    0x5A36484130LL,
    0x5448344430LL
}; // adresses utilisées pour la communication entre le master et les slaves

void print64Hex(uint64_t val) {
    uint32_t high = (uint32_t)(val >> 32);  // Poids fort
    uint32_t low = (uint32_t)(val & 0xFFFFFFFF); // Poids faible
  
    Serial.print(high, HEX);
    Serial.println(low, HEX);
}