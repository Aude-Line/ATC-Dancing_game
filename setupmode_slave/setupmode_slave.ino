#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

const int buttonPins[] = {4, 5, 6, 7};  // Array of button pin numbers (Yellow, Green, Blue, Red)

RF24 wireless(7, 8); // Set accordingly to branching
const byte addresses[][6] = {"00001","00002"};

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

struct Payload {
  uint8_t commandFlags;
  int Data;
};

Payload receivedData;
Payload dataToSend;

const int deviceID = 1; // device ID



void setup() {
  // At first they need to be input
  for (int i = 0; i < 4; i++){
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  Serial.begin(9600);
  wireless.begin();
  wireless.setChannel(125);
  wireless.openWritingPipe(addresses[1]);
  wireless.openReadingPipe(1, addresses[0]); // Address of transmitter and data pipe 
  wireless.setPALevel(RF24_PA_LOW);
  wireless.startListening();
}

void loop() {
  if (wireless.available()) {
    wireless.read(&receivedData, sizeof(receivedData));
    bool shouldAct =  receivedData.commandFlags & (1 << deviceID);  // This checks if bit of my device is set to 1 

    if (shouldAct) {
      if (receivedData.Data == ENTER_SETUP) {  // ENTER_SETUP command
        enterSetupMode();
      }
  }
}}

void enterSetupMode() {
  Serial.println(" Entered SetUp Mode");
  Serial.println("Select your color");



  while(true){
  for (int i = 0; i < 4; i++) {
  if (digitalRead(buttonPins[i]) == LOW) {
    assignColor(static_cast<PlayerColor>(i+1)); // convert button index to Color Enum
    break;  // Exit the loop once a color is selected
  }
  }}

}

void assignColor(PlayerColor color) { 
  // Print out the selected color
  Serial.print("Selected Color: ");
  switch (color) {
    case YELLOW:
      Serial.println("Yellow");
      break;
    case GREEN:
      Serial.println("Green");
      break;
    case BLUE:
      Serial.println("Blue");
      break;
    case RED:
      Serial.println("Red");
      break;
    default:
      Serial.println("Unknown Color");
      break;
  }
  struct SetUpMessage {
    uint8_t commandFlags;
    PlayerColor color; // Selected color or none
  };
  // Send the selected color to the master (for example, as an integer or enum value)
  SetUpMessage colorData;
  colorData.commandFlags = B0000001; 
  colorData.color = color; // send the enum values per each color

  wireless.stopListening();
  bool success = wireless.write(&colorData, sizeof(colorData));  // Send color to master
  wireless.startListening();  // Start listening again

  if (success) {
    Serial.println("Color sent to master successfully!");
  } else {
    Serial.println("Failed to send color.");
  }
}





