/* Master set up operation*/
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Defining the Buttons
const int StartButton = 2; // Select correct pin
const int SetUpButton = 3; // Select correct pin. Which one is the one that stays down??

// Defining the Potentiometers
const int potPlayerPin = A0;
const int potDifficultyPin = A1;
const int potModePin = A2;

// Defining the buttons states
bool StartState = false;
bool SetUpState = false;

// Defining the CSN and CE pins and the address pipes
RF24 wireless(10, 9);
const byte addresses[][6] = {"00001", "00002"};

// Enum for clarity
enum PlayerColor : uint8_t { 
  NONE=0, 
  RED, 
  GREEN, 
  BLUE, 
  YELLOW 
};

enum MasterCommand : uint8_t {
  NO_COMMAND = 0,
  ENTER_SETUP = 1,
  START_GAME = 2
};


// Defining the structure of the data for flagged communication
struct Payload {
  uint8_t commandFlags;
  int Data;
};
Payload datatoSend;

// Defining the Master ID
const int deviceID = 0;


void setup() {
  pinMode(StartButton, INPUT_PULLUP);
  pinMode(SetUpButton, INPUT_PULLUP);
  Serial.begin(9600);

  wireless.begin();
  wireless.setChannel(125);
  wireless.openWritingPipe(addresses[0]); // Address of Transmitter which will send data
  wireless.openReadingPipe(1, addresses[1]);
  wireless.setPALevel(RF24_PA_LOW);
  wireless.stopListening();

  Serial.println("Ready. Press SETUP to begin");

}

void loop() {
  SetUpState = digitalRead(SetUpButton) == LOW;
  if (SetUpState) {
    SetUpMode();
  }

}

void SetUpMode () {
  Serial.println("=== ENTERING SETUP MODE ===");


  // Communicate with the slaves
  datatoSend.commandFlags = B11111111; // All slaves should listen
  datatoSend.Data = ENTER_SETUP;

  // Send the message to all slaves
 
  wireless.stopListening();  // Stop listening before sending
  bool success = wireless.write(&datatoSend, sizeof(datatoSend));
  Serial.println(success ? "Setup signal sent" : "Failed to send");
  wireless.startListening();  // Go back to listening for slave replies
  
  int players = 1;
  int difficulty = 0;
  int gameMode = 0;

  wireless.startListening(); // To enable receving from slaves

  // Messages coming from slaves to set up color of players
  struct SetUpMessage {
    uint8_t commandFlags;
    PlayerColor color; // Selected color or none
  };

  struct BoardAssignment {
    bool active = false;
    PlayerColor color = NONE;
  };

  BoardAssignment boardAssignments[2]; // Will be up to 4, rigyht now 2



  while (true) {
    // Read Potentiometers
    players = map(analogRead(potPlayerPin), 0, 1023, 1, 4);
    difficulty = map(analogRead(potDifficultyPin), 0, 1023, 1, 10); // How many difficulties possibility do we want??
    gameMode = map(analogRead(potModePin), 0, 1023, 1, 4);

    // Check if slave has sent an update
    if (wireless.available()) {
      SetUpMessage msg;
      wireless.read(&msg, sizeof(msg)); // we need to add that the mastr will read only if his bit is turned on
      bool shouldAct =  msg.commandFlags & (1 << deviceID); // Is it for me?
      if (shouldAct) {
        // Checks that the board ID received is valid
        if (msg.commandFlags < 4) {
          // Creating a reference
          BoardAssignment &assignment = boardAssignments[msg.commandFlags];

          // The case in which I press a green button twice for example
          if (assignment.active && assignment.color == msg.color) {
            assignment.active = false;
            assignment.color = NONE;
            Serial.print( "Board ");
            Serial.print(msg.commandFlags);
            Serial.println(" deselected");
          } else {
            // Assign new color
            assignment.active = true;
            assignment.color = msg.color;
            Serial.print ("Board ");
            Serial.print(msg.commandFlags);
            Serial.print(" set to 2");
            printColorName(msg.color);
          }
        }
        }

        // Print current selection as Feedback
        Serial.print("Players: ");
        Serial.print(players);
        Serial.print(" | Difficulty: ");
        Serial.print(difficulty);
        Serial.print(" | Game Mode: ");
        Serial.println(gameMode);

        
        for (int i = 0; i < 2; i++) { // could be also 4 when we have more boards
          if (boardAssignments[i].active) {
            Serial.print("Board "); Serial.print(i);
            Serial.print(" => ");
            printColorName(boardAssignments[i].color);
          }
        }


        Serial.println("Press START to finalize");
        delay(300); // we will not have delays is only for the serial print

        
        // Check for Start button press and exit in case
        StartState = digitalRead(StartButton) == LOW;
        if (StartState) {
          Serial.println("=== SETUP COMPLETE ===");
          Serial.print("Selected Players: "); Serial.println(players);
          Serial.print("Selected Difficulty: "); Serial.println(difficulty);
          Serial.print("Selected Mode: "); Serial.println(gameMode);
          wireless.stopListening();

          // Will the master have to send something to the slaves?


          break; //Exit setupmode}
        }
      }
  }  
}

void printColorName( PlayerColor color) {
  switch (color) {
    case RED: Serial.println("RED"); break;
    case YELLOW: Serial.println("YELLOW"); break;
    case BLUE: Serial.println("BLUE"); break;
    case GREEN: Serial.println("GREEN"); break;
    default: Serial.println("NONE"); break;

  }
}