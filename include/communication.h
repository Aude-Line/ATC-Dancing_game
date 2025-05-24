#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include <util.h>

extern const uint64_t addresses[2]; // adresses utilisées pour la communication entre le master et les slaves, definies dans le .cpp

enum MasterCommand : uint8_t {
    CMD_STOP_GAME,
    CMD_SETUP,
    CMD_BUTTONS,
    CMD_SCORE,
    CMD_START_GAME
};

enum SCORE : uint8_t {
    SCORE_FAILED = 0,
    SCORE_SUCCESS = 1,
    SCORE_NEUTRAL = 2 // Used to indicate a neutral score, e.g., when the game is still ongoing
};

enum SlaveButtonsState : uint8_t {
    WRONG_BUTTONS_PRESSED,
    RIGHT_BUTTONS_PRESSED,
    BUTTONS_RELEASED
};

struct PayloadFromMasterStruct{
  MasterCommand command = CMD_STOP_GAME;
  uint8_t buttonsToPress = 0; //Masque de bits pour savoir quels boutons appuyer (0x01 = bouton 1, 0x02 = bouton 2, 0x04 = bouton 3, 0x08 = bouton 4)
  SCORE score = SCORE_FAILED; //true si le joueur a réussi, false sinon
};
struct PayloadFromSlaveStruct{
  uint8_t slaveId = 0; //id du slave qui envoie le message
  Player playerId = NONE; //needed for setup and adjustment
  SlaveButtonsState buttonsPressed = BUTTONS_RELEASED; //en mode le/les bons boutons qui devaient être appuyés ont tous été appuyés
};

void print64Hex(uint64_t val);
void printPayloadFromMasterStruct(const PayloadFromMasterStruct& payload); //passage par référence pour éviter de faire une copie de la structure
void printPayloadFromSlaveStruct(const PayloadFromSlaveStruct& payload);

#endif