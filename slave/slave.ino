const int button1 = 3,
          button2 = 4,
          button3 = 5,
          button4 = 6;

//Momentary implementation of pin connected LEDs instead of LED strips connected bc I2C
const int led1 = 10, 
          led2 = 11, 
          led3 = 12, 
          led4 = 13;

const int buzzer = 9;

//A button unit is characterized by a button and a led
struct ButtonUnit{
  const int button; 
  const int led;
};

const ButtonUnit unit1 = {button1, led1};
const ButtonUnit unit2 = {button2, led2};
const ButtonUnit unit3 = {button3, led3};
const ButtonUnit unit4 = {button4, led4};

//Define a general vector containg all the system unit
ButtonUnit units[] = {unit1, unit2, unit3, unit4};

int score = 0;

//The difficulies are represented by the interval between 2 LED lighting and the delay the player can push the button
struct Difficulty {
    int lightingInterval[2];
    unsigned long pushingDelay;
};

// Define difficulties with both values
const Difficulty EASY   = {{8000, 6000}, 4000};
const Difficulty MEDIUM = {{6000, 4000}, 3000};
const Difficulty HARD   = {{3000, 2000}, 2000};

void setup(){
  initAllPins();
}

void loop(){
  Difficulty currentDiff = HARD;
  MOCKwaitLighting(currentDiff.lightingInterval);
  //Select randomly one of the LED to light it up
  int unitIdx = MOCKgetRdmUnit();
  lightUpLED(unitIdx);
  bool buttonPressed = isButtonPressed(unitIdx,currentDiff.pushingDelay);
  auditiveFB(buttonPressed);
}


/* 
  Initialise the different pins used by the system
*/
void initAllPins(){
  initUnit();
  pinMode(buzzer, OUTPUT);
}

/* 
  Initialise all the units of the system 
*/
void initUnit(){
  for(ButtonUnit unit : units){
      //To spare some resistors for the butons, we used the pin in build pull up resistor
      pinMode(unit.button, INPUT_PULLUP); 
      pinMode(unit.led, OUTPUT);
  }
}

/*
  Select randomly one of the 4 unit (button + LED) and return its index (1 to 4)

  NOTE: Mock function of the master
*/
int MOCKgetRdmUnit(){
  //sizeof return the size of the array
  return random(0,sizeof(units) / sizeof(units[0]));

}

/*
  Wait a certain random delay influenced by the difficulty
*/
void MOCKwaitLighting(int lightingInterval[2]){
  delay(random(lightingInterval[0],lightingInterval[1])); 
}


/*
  Light up the LED with the corresponding given pin 
*/
void lightUpLED(const int ledIdx){
  digitalWrite(units[ledIdx].led, HIGH);
}


/*
  Determine if the correct button has been pressed on time, depending on the pushing difficulty

  NOTE: MAKE A WRONG SOUND IF WRONG BUTTON PRESSED -> REMOVE WHILE
*/
bool isButtonPressed(const int unitIdx, unsigned long pushingDelay){
  unsigned long startTime = millis();
  //The player can pressed on the button only for a few delay determined by the difficulty
  while (millis()-startTime < pushingDelay) {
    // Check if the correct button is pressed
    if (digitalRead(units[unitIdx].button) == LOW) { 
      digitalWrite(units[unitIdx].led, LOW); // I would add this here not outside the couple
      //As soon as the button is pressed, the light turn off and the buzzer right song is player
      //This prevent unecessary waiting time
      return true;
    }

    // Check if any other button is pressed (wrong button)
    for (int i = 0; i < 4; i++) {
      if (i != unitIdx && digitalRead(units[i].button) == LOW) {
        digitalWrite(units[unitIdx].led, LOW);
        return false;
      }
    }

  }
  digitalWrite(units[unitIdx].led, LOW);
  return false;
}



/* 
  Provide the appropriate auditive feedback depending on the player performance
*/
void auditiveFB(bool buttonPressed){
  if(buttonPressed) {
    tone(buzzer,2000,200);
    delay(250);
    tone(buzzer,2000,300);
  } else {
    tone(buzzer, 100, 500);
  }
}

