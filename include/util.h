#ifndef UTIL_H
#define UTIL_H 

#include <Arduino.h>

#define SCORE_FAILED 0
#define SCORE_SUCCESS 1
#define SOUND_DURATION_LONG 500
#define SOUND_DURATION_SHORT 100
#define SOUND_FREQUENCY_GREAT 2000
#define SOUND_FREQUENCY_BAD 100
#define SOUND_FREQUENCY_NEUTRAL 600

enum Color{
    RED, 
    GREEN, 
    BLUE, 
    YELLOW,
    NB_COLORS //Nombre de couleurs, utilisé pour le nombre de joueurs
}; //jsp si besoin comme je suis partie du principe que on peut envoyer potentiellemnt plusieurs boutons à appuyer

enum Player : int8_t {
    NONE = -1,
    PLAYER_RED,
    PLAYER_GREEN,
    PLAYER_BLUE,
    PLAYER_YELLOW,
    MAX_PLAYERS//Nombre de joueurs, utilisé pour le nombre de couleurs
};

#endif