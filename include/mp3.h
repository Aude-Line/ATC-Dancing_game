#ifndef MP3_H
#define MP3_H

#include "WT2605C_Player.h"
#include <SoftwareSerial.h>



class MP3Module {
    public:
        MP3Module(uint8_t rxPin, uint8_t txPin);
        void setVolume(const uint8_t volume);
        void increaseVolume();
        void decreaseVolume();

        void playTrack(const uint8_t trackId);
        void playTrack(const char* directory, const uint8_t trackId);
        void playTrack(const char* trackName);


        void nextTrack();
        void previousTrack();
        void pause_or_play();
        void stop();

        void loopTrack();

    private:
        //Prive functions
        void init();

        // Attributes
        SoftwareSerial* SSerial;
        WT2605C<SoftwareSerial> Mp3Player;

};

#endif