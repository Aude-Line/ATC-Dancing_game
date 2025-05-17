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
  lastState = static_cast<ButtonState>(digitalRead(buttonPin)); // état initial du bouton
  currentState = lastState;
}

void Button::updateState(){
  lastState = static_cast<ButtonState>(currentState%2); // to get only PRESSED or NOT_PRESSED
  ButtonState reading = static_cast<ButtonState>(digitalRead(buttonPin));

  if(lastState == NOT_PRESSED && reading == PRESSED){
    currentState = JUST_PRESSED;
  }else if(lastState == PRESSED && reading == NOT_PRESSED){
    currentState = JUST_RELEASED;
  }else{
    currentState = reading;
  }
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