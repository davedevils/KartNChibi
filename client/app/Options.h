// the command line of the client
#pragma once

#include <cstdint>
#include <string>

namespace KnC::Client {

struct Options {
    std::string host = "127.0.0.1";
    uint16_t port = 50017;
    std::string user;
    std::string pass;
    bool autoWalk = false;
    // the auto walk stops on this screen logo login channel menu lobby room or race
    std::string stopAt;
    std::string screenshot;
    int frames = 3;
    std::string gameDir;
    std::string uiDir;
    std::string language = "Eng";
    std::string wireLog;
    uint16_t width = 1024;
    uint16_t height = 768;
    // true when the size flag was given it wins over the window and wide rows of Option2 ini
    bool sizeGiven = false;
    // the lobby creates a room on this track and stops there
    int autoCreateTrack = 0;
    // the lobby joins the first open room and stops there
    bool autoJoin = false;
    // creates a room on this track starts alone and drives the line the captures come by themselves
    int autoRaceTrack = 0;
    // seconds the auto race drives before it gives up when the race never ends
    int raceSeconds = 240;
    // seconds the auto race waits in the room before the start so the server can seat its CPU cars
    int waitSeconds = 0;
    // the hour of the day the track scene draws with
    int hour = 12;
    // the shop buys the first affordable row of the consumable tab and captures the wallet twice
    bool autoBuy = false;
    // the creation popup of a fresh account is answered with this nickname
    std::string autoCreateCharacter;
    // the lobby opens the mission menu and starts this mission id zero takes the first row
    int autoMission = -1;
    // the lobby opens the messenger and sends a friend request to this name
    std::string addFriend;
    // the messenger accepts every pending friend request
    bool acceptFriends = false;
    // the lobby chat line sent once with the whisper target when one is given
    std::string say;
    std::string whisperTo;
    // the messenger sends this note to the whisper name
    std::string note;
    // the password of the created room and of the join
    std::string roomPassword;
    // the game mode of the created room 0 item single 1 item team 2 speed single 3 speed team
    int roomMode = 0;
    // the team picked in the room minus one leaves it alone
    int team = -1;
    // the lobby sends the quick match verb with this mode minus one never
    int quickMatch = -1;
    // no audio device is opened
    bool mute = false;
    // seconds a staged run stays open after its last capture so another client can meet it
    int linger = 0;
    // a screen opened offline on the sample server logo login channel menu lobby room race result and the rest
    std::string state;
    // the window opens behind the others and takes no focus a test run beside a person at the keyboard
    bool noFocus = false;
    // the grey status line of the screens is drawn the stock has none so it stays off
    bool statusLine = false;
    // a script file of one verb per line that clicks and types like a user see the client README
    std::string script;
};

// false on a bad argument the usage text goes to stderr
bool parseOptions(int argc, char** argv, Options& out);
void printUsage();

}
