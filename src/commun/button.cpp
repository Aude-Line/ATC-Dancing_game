#include "button.h"

Button::Button(uint8_t buttonPin, uint8_t ledPin, Adafruit_AW9523* aw)
  : buttonPin(buttonPin), ledPin(ledPin), aw(aw), ledAw9523(aw != nullptr) {
    init();
}

void Button::init() {
  pinMode(buttonPin, INPUT_PULLUP);
  if (ledAw9523) {
    aw->pinMode(ledPin, OUTPUT); // set to constant current drive!
    aw->digitalWrite(ledPin, LOW);     // LED éteinte (active LOW)
  } else {
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);          // LED éteinte (active HIGH)
  }
  ledOn = false;
  lastState = NOT_PRESSED; // état initial du bouton (non appuyé)
}

ButtonState Button::state(){ // detection uniquement d'une falling edge
  ButtonState reading = static_cast<ButtonState>(digitalRead(buttonPin));

  if (reading == lastState) {
    lastDebounceTime = millis(); // reset du timer
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (lastState == NOT_PRESSED && reading == PRESSED) {
      lastState = PRESSED;
      return JUST_PRESSED; // bouton appuyé
    } else if (lastState == PRESSED && reading == NOT_PRESSED) {
      lastState = NOT_PRESSED;
      return JUST_RELEASED; // bouton relâché
    }
  }
  return lastState;
}

void Button::toggleLed(){
  if(ledOn) {
    turnOffLed();
  } else {
    turnOnLed();
  }
}

void Button::turnOnLed(){
  if (ledAw9523) {
    aw->digitalWrite(ledPin, HIGH); // LED allumée (active LOW)
  } else {
    digitalWrite(ledPin, HIGH);    // LED allumée (active HIGH)
  }
  ledOn = true;
}

void Button::turnOffLed(){
  if (ledAw9523) {
    aw->digitalWrite(ledPin, LOW); // LED éteinte (active LOW)
  } else {
    digitalWrite(ledPin, LOW);      // LED éteinte (active HIGH)
  }
  ledOn = false;
}