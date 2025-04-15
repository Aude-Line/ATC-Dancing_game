#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include <Adafruit_AW9523.h>

//si isPressed et isLED on -> étindre la led car il devait ^tre appuyé dans le jeu
//pour setup : is isPressed et isLedOn = false -> alumer la LED et envoi message
class Button {
    public:
      Button(uint8_t buttonPin, uint8_t ledPin, Adafruit_AW9523* aw = nullptr);
  
      void init();
      bool isPressed() const;
      bool isLedOn() const { return ledOn; }
      void turnOnLed();
      void turnOffLed();
  
    private:
      const uint8_t buttonPin;
      const uint8_t ledPin;
      Adafruit_AW9523* aw;    // pointeur vers un objet AW9523
      bool ledAw9523;         // true si aw != nullptr
      bool ledOn = false;
  };

  #endif