#ifndef COMMUN_H
#define COMMUN_H

#include <Arduino.h>
#include <Adafruit_AW9523.h>

#define MAX_PLAYERS 4 //Ne pas modifier pour utiliser toutes les couleurs

extern const uint64_t addresses[2]; // adresses utilisées pour la communication entre le master et les slaves, definies dans le .cpp

enum Colors{
    RED, 
    GREEN, 
    BLUE, 
    YELLOW
}; //jsp si besoin comme je suis partie du principe que on peut envoyer potentiellemnt plusieurs boutons à appuyer

enum MasterCommand : uint8_t {
    CMD_STOP_GAME,
    CMD_SETUP,
    CMD_BUTTONS,
    CMD_SCORE,
    CMD_MISSED_BUTTONS
};

struct PayloadFromMasterStruct{
  MasterCommand command;
  uint8_t buttonsToPress; //Masque de bits pour savoir quels boutons appuyer (0x01 = bouton 1, 0x02 = bouton 2, 0x04 = bouton 3, 0x08 = bouton 4)
  uint16_t score;
};
struct PayloadFromSlaveStruct{
  int8_t idPlayer; //needed for setup and adjustment
  bool buttonsPressed; //en mode le/les bons boutons qui devaient être appuyés ont tous été appuyés
};

//si isPressed et isLED on -> étindre la led car il devait ^tre appuyé dans le jeu
//pour setup : is isPressed et isLedOn = false -> alumer la LED et envoi message
class Button {
  public:
    Button(uint8_t buttonPin, uint8_t ledPin, Adafruit_AW9523* aw = nullptr);

    void init();
    bool isPressed() const;
    bool isLedOn() const { return ledOn; }
    void turnOnLed();
    void turnOffLed();

  private:
    const uint8_t buttonPin;
    const uint8_t ledPin;
    Adafruit_AW9523* aw;    // pointeur vers un objet AW9523
    bool ledAw9523;         // true si aw != nullptr
    bool ledOn = false;
};

void print64Hex(uint64_t val);
void printPayloadFromMasterStruct(const PayloadFromMasterStruct& payload); //passage par référence pour éviter de faire une copie de la structure
void printPayloadFromSlaveStruct(const PayloadFromSlaveStruct& payload);

#endif