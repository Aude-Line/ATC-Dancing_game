#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>

extern const uint64_t addresses[2]; // adresses utilisées pour la communication entre le master et les slaves, definies dans le .cpp

enum Player : int8_t {
  NONE = -1,
  PLAYER_1,
  PLAYER_2,
  PLAYER_3,
  PLAYER_4,
  MAX_PLAYERS//Nombre de joueurs, utilisé pour le nombre de couleurs
};

enum Color{
    RED, 
    GREEN, 
    BLUE, 
    YELLOW,
    NB_COLORS //Nombre de couleurs, utilisé pour le nombre de joueurs
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
  Player idPlayer; //needed for setup and adjustment
  bool buttonsPressed; //en mode le/les bons boutons qui devaient être appuyés ont tous été appuyés
};

void print64Hex(uint64_t val);
void printPayloadFromMasterStruct(const PayloadFromMasterStruct& payload); //passage par référence pour éviter de faire une copie de la structure
void printPayloadFromSlaveStruct(const PayloadFromSlaveStruct& payload);

#endif