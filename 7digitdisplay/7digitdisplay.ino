#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_7segment matrix = Adafruit_7segment();

void setup() {
  // put your setup code here, to run once:
#ifndef __AVR_ATtiny85__
  Serial.begin(9600);
  Serial.println("7 Segment Backpack Test");
#endif
  matrix.begin(0x70);
}

void loop() {
  // put your main code here, to run repeatedly:
    // print with print/println
  for (uint16_t counter = 0; counter < 9999; counter++) {
    matrix.println(counter);
    matrix.writeDisplay();
    delay(10);
  }
    // print a floating point 
  matrix.print(12.34);
  matrix.writeDisplay();
  delay(500);

  // print a string message
  matrix.print("7SEG");
  matrix.writeDisplay();
  delay(10000);
}
