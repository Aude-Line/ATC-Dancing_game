#include <Arduino.h>

#ifndef SLAVE_ID
#define SLAVE_ID 0
#endif

void setup() {
  Serial.begin(9600);
  Serial.print("Je suis le slave ID : ");
  Serial.println(SLAVE_ID);
}

void loop() {
  // comportement selon SLAVE_ID
}