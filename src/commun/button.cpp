#include "button.h"

Button::Button(uint8_t buttonPin, uint8_t ledPin, Adafruit_AW9523* aw)
  : buttonPin(buttonPin), ledPin(ledPin), aw(aw), ledAw9523(aw != nullptr) {
    init();
  }

void Button::init() {
  pinMode(buttonPin, INPUT_PULLUP);
  if (ledAw9523) {
    aw->pinMode(ledPin, AW9523_LED_MODE); // set to constant current drive!
    aw->analogWrite(ledPin, 0);     // LED éteinte (active LOW)
  } else {
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);          // LED éteinte (active HIGH)
  }
  ledOn = false;
  lastState = HIGH; // état initial du bouton (non appuyé)
}

bool Button::isPressed(){ // detection uniquement d'une falling edge
  if(lastState == LOW && digitalRead(buttonPin) == HIGH) {
    lastState = HIGH; // bouton relâché
    return false;
  } else if(lastState == HIGH && digitalRead(buttonPin) == LOW) {
    lastState = LOW; // bouton appuyé
    return true;
  }
  return false; // pas de changement d'état
}

void Button::turnOnLed(){
  if (ledAw9523) {
    aw->analogWrite(ledPin, 255); // LED allumée (active LOW)
  } else {
    digitalWrite(ledPin, HIGH);    // LED allumée (active HIGH)
  }
  ledOn = true;
}

void Button::turnOffLed(){
  if (ledAw9523) {
    aw->analogWrite(ledPin, 0); // LED éteinte (active LOW)
  } else {
    digitalWrite(ledPin, LOW);      // LED éteinte (active HIGH)
  }
  ledOn = false;
}