#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>


// Define radio module with CE, CSN pins
RF24 wireless(10, 9);  // CE, CSN pins

// Address for communication
const byte masterAddress[6] = "00001";
const byte slaveAddress[6] = "00002";

const int led_r = 4
const int led_g = 5
const int led_b = 6
const int led_y = 7


#define potentiometer A0

const int MAXPOTENTIOMETERVAL = 1023;

int leds[] = {led1, led2, led3, led4};

// Address for communication
const byte masterAddress[6] = "00001";
const byte slaveAddress[6] = "00002";


//The difficulies are represented by the interval between 2 LED lighting and the delay the player can push the button
struct Difficulty {
    int lightingInterval[2];
    unsigned long pushingDelay;
};

// Define difficulties with both values
const Difficulty EASY   = {{8000, 6000}, 4000};
const Difficulty MEDIUM = {{6000, 4000}, 3000};
const Difficulty HARD   = {{3000, 1000}, 1000};

// the setup routine runs once when you press reset:
void setup() {
  // initialize serial communication at 9600 bits per second:
Serial.begin(9600);
    // setting connection
    wireless.begin();
    wireless.setChannel(125); // Setting channels
    wireless.openWritingPipe(slaveAddress); // Master sends to Slave
    wireless.openReadingPipe(1, masterAddress);  // Master receives from Slave
    wireless.setPALevel(RF24_PA_LOW); // Set power level (low for short distances)
    wireless.stopListening(); // Master is the sender initially
    
    
    
    // set the digital pin as output:
    pinMode(led_r, OUTPUT);
    pinMode(led_g, OUTPUT);
    pinMode(led_b, OUTPUT);
    pinMode(led_y, OUTPUT);
}

// the loop routine runs over and over again forever:
void loop() {
  //Get the value of the pentotiometer
  int diffValue = analogRead(potentiometer);
  //Serial.println(modeValue);
  //int delayVal = getGameModeDelay(modeValue, 3, 200);
  //Serial.println(delayVal);
  char difficulty = getDifficulty(diffValue);

  //delay(delayVal);
}


int getGameModeDelay(int modeValue, int nbMode, int minDelay){
  int interval = MAXPOTENTIOMETERVAL/nbMode;
  for(int i = 0; i < nbMode; i++){
    if(interval*i < modeValue && modeValue <= (i+1)*interval){
      return minDelay*(i+1)*(i+1);
    }
  }
}

char getDifficulty(int diffValue){
  int nbMode = sizeof(difficulties);
  int interval = MAXPOTENTIOMETERVAL/nbMode;
  for(int i = 0; i < nbMode; i++){
    if(interval*i < diffValue && diffValue <= (i+1)*interval){
      return difficulties[i];
    }
  }
}

void difficultyAction(char difficulty){
  randomLighting();
  for(int i = 0; i < sizeof(difficulties); i++){
    if(difficulty == difficulties[i]){
        delay(speeds[i]);
    }
  }
  
}


/*
  Select randomly one of the 4 unit (button + LED) and return its index (1 to 4)
*/
int getRdmUnit(){
  //sizeof return the size of the array
  return random(0,sizeof(units) / sizeof(units[0]));

}

/*
  Wait a certain random delay influenced by the difficulty
*/
void waitLighting(int lightingInterval[2]){
  delay(random(lightingInterval[0],lightingInterval[1])); 
}


/*
  Light up the LED with the corresponding given pin 
*/
void lightUpLED(int ledIdx){
  digitalWrite(units[ledIdx].led, HIGH);
}



