#include <Arduino.h>
#ifndef COMMUN_H
#define COMMUN_H

#define MAX_PLAYERS 4 //Ne pas modifier pour utiliser toutes les couleurs

extern const uint64_t addresses[2]; // adresses utilisées pour la communication entre le master et les slaves, definies dans le .cpp

enum Colors{
    RED, 
    GREEN, 
    BLUE, 
    YELLOW
}; //jsp si besoin comme je suis partie du principe que on peut envoyer potentiellemnt plusieurs boutons à appuyer
enum Player : int8_t { //forcer à un int8_t pour réduire le payload
    NONE=-1, 
    PLAYER1, 
    PLAYER2, 
    PLAYER3, 
    PLAYER4
};
enum MasterCommand : uint8_t {
    CMD_STOP_GAME,
    CMD_SET_UP,
    CMD_BUTTONS,
    CMD_SCORE,
    CMD_MISSED_BUTTONS
};

struct PayloadFromMasterStruct{
  MasterCommand command;
  uint8_t buttonsToPress;
  uint16_t score;
};
struct PayloadFromSlaveStruct{
  Player idPlayer; //maybe not needed but can allow to adjust communication if there was a setup missed
  bool buttonsPressed; //en mode le/les bons boutons ont tous été appuyés
};

void print64Hex(uint64_t val);

#endif