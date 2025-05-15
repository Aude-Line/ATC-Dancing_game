#include "communication.h"

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

void printPayloadFromMasterStruct(const PayloadFromMasterStruct& payload) {
    Serial.println(F("Payload content:"));
    Serial.print(F("  Command: "));
    Serial.println(payload.command);
    Serial.print(F("  ButtonsToPress: "));
    Serial.println(payload.buttonsToPress);
    Serial.print(F("  Score: "));
    Serial.println(payload.score);
}

void printPayloadFromSlaveStruct(const PayloadFromSlaveStruct& payload) {
    Serial.println(F("Payload content:"));
    Serial.print(F("  Slave ID: "));
    Serial.println(payload.slaveId);
    Serial.print(F("  ID player: "));
    Serial.println(payload.playerId);
    Serial.print(F("  Buttons are pressed correctly: "));
    Serial.println(payload.rightButtonsPressed);
}