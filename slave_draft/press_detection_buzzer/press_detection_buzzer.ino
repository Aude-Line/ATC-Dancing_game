int buzzer = 7;
int buttonpin = 6; 
int LedButton = 5;
 //buzzer to arduino pin 9


void setup(){
  pinMode(buttonpin, INPUT); 
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output
  
  pinMode(LedButton, OUTPUT);
}

void   loop(){
  delay(random(2000,5000)); // wait a random time between 2 and 5 sec
  digitalWrite(LedButton, HIGH);
  unsigned long startTime = millis();
  bool buttonPressed = false;
  
  while (millis()-startTime < 2000) {
    if (digitalRead(buttonpin) == HIGH) { 
      buttonPressed = true;
    }
  }

  digitalWrite(LedButton, LOW);

  if(buttonPressed) {
    tone(buzzer,2000,200);
    delay(250);
    tone(buzzer,2000,300);
  } else {
    tone(buzzer, 100, 500);
  }


}
