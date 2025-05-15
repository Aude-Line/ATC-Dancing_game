#include "mp3.h"

MP3Module::MP3Module(uint8_t rxPin, uint8_t txPin)
    : SSerial(new SoftwareSerial(rxPin, txPin)){
    init();
}


void MP3Module::init(){
    SSerial->begin(115200);
    Mp3Player.init(*SSerial);

    Serial.println("MP3 Player initialized");
    setVolume(20); // Set volume to 0
}

void MP3Module::setVolume(const uint8_t volume){
    Mp3Player.volume(volume);
    Serial.println("Volume set to: " + String(volume));
}

void MP3Module::increaseVolume(){
    int err = Mp3Player.volumeUp();
    if(!err) Serial.println("Volume up");
    else Serial.println("ERROR");
}

void MP3Module::decreaseVolume(){
    int err = Mp3Player.volumeDown();
    if(!err) Serial.println("Volume down");
    else Serial.println("ERROR");
}

// Play a music in the SD root given its id
void MP3Module::playTrack(const uint8_t trackId){
    Mp3Player.playSDRootSong(trackId);
    Serial.println("Playing track: " + String(trackId));
}

// Play a music in a specific directory given its id
void MP3Module::playTrack(const char* directory, const uint8_t trackId){
    Mp3Player.playSDDirectorySong(directory, trackId);
    Serial.println("Playing track: " + String(trackId) + " in directory: " + *directory);
}


// Play a music in the SD root given its name
void MP3Module::playTrack(const char* trackName){
    Mp3Player.playSDSong(trackName);
    Serial.println("Playing track: " + *trackName);
}

void MP3Module::nextTrack(){
    Mp3Player.next();
    Serial.println("Next song");
}


void MP3Module::previousTrack(){
    Mp3Player.previous();
    Serial.println("Previous song");
}

void MP3Module::pause_or_play(){
    Mp3Player.pause_or_play();
    Serial.println("Pause or Play");
}


void MP3Module::stop(){
    Mp3Player.stop();
    Serial.println("Stop");
}   

void MP3Module::loopTrack(){
    int err = Mp3Player.playMode(WT2605C_SINGLE_CYCLE);
    Serial.println(err);
    if(!err) Serial.println("The playback mode is set to Single song loop mode.");
    else Serial.println("ERROR");
}