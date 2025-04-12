/**
* Master code to test the skeleton of the code (communication and threads)
*/

#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#define NBR_SLAVES 3 //MAX 5 or else we can no longer use the multiceiver function (pipe 0 is reserved for transmission, pipes 1-5 reception)
#define MAX_PLAYERS 4 //Ne pas modifier pour utiliser toutes les couleurs
#define CE_PIN 9
#define CSN_PIN 10

enum Colors{RED, GREEN, BLUE, YELLOW}; //jsp si besoin comme je suis partie du principe que on peut envoyer potentiellemnt plusieurs boutons à appuyer
enum Player{NONE=-1, PLAYER1, PLAYER2, PLAYER3, PLAYER4};
enum State{STOP_GAME, SETUP, GAME};
enum CommandsFromMaster{STOP_GAME, SETUP, BUTTONS, SCORE, MISSED_BUTTONS};

struct PlayerStruct{
  uint8_t modules; //mask
  uint8_t nbrOfModules; //calcul dynamique avec modules
  uint16_t score;
};
struct ModuleStruct{
  Player playerOfModule;
  uint8_t buttonsToPress; //mask
};
PlayerStruct players[MAX_PLAYERS];
ModuleStruct modules[NBR_SLAVES];


struct PayloadFromMasterStruct{
  CommandsFromMaster command;
  uint8_t buttonsToPress;
  uint16_t score;
};
struct PayloadFromSlaveStruct{
  Player idPlayer; //maybe not needed but can allow to adjust communication if there was a setup missed
  uint8_t buttonsPressed;
};

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);

/*
* reference values of the tx and rx channels
* they will be dynamically attributed for each slave, max 6 slaves
* exemple: slave 3 will use the adresses 5A36484133 and 5448344433
*/
const uint64_t addresses[2] = { 0x5A36484130LL,
                                0x5448344430LL};

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    // some boards need to wait to ensure access to serial over USB
  }
  delay(1500); //juste pour que le terminal ne commence pas à lire avant et qu'il y ait des trucs bizarres dedans, jsp si c'est ok normalement

  radio.begin();
  radio.setChannel(125);
  radio.setPALevel(RF24_PA_LOW);
  Serial.print(F("Addresses to receive: "));
  for (uint8_t i = 0; i < NBR_SLAVES; ++i){
    Serial.print(F("  Pipe "));
    Serial.print(i + 1);
    Serial.print(F(" → Adresse: 0x"));
    print64Hex(addresses[1]+i);  // affiche l'adresse en hexadécimal
    radio.openReadingPipe(i+1, addresses[1]+i); //use pipes 1-5 for reception, pipe 0 is reserved for transmission in this code
  }
  radio.startListening();  // put radio in RX mode

  initPlayers(players, modules);
}

void loop() {
  //print64Hex(addresses[1]);
  //print64Hex(addresses[1]+1);
  //delay(1000);
}

void print64Hex(uint64_t val) {
  uint32_t high = (uint32_t)(val >> 32);  // Poids fort
  uint32_t low = (uint32_t)(val & 0xFFFFFFFF); // Poids faible

  Serial.print(high, HEX);
  Serial.println(low, HEX);
}

void sendMessage(CommandsFromMaster command, uint8_t receivers){
  PayloadFromMasterStruct payloadFromMaster;
  radio.stopListening();
  for (uint8_t slave = 0; slave < NBR_SLAVES; ++slave){
    if(receivers & (1 << slave)){
      //creation du payload
      payloadFromMaster.command = command;
      payloadFromMaster.buttonsToPress = modules[slave].buttonsToPress;
      if(modules[slave].playerOfModule != NONE){
        payloadFromMaster.score = players[modules[slave].playerOfModule].score;
      }else{
        payloadFromMaster.score = 0;
      }

      //début de la com
      radio.openWritingPipe(addresses[1]+slave);¨
      unsigned long start_timer = micros();                  // start the timer
      bool report = radio.write(&payloadFromMaster, sizeof(payloadFromMaster));  // transmit & save the report
      unsigned long end_timer = micros();                    // end the timer

      // Affichage complet
      Serial.println(F("\n==========NEW TRANSMISSION=========="));
      Serial.print(F("Slave index: "));
      Serial.println(slave);
      Serial.print(F("Writing pipe address: 0x"));
      print64Hex(addr);

      Serial.println(F("Payload content:"));
      Serial.print(F("  Command: "));
      Serial.println(payloadFromMaster.command);
      Serial.print(F("  ButtonsToPress: "));
      Serial.println(payloadFromMaster.buttonsToPress);
      Serial.print(F("  Score: "));
      Serial.println(payloadFromMaster.score);

      if (report) {
        Serial.print(F("✅ Transmission successful in "));
        Serial.print(end_timer - start_timer);
        Serial.println(F(" µs"));
      } else {
        Serial.println(F("❌ Transmission failed"));
      }
    }
  }
  radio.startListening();  // put radio in RX mode
}

void initPlayers(PlayerStruct* players, ModuleStruct* modules){
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    players[i].modules = 0;
    players[i].nbrOfModules=0; //calcul dynamique avec modules
    players[i].score=0;
  }
  for (uint8_t i = 0; i < NBR_SLAVES; i++){
    modules[i].playerOfModule = NONE;
    modules[i].buttonsToPress = 0;
  }
}