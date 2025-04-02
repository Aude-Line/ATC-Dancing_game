int button1 = 3;
int button2 = 4;
int button3 = 5;
int button4 = 6;

int buzzer = 9;

//Momentary implementation of pin connected LEDs instead of LED strips connected bc I2C
int led1 = 10;
int led2 = 11;
int led3 = 12; 
int led4 = 13;

//Vector containing all the button used in a module
int buttons[] = {button1, button2, button3, button4};

int leds[] = {led1, led2, led3, led4};

//Each difficulty is associated with the time (s) between two lightings
struct Difficulty {
    int lightingInterval[2];
    unsigned long pushingDelay;
};

// Define difficulties with both values
const Difficulty EASY   = {{8000, 6000}, 4000};
const Difficulty MEDIUM = {{6000, 4000}, 3000};
const Difficulty HARD   = {{3000, 1000}, 1000};

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
  initButtons();
  initLeds();
  pinMode(buzzer, OUTPUT);
}


/*
  Initialise all the buttons of the system as input pin with integrated pullup resistor enable 

  NOTE: the buttons inialisation is based on the global variable "buttons", modify this global variables if less buttons are used
*/
void initButtons(){
  for(int button: buttons){
      pinMode(button, INPUT_PULLUP); 
  }
}

/*
  Initialise all the leds of the system as output pin 

  NOTE: Must be replaced by the LED strip new version
*/
void initLeds(){
  for(int led: leds){
    pinMode(led, OUTPUT);
  }
}


/*
  Select randomly one of the 4 unit (button + LED) and return its index (1 to 4)

  NOTE: Mock function of the master
*/
int MOCKgetRdmUnit(){
  //sizeof return the size of the array
  return random(0,sizeof(leds) / sizeof(leds[0]));

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
void lightUpLED(int ledIdx){
  digitalWrite(leds[ledIdx], HIGH);
}


/*
  Determine if the correct button has been pressed on time, depending on the pushing difficulty
*/
bool isButtonPressed(int unitIdx, unsigned long pushingDelay){
  bool buttonPressed = false;
  unsigned long startTime = millis();
  //The player can pressed on the button only for a few delay determined by the difficulty
  while (millis()-startTime < pushingDelay) {
    if (digitalRead(buttons[unitIdx]) == LOW) { 
      buttonPressed = true;
      //As soon as the button is pressed, the light turn off and the buzzer right song is player
      //This prevent unecessary waiting time
      break;
    }
  }

  digitalWrite(leds[unitIdx], LOW);

  return buttonPressed;
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

