#include "commun.h"

const uint64_t addresses[2] = {
    0x5A36484130LL,
    0x5448344430LL
}; // adresses utilisées pour la communication entre le master et les slaves

Button::Button(uint8_t buttonPin, uint8_t ledPin, Adafruit_AW9523* aw)
  : buttonPin(buttonPin), ledPin(ledPin), aw(aw), ledAw9523(aw != nullptr) {
    init();
  }

void Button::init() {
  pinMode(buttonPin, INPUT_PULLUP);
  if (ledAw9523) {
    aw->pinMode(ledPin, AW9523_LED_MODE); // set to constant current drive!
    aw->digitalWrite(ledPin, HIGH);     // LED éteinte (active LOW)
  } else {
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);          // LED éteinte (active HIGH)
  }
  ledOn = false;
}

bool Button::isPressed() const {
  return digitalRead(buttonPin) == LOW;
}

void Button::turnOnLed() {
  if (ledAw9523) {
    aw->digitalWrite(ledPin, LOW); // LED allumée (active LOW)
  } else {
    digitalWrite(ledPin, HIGH);    // LED allumée (active HIGH)
  }
  ledOn = true;
}

void Button::turnOffLed() {
  if (ledAw9523) {
    aw->digitalWrite(ledPin, HIGH); // LED éteinte (active LOW)
  } else {
    digitalWrite(ledPin, LOW);      // LED éteinte (active HIGH)
  }
  ledOn = false;
}

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
    Serial.print(F("  ID player: "));
    Serial.println(payload.idPlayer);
    Serial.print(F("  Buttons are pressed correctly: "));
    Serial.println(payload.buttonsPressed);
}