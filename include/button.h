#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include <Adafruit_AW9523.h>

enum ButtonState {
  PRESSED = 0,       // LOW
  NOT_PRESSED = 1,    // HIGH
  JUST_PRESSED = 2,
  JUST_RELEASED = 3
};

class Button {
    public:
      Button(uint8_t buttonPin, uint8_t ledPin, Adafruit_AW9523* aw = nullptr);
  
      void init();
      void updateState(); //Update the state of the button
      ButtonState getState() const {return currentState;} //Get the current state of the button, updateState needs to be called before to have the right state

      bool isLedOn() const { return ledOn; }
      void toggleLed();
      void turnOnLed();
      void turnOffLed();
 
  
    private:
      const uint8_t buttonPin;
      const uint8_t ledPin;
      Adafruit_AW9523* aw;    // pointeur vers un objet AW9523
      bool ledAw9523;         // true si aw != nullptr
      bool ledOn = false;
      ButtonState lastState = NOT_PRESSED;
      ButtonState currentState = NOT_PRESSED;
  };

  #endif