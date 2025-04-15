#ifndef UTIL_H
#define UTIL_H 

#include <Arduino.h>

#define SCORE_FAILED 0
#define SOUND_DURATION_LONG 500
#define SOUND_DURATION_SHORT 100
#define SOUND_FREQUENCY_GREAT 2000
#define SOUND_FREQUENCY_BAD 100
#define SOUND_FREQUENCY_NEUTRAL 600

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

#endif