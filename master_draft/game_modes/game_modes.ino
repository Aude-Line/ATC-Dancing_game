#define led_r 4
#define led_g 5
#define led_b 6
#define led_y 7

#define potentiometer A0

const int MAXPOTENTIOMETERVAL = 1023;

//Is it possible to make a typedef of the mode ? 
const char EASY = "EASY";
const char MEDIUM = "MEDIUM";
const char HARD = "HARD";
const char difficulties[] = {EASY, MEDIUM, HARD};
const int speeds[] = {2000, 1000, 500};

// the setup routine runs once when you press reset:
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
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
This function selects randomly one of the system's LED and light it up while the other are turned off
*/
void randomLighting(){
  //Select randomly one of the LED id
  int rand_num = random(4,8);
  //Serial.println(rand_num);
  if(rand_num == 4){
    digitalWrite(led_r, HIGH);
    digitalWrite(led_g, LOW);
    digitalWrite(led_b, LOW);
    digitalWrite(led_y, LOW);
  }
  if(rand_num == 5){
    digitalWrite(led_r, LOW);
    digitalWrite(led_g, HIGH);
    digitalWrite(led_b, LOW);
    digitalWrite(led_y, LOW);
  }
  if(rand_num == 6){
    digitalWrite(led_r, LOW);
    digitalWrite(led_g, LOW);
    digitalWrite(led_b, HIGH);
    digitalWrite(led_y, LOW);
  }
  if(rand_num == 7){
    digitalWrite(led_r, LOW);
    digitalWrite(led_g, LOW);
    digitalWrite(led_b, LOW);
    digitalWrite(led_y, HIGH);
  }
}


