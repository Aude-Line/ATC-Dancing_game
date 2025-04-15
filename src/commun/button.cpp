#include "button.h"

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