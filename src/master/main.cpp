#include <Arduino.h>
#include <Adafruit_AW9523.h>
#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#include <master_pins.h>
#include <util.h>
#include <communication.h>
#include <button.h>

enum State{STOPGAME, SETUP, GAMEMODE1, GAMEMODE2};

struct PlayerStruct{
  uint8_t modules; //mask
  uint8_t nbrOfModules; //calcul dynamique avec modules
  uint16_t score;
};
struct ModuleStruct{
  int8_t playerOfModule;
  uint8_t buttonsToPress; //mask
};
PlayerStruct players[MAX_PLAYERS];
ModuleStruct modules[NBR_SLAVES];

void initPlayers(PlayerStruct* players, ModuleStruct* modules);
void sendMessage(MasterCommand command, uint8_t receivers);
void read();

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);
unsigned long lastSendTime = 0;

State actualState = STOPGAME; // Added a similar state as in slave to switch 
Button* StartButton; // Defined the start and setup button and states
Button* SetUpButton;
bool StartButtonPressed = false;
bool SetUpButtonPressed = false;

void setup() {
  Serial.begin(9600);
  Serial.println("Je suis le master !");

  // initialize the transceiver on the SPI bus
  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {}  // hold in infinite loop
  }
  // Inizialization of buttons and potentiometers
  StartButton = new Button(START_BUTTON_PIN, START_LED_PIN);
  SetUpButton = new Button (SETUP_BUTTON_PIN, SETUP_LED_PIN);
  bool SetUpButtonPressed = false;
  bool StartButtonPressed = false;

  pinMode(POTENTIOMETER_MODE_PIN, INPUT);
  pinMode(POTENTIOMETER_DIFFICULTY_PIN, INPUT);

  radio.setChannel(125);
  radio.setPALevel(RF24_PA_LOW);
  radio.setRetries(5, 15);
  radio.enableDynamicPayloads(); //as we have different payload size
  Serial.print(F("Addresses to receive: "));
  for (uint8_t i = 0; i < NBR_SLAVES; ++i){
    Serial.print(F("  Pipe "));
    Serial.print(i + 1);
    Serial.print(F(" → Adresse: 0x"));
    print64Hex(addresses[1]+i);  // affiche l'adresse en hexadécimal
    radio.openReadingPipe(i+1, addresses[1]+i); //use pipes 1-5 for reception, pipe 0 is reserved for transmission in this code
  }
  radio.startListening();  // put radio in RX mode
  lastSendTime = millis();

  initPlayers(players, modules);
}

void loop() {
  read();
  unsigned long currentTime = millis();   // Récupère le temps actuel
  // Vérifier si 2 secondes se sont écoulées
  if (currentTime - lastSendTime >= 2000) {  
    // Mettre à jour le dernier temps d'envoi
    lastSendTime = currentTime;

    // Générer une commande aléatoire entre 0 et 4 (selon les valeurs possibles de l'énum)
    //MasterCommand command = static_cast<MasterCommand>(random(0, 1));  // Random entre 0 et 4 (STOP_GAME à MISSED_BUTTONS)
    MasterCommand command = CMD_SETUP;  // Pour tester, on envoie toujours la même commande
    // Générer un masque de récepteurs aléatoire
    uint8_t receivers = random(0, (1 << NBR_SLAVES));  // Masque binaire avec NBR_SLAVES bits

    Serial.println(F("\n==========VALS COMMAND AND RECEIVERS=========="));
    // Afficher les valeurs dans le terminal pour vérifier
    Serial.print(F("Random command: "));
    Serial.println(command);
    Serial.print(F("Random receivers mask: "));
    Serial.println(receivers, BIN);  // Affiche en binaire

    // Appeler la fonction d'envoi avec la commande et les récepteurs générés
    sendMessage(command, receivers);
  }
  uint8_t gameMode = 0;
  uint8_t receivers = 0;
  // The actual game logic
  if (SetUpButton->isPressed()){
    // If the setupbutton has been pressed enter the setupmode
    actualState = SETUP;
    SetUpButtonPressed = true;
  }

  if (StartButton->isPressed() & SetUpButtonPressed){
    // If a start button is pressed after entering setup mode
    gameMode = map(analogRead(POTENTIOMETER_MODE_PIN), 0, 1023, 1, 2);
    SetUpButtonPressed = false; // We give the possibility of changing again the setup
    // Entering the mode according to the selected one
    switch(gameMode){
      case 1: {
        actualState = GAMEMODE1;
        break;
      }
      case 2: {
        actualState = GAMEMODE2;
        break;
      }
    }
  }

  switch(actualState){
    case SETUP:{
      receivers = (1 << NBR_SLAVES)-1; // Setting 1 to all slaves
      sendMessage(CMD_SETUP,receivers); // Telling all the slaves to enter setup mode
      read();
    }
    case STOPGAME: {}
    case GAMEMODE1: {}
    case GAMEMODE2: {}
    default: {}
  }
}

void sendMessage(MasterCommand command, uint8_t receivers){
  PayloadFromMasterStruct payloadFromMaster;
  radio.stopListening();
  for (uint8_t slave = 0; slave < NBR_SLAVES; ++slave){
    if(receivers & (1 << slave)){
      //creation du payload
      /* caché pour l'instant car mock comm
      payloadFromMaster.command = command;
      payloadFromMaster.buttonsToPress = modules[slave].buttonsToPress;
      if(modules[slave].playerOfModule != NONE){
        payloadFromMaster.score = players[modules[slave].playerOfModule].score;
      }else{
        payloadFromMaster.score = 0;
      }
      */
      // Création d'un payload aléatoire pour tester
      payloadFromMaster.command = command;  // Commande aléatoire entre 0 et 5 (en fonction de ton enum)
      payloadFromMaster.buttonsToPress = random(0, 16);  // Valeur aléatoire entre 0 et 15 pour les 4 boutons
      payloadFromMaster.score = random(0, 2);

      //début de la com
      radio.openWritingPipe(addresses[0]+slave);
      unsigned long start_timer = micros();                  // start the timer
      bool report = radio.write(&payloadFromMaster, sizeof(payloadFromMaster));  // transmit & save the report
      unsigned long end_timer = micros();                    // end the timer

      // Affichage complet
      Serial.println(F("\n==========NEW TRANSMISSION=========="));
      Serial.print(F("Slave index: "));
      Serial.println(slave);
      Serial.print(F("Writing pipe address: 0x"));
      print64Hex(addresses[1]+slave);

      printPayloadFromMasterStruct(payloadFromMaster);
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

// I changed the type if we want to store the message received
void read(){
  uint8_t pipe;
  PayloadFromSlaveStruct payloadFromSlave;
  if (radio.available(&pipe)) {              // is there a payload? get the pipe number that received it
    uint8_t bytes = radio.getDynamicPayloadSize();  // get the size of the payload
    radio.read(&payloadFromSlave, bytes);             // fetch payload from FIFO

    Serial.println(F("\n==========NEW RECEPTION=========="));
    Serial.print(F("From slave "));
    Serial.println(pipe-1);
    printPayloadFromSlaveStruct(payloadFromSlave);
    return payloadFromSlave;
  }
}

void initPlayers(PlayerStruct* players, ModuleStruct* modules){
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    players[i].modules = 0;
    players[i].nbrOfModules=0; //calcul dynamique avec modules
    players[i].score=0;
  }
  for (uint8_t i = 0; i < NBR_SLAVES; i++){
    modules[i].playerOfModule = -1; //NONE
    modules[i].buttonsToPress = 0;
  }
}