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
  lastState = HIGH; // état initial du bouton (non appuyé)
}

bool Button::isPressed(){ // detection uniquement d'une falling edge
  bool reading = digitalRead(buttonPin);

  if (reading == lastState) {
    lastDebounceTime = millis(); // reset du timer
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    //Serial.println("Debounce time écoulé !");
    if (lastState == HIGH && reading == LOW) {
      lastState = LOW;
      return true;
    } else if (lastState == LOW && reading == HIGH) {
      lastState = HIGH;
      return false; // bouton relâché
    }
  }

  return false;
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