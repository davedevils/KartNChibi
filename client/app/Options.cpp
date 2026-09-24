#include "Options.h"

#include <cstdio>
#include <cstdlib>

namespace KnC::Client {

bool parseOptions(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--host" && hasValue) out.host = argv[++i];
        else if (arg == "--port" && hasValue) out.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        else if (arg == "--user" && hasValue) out.user = argv[++i];
        else if (arg == "--pass" && hasValue) out.pass = argv[++i];
        else if (arg == "--auto") out.autoWalk = true;
        else if (arg == "--stop-at" && hasValue) out.stopAt = argv[++i];
        else if (arg == "--screenshot" && hasValue) out.screenshot = argv[++i];
        else if (arg == "--frames" && hasValue) out.frames = std::atoi(argv[++i]);
        else if (arg == "--game" && hasValue) out.gameDir = argv[++i];
        else if (arg == "--ui-dir" && hasValue) out.uiDir = argv[++i];
        else if (arg == "--lang" && hasValue) out.language = argv[++i];
        else if (arg == "--wire" && hasValue) out.wireLog = argv[++i];
        else if (arg == "--auto-create" && hasValue) out.autoCreateTrack = std::atoi(argv[++i]);
        else if (arg == "--auto-join") out.autoJoin = true;
        else if (arg == "--auto-buy") out.autoBuy = true;
        else if (arg == "--auto-race" && hasValue) out.autoRaceTrack = std::atoi(argv[++i]);
        else if (arg == "--race-seconds" && hasValue) out.raceSeconds = std::atoi(argv[++i]);
        else if (arg == "--wait" && hasValue) out.waitSeconds = std::atoi(argv[++i]);
        else if (arg == "--hour" && hasValue) out.hour = std::atoi(argv[++i]);
        else if (arg == "--auto-create-character" && hasValue) out.autoCreateCharacter = argv[++i];
        else if (arg == "--auto-mission" && hasValue) out.autoMission = std::atoi(argv[++i]);
        else if (arg == "--add-friend" && hasValue) out.addFriend = argv[++i];
        else if (arg == "--accept-friends") out.acceptFriends = true;
        else if (arg == "--say" && hasValue) out.say = argv[++i];
        else if (arg == "--whisper" && hasValue) out.whisperTo = argv[++i];
        else if (arg == "--note" && hasValue) out.note = argv[++i];
        else if (arg == "--room-pass" && hasValue) out.roomPassword = argv[++i];
        else if (arg == "--mode" && hasValue) out.roomMode = std::atoi(argv[++i]);
        else if (arg == "--team" && hasValue) out.team = std::atoi(argv[++i]);
        else if (arg == "--quick-match" && hasValue) out.quickMatch = std::atoi(argv[++i]);
        else if (arg == "--mute") out.mute = true;
        else if (arg == "--linger" && hasValue) out.linger = std::atoi(argv[++i]);
        else if (arg == "--state" && hasValue) out.state = argv[++i];
        else if (arg == "--no-focus") out.noFocus = true;
        else if (arg == "--status-line") out.statusLine = true;
        else if (arg == "--script" && hasValue) out.script = argv[++i];
        else if (arg == "--size" && hasValue) {
            int w = 0, h = 0;
            if (std::sscanf(argv[++i], "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0) return false;
            out.width = static_cast<uint16_t>(w);
            out.height = static_cast<uint16_t>(h);
            out.sizeGiven = true;
        }
        else return false;
    }
    // the sample states ride the auto walk to their screen the sample server answers the wire
    if (!out.state.empty()) {
        const std::string& st = out.state;
        out.user = "sample";
        out.pass = "sample";
        if (st == "room") { out.autoCreateTrack = 90; out.stopAt = "room"; }
        else if (st == "trackpick" || st == "inviting") { out.autoCreateTrack = 90; out.stopAt = st; }
        else if (st == "shopitem") out.stopAt = "shopitem";
        else if (st == "race") { out.autoRaceTrack = 90; out.stopAt = "race"; }
        else if (st == "result") { out.autoRaceTrack = 90; out.stopAt = "result"; }
        else if (st == "messenger") out.stopAt = "friends";
        else if (st == "creation") out.stopAt = "lobby";
        // the sample opens on the second row the first is cleared an explicit auto mission picks another
        else if (st == "mission") { if (out.autoMission < 0) out.autoMission = 1; out.stopAt = "mission"; }
        else out.stopAt = st;
    }
    if (!out.stopAt.empty() || out.autoCreateTrack > 0 || out.autoJoin || out.autoRaceTrack > 0 || out.autoBuy ||
        out.autoMission >= 0 || !out.addFriend.empty() || out.acceptFriends || out.quickMatch >= 0 || !out.note.empty())
        out.autoWalk = true;
    if (out.autoBuy && out.stopAt.empty()) out.stopAt = "shop";
    if (out.autoMission >= 0 && out.stopAt.empty()) out.stopAt = "mission";
    if ((!out.addFriend.empty() || out.acceptFriends || !out.note.empty()) && out.stopAt.empty()) out.stopAt = "friends";
    if (out.quickMatch >= 0 && out.stopAt.empty()) out.stopAt = "room";
    // a created room with a wait presses start after the wait so it races like an auto race
    if (out.autoCreateTrack > 0 && out.waitSeconds > 0 && out.autoRaceTrack == 0) out.autoRaceTrack = out.autoCreateTrack;
    if ((out.autoRaceTrack > 0 || out.autoJoin) && out.stopAt.empty()) out.stopAt = "result";
    else if (out.autoCreateTrack > 0 && out.stopAt.empty()) out.stopAt = "room";
    return true;
}

void printUsage() {
    std::fprintf(stderr,
        "usage: knc_client --game <client folder> [--host 127.0.0.1] [--port 50017] [--user u] [--pass p]\n"
        "                  [--auto] [--stop-at logo|login|channel|menu|lobby|garage|shop|missions|mission|friends|room|race|result]\n"
        "                  [--auto-create <track id>] [--auto-join] [--auto-race <track id>] [--mode M] [--team T]\n"
        "                  [--room-pass pw] [--quick-match M] [--race-seconds N] [--wait N] [--hour H]\n"
        "                  [--auto-create-character nick] [--auto-mission id] [--add-friend name] [--accept-friends]\n"
        "                  [--say text] [--whisper name] [--note text] [--mute] [--linger N]\n"
        "                  [--screenshot out.png] [--frames N] [--size WxH] [--ui-dir <folder>]\n"
        "                  [--lang Eng] [--wire wire.log]\n"
        "                  [--state logo|login|channel|menu|lobby|room|race|result|garage|shop|missions|mission|messenger|creation]\n"
        "                  [--no-focus] [--status-line] [--script <file>]\n"
        "       F12 writes <screen>.png next to the exe\n"
        "       script verbs: wait N, click X Y, rclick X Y, key NAME, text WORDS, shot NAME,\n"
        "       waitfor SCREEN [N], expect SCREEN, quit\n");
}

}
