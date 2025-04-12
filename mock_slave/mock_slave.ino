/**
* Master code to test the skeleton of the code (communication and threads)
*/

#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#define ID_SLAVE 1 //Unique and in [0, NBR_SLAVES[ (defined in the master code)
#define CE_PIN 9
#define CSN_PIN 10

enum Colors{RED, GREEN, BLUE, YELLOW}; //jsp si besoin comme je suis partie du principe que on peut envoyer potentiellemnt plusieurs boutons à appuyer
enum Player : int8_t {NONE=-1, PLAYER1, PLAYER2, PLAYER3, PLAYER4}; //forcer à un int8_t pour réduire le payload
enum State{STOP_GAME, SETUP, GAME};
enum CommandsFromMaster{STOP_GAME, SETUP, BUTTONS, SCORE, MISSED_BUTTONS};

struct PayloadFromMasterStruct{
  CommandsFromMaster command;
  uint8_t buttonsToPress;
  uint16_t score;
};
struct PayloadFromSlaveStruct{
  Player idPlayer; //maybe not needed but can allow to adjust communication if there was a setup missed
  bool buttonsPressed; //en mode le/les bons boutons ont tous été appuyés
};

// instantiate an object for the nRF24L01 transceiver
RF24 radio(CE_PIN, CSN_PIN);

/*
* reference values of the rx and tx channels
* they will be dynamically attributed for each slave, max 6 slaves
* exemple: slave 3 will use the adresses 5A36484133 and 5448344433
*/
const uint64_t addresses[2] = { 0x5A36484130LL,
                                0x5448344430LL};

uint16_t time = 0;
Player idPlayer = PLAYER1;

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    // some boards need to wait to ensure access to serial over USB
  }

  delay(1500); //juste pour que le terminal ne commence pas à lire avant et qu'il y ait des trucs bizarres dedans, jsp si c'est ok normalement
  Serial.print("address to send: ");
  print64Hex(addresses[1] + ID_SLAVE);
  Serial.print("address to receive: ");
  print64Hex(addresses[0] + ID_SLAVE);
  
  // initialize the transceiver on the SPI bus
  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {}  // hold in infinite loop
  }
  radio.setChannel(125);
  radio.setPALevel(RF24_PA_LOW);
  radio.enableDynamicPayloads(); //as we have different payload size
  radio.openWritingPipe(addresses[1]+ID_SLAVE);
  radio.openReadingPipe(1, addresses[0]+ID_SLAVE);
  radio.startListening();

  // Copied from MulticeiverDemo code, need adaptation
  //According to the datasheet, the auto-retry features's delay value should
  // be "skewed" to allow the RX node to receive 1 transmission at a time.
  // So, use varying delay between retry attempts and 15 (at most) retry attempts
  //radio.setRetries(((role * 3) % 12) + 3, 15);  // maximum value is 15 for both args

  time = random(0, 1500);
}

void loop() {
  read();

  unsigned long currentTime = millis();   // Récupère le temps actuel
  if (currentTime - lastSendTime >= time) {  
    // Mettre à jour le dernier temps d'envoi
    lastSendTime = currentTime;

    bool trueButtons = random(0, 2);  // Random entre 0 et 1
    // Afficher les valeurs dans le terminal pour vérifier
    Serial.print(F("Random trueButtons: "));
    Serial.println(trueButtons);
    Serial.print(F("Player ID: "));
    Serial.println(idPlayer);  // Affiche en binaire

    // Appeler la fonction d'envoi avec la commande et les récepteurs générés
    sendMessage(trueButtons);
  }
}

void sendMessage(bool buttonsPressed){
  PayloadFromSlaveStruct payloadFromSlave;
  
  payloadFromSlave.idPlayer = idPlayer;
  payloadFromSlave.buttonsPressed = buttonsPressed;

  //début com 
  radio.stopListening();
  unsigned long start_timer = micros();                  // start the timer
  bool report = radio.write(&payloadFromSlave, sizeof(payloadFromSlave));  // transmit & save the report
  unsigned long end_timer = micros();                    // end the timer
  radio.startListening();  // put radio in RX mode

  Serial.println(F("Payload content:"));
  Serial.print(F("  PlayerID: "));
  Serial.println(payloadFromSlave.idPlayer);
  Serial.print(F("  Buttons pressed correctly: "));
  Serial.println(payloadFromSlave.buttonsPressed);

  if (report) {
    Serial.print(F("✅ Transmission successful in "));
    Serial.print(end_timer - start_timer);
    Serial.println(F(" µs"));
  } else {
    Serial.println(F("❌ Transmission failed"));
  }
}

void read(){
  PayloadFromMasterStruct payloadFromMaster;
  if (radio.available()) {              // is there a payload? get the pipe number that received it
    uint8_t bytes = radio.getDynamicPayloadSize();  // get the size of the payload
    radio.read(&payloadFromMaster, bytes);             // fetch payload from FIFO

    Serial.println(F("\n==========NEW RECEPTION=========="));

    Serial.println(F("Payload content:"));
    Serial.print(F("  Command: "));
    Serial.println(payloadFromMaster.command);
    Serial.print(F("  ButtonsToPress: "));
    Serial.println(payloadFromMaster.buttonsToPress);
    Serial.print(F("  Score: "));
    Serial.println(payloadFromMaster.score);
  }
}

void print64Hex(uint64_t val) {
  uint32_t high = (uint32_t)(val >> 32);  // Poids fort
  uint32_t low = (uint32_t)(val & 0xFFFFFFFF); // Poids faible

  Serial.print("0x");
  Serial.print(high, HEX);
  Serial.println(low, HEX);
}
