int buzzer1 = 2;
int button1 = 3; 
int ledButton1 = 4;

int buzzer2 = 7;
int button2 = 6; 
int ledButton2 = 5;

int buzzer3 = 8;
int button3 = 9; 
int ledButton3 = 10;

int buzzer4 = 11;
int button4 = 12; 
int ledButton4 = 13;

int ledButtons[] = {ledButton1, ledButton2, ledButton3, ledButton4};

enum Difficulties {EASY, MEDIUM, HARD};

void setup(){
  initAllPins();
}

void loop(){
  int modules[] = {2,4};
  int module = getRandomModule(modules);
  randomLighting(module);
  unsigned long startTime = millis();
  bool buttonPressed = false;

  while (millis()-startTime < 2000) {
    if (digitalRead(button2) == LOW) { 
      buttonPressed = true;
      //As soon as the button is pressed, the light turn off and the buzzer right song is player
      //This prevent unecessary waiting time
      break;
    }
  }


  digitalWrite(ledButton2, LOW);

  if(buttonPressed) {
    tone(buzzer2,2000,200);
    delay(250);
    tone(buzzer2,2000,300);
  } else {
    tone(buzzer2, 100, 500);
  }

}


/* 
  Initialise the different pins used by the system
*/
void initAllPins(){
  initPin(buzzer1, button1, ledButton1);
  initPin(buzzer2, button2, ledButton2);
  initPin(buzzer3, button3, ledButton3);
  initPin(buzzer4, button4, ledButton4);
}

void initPin(int buzzer, int button, int ledbutton){
  pinMode(buzzer, OUTPUT);
  //We use the inbuilt pullup resistor to spare some resistors (see the documentation for more information)
  pinMode(button, INPUT_PULLUP); 
  pinMode(ledbutton, OUTPUT);
}


int getRandomModule(int modules[]){
  return modules[random(0,sizeof(modules) + 1)];

}


void lightButton(int module){
  for(int ledButton: ledButtons){
    //The module doesn't correspond to the LED numbers
    if(ledButton == module){
      digitalWrite(ledButton, HIGH);
    }
    else{
      digitalWrite(ledButton, HIGH);
    }
  }
}

/* 
  Wait a difficulty depending random time until lighting the giving light
*/
void randomLighting(int module){
  //Change the delay depending on the difficulty
  delay(random(2000,5000)); // wait a random time between 2 and 5 sec

  lightButton(module);
}

