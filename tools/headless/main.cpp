// drives the login to lobby handshake with no GUI and logs the whole wire both directions for diffing servers
#define WIN32_LEAN_AND_MEAN
#include <cstdlib>
#include <ctime>
#include <winsock2.h>
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "NetClient.h"
#include "CatalogDecode.h"
#include "Drive.h"

using namespace knc::headless;
using namespace KnC::Client;
namespace CMD = ::knc::CMD;
using ::knc::Packet;

namespace {

std::u16string toU16(const std::string& s) { return std::u16string(s.begin(), s.end()); }

// login stage so the flow reacts to the right reply and does not double send
enum class Stage { LauncherLogin, ClientInfo, ChannelWait, GameServer, Lobby };

struct Flow {
    NetClient& net;
    std::string host;
    std::string user, pass;
    uint32_t firstDriverKey = 0;   // first driver key seen the creation popup answers with it
    std::string token;
    Stage stage = Stage::LauncherLogin;
    uint16_t gamePort = 50018;  // our default overwritten if the server carries one

    // drive mode follow a track waypoint path once in a room
    bool driveMode = false;
    bool boostMode = false;   // set drift and boost state bits while driving
    std::string wpFile;
    bool inLobby = false, driving = false;
    Driver driver;

    // ghost mode enter a solo time trial drive it live then upload the lap
    bool ghostMode = false;
    int ghostTrack = 1;
    int ghostTargetMs = 0;   // 0 drives real laps else drive to this exact time
    bool ghostActive = false, ghostDone = false;
    std::vector<uint8_t> ghostFrames;
    uint32_t ghostFrameCount = 0;
    double ghostFrameTimer = 0.0;

    void ghostEnter() {
        Packet enter(CMD::C_TUTORIAL_COMPLETE);   // 0xAA enter ghost race 4 byte track
        enter.writeUInt32(static_cast<uint32_t>(ghostTrack));
        net.send(enter);
        net.send(Packet::fromCmdFull(0xB1));      // stage 15 began recording starts
        ghostActive = true;
        printf("[GHOST] entered track %d, driving the lap live\n", ghostTrack);
    }
    // called each motion tick while racing samples a frame at 5Hz
    void ghostSample(double dtMs, float x, float y, float z, uint8_t yaw) {
        ghostFrameTimer += dtMs;
        if (ghostFrameTimer < 200.0) return;
        ghostFrameTimer -= 200.0;
        auto f = encodeReplayFrame(x, y, z, yaw);
        ghostFrames.insert(ghostFrames.end(), f.begin(), f.end());
        ++ghostFrameCount;
    }
    void ghostFinish() {
        net.send(Packet::fromCmdFull(0xB2));      // final lap crossed announce count
        Packet cnt = Packet::fromCmdFull(0xAE);
        cnt.writeUInt32(ghostFrameCount);
        net.send(cnt);
        // the server chunks at 136 frames and 3808 bytes an oversized chunk reads back as zero
        const uint32_t kMaxFramesPerChunk = 136;
        for (uint32_t sent = 0; sent < ghostFrameCount; sent += kMaxFramesPerChunk) {
            const uint32_t n = (ghostFrameCount - sent) < kMaxFramesPerChunk
                             ? (ghostFrameCount - sent) : kMaxFramesPerChunk;
            Packet chunk = Packet::fromCmdFull(0xAF);
            chunk.writeUInt32(n);
            chunk.writeBytes(ghostFrames.data() + static_cast<size_t>(sent) * 28,
                             static_cast<size_t>(n) * 28);
            net.send(chunk);
        }
        const uint32_t timeMs = ghostTargetMs > 0
                              ? static_cast<uint32_t>(ghostTargetMs) : ghostFrameCount * 200u;
        // body is track then time then zero a careless submit erases the holder instead of beating them
        const char* noSub = getenv("KNC_GHOST_NOSUBMIT");
        if (noSub && noSub[0] == '1') {
            printf("[GHOST] no submit, drove %u frames\n", ghostFrameCount);
            ghostDone = true;
            return;
        }
        Packet sub = Packet::fromCmdFull(0xB0);
        sub.writeUInt32(static_cast<uint32_t>(ghostTrack));
        sub.writeUInt32(timeMs);
        sub.writeUInt32(0);
        net.send(sub);
        ghostDone = true;
        printf("[GHOST] finished, uploaded %u frames time %ums track %d\n",
               ghostFrameCount, timeMs, ghostTrack);
    }

    void sendLauncherLogin() {
        Packet p(CMD::C_LAUNCHER_LOGIN);
        p.writeString(user);
        p.writeString(pass);
        net.send(p);
    }
    void sendClientInfo() {
        Packet p(CMD::C_CLIENT_INFO);
        p.writeWString(toU16(token));   // session USERNAME server ip the client thinks it hit
        p.writeString(host);
        net.send(p);
    }
    void sendFullState() { net.send(Packet(CMD::C_FULL_STATE)); }
    void sendClientAuth() {
        // action 4 is the direct login path no launcher token dance needed for a headless
        Packet p(CMD::C_CLIENT_AUTH);
        // the version cstring is the launcher's not the binary's overridable by an environment variable
        {
            const char* v = getenv("KNC_CLIENT_VERSION");
            p.writeString((v && v[0]) ? v : "2");     // version
        }
        p.writeInt32(4);                    // action 4 direct login
        p.writeWString(toU16(user));
        p.writeWString(toU16(pass));
        net.send(p);
    }
    void sendReauth() {
        // sub 480500 what the stock client sends on every connection after the first
        Packet p = Packet::fromCmdFull(0x00A7);
        const char* v = getenv("KNC_CLIENT_VERSION");
        p.writeString((v && v[0]) ? v : "2");
        p.writeInt32(4);
        p.writeWString(toU16(reauthToken));
        p.writeUInt32(reauthAccount);
        net.send(p);
    }
    void authSequence() { sendFullState(); sendClientAuth(); }
    // after a redirect the ticket goes back a login server that gave none gets the credentials again
    void reauthSequence() {
        sendFullState();
        if (!reauthToken.empty()) sendReauth(); else sendClientAuth();
    }
    void sendChannelSelect() {
        Packet p(CMD::C_CHANNEL_SELECT);
        p.writeInt32(0x0E);   // screen id seen on the wire channel 0
        p.writeInt32(0);
        net.send(p);
    }
    void sendCreateRoom() {
        Packet p(CMD::C_CREATE_ROOM_REQ);   // 0x2D room name no password
        p.writeWString(u"bot");
        p.writeWString(u"");
        p.writeInt32(8);   // maxUsers mode item single flag public so matchmaking can see it
        p.writeInt32(0);
        p.writeInt32(0);
        p.writeInt32(1);
        net.send(p);
    }

    // two bot multiplayer host creates a room and writes its id join reads it and both drive the race
    bool hostMode = false, joinMode = false;
    // the head of the profile blob echoed on reconnect the way the stock client does
    uint32_t reauthAccount = 0;
    std::string reauthToken;
    // the server seats a bot in a waiting room after the wait timer start once that has had time
    double hostRoomUpAt = 0;
    bool   hostAutoStarted = false;
    std::string roomIdFile = "roomid.txt";
    bool roomStarted = false;
    double raceGoAt = 0;
    bool motionInit = false;  // reset the dt at GO else the first step bursts checkpoints
    bool raceFinished = false;
    int  lastLapsSent = 0;
    bool trackPicked = false;
    // when the room master leaves it moves to us only the master start press makes the server send the GO
    double startPressAt = 0;
    int    startPresses = 0;
    bool   raceGo = false;
    // the race scene loads well before the green light coordinates sent between the two drop us off the grid
    bool   raceLaunched = false;

    bool packedMotion = true;   // real client noisier motion volume reveals bot set false on own server

    // measured off a human race byte 17 is a flag byte 18 packs speed low and drift charge high
    uint8_t  prevYaw = 0;
    double   driftSince = 0;      // when the current drift began 0 when not drifting
    int32_t  heldItem = -1;
    // every track ships a start ini with 16 grid slots as x y z heading like the racing line
    std::vector<Waypoint> gridSlots;
    // joining a room only means we asked talking before the room context returns spams a room we never entered
    bool inRoomConfirmed = false;
    int gridSlot = 1;
    std::u16string myName;     // to spot our own row in the member list itembox ini of the loaded track
    std::vector<Waypoint> itemBoxes;
    std::vector<char>     boxTaken;
    double useItemAt = 0;
    // getting hit is reported by the victim our bot never reported anything so the server never knew it was hit
    double stunUntil = 0;
    double boostUntil = 0;   // a mini boost keeps 0x80 lit for a moment
    float  lastX = 0, lastY = 0;
    unsigned itemRng = 12345;
    double   lastSpeed = 0;
    int32_t myChar = 0, myKart = 0;   // our own appearance keys
    bool    appearanceSent = false;
    double tickAt = 0;        // the loudest race packet by far 0xCF 0x58 0x49 go out once when the GO lands
    bool   racePrimed = false;

    int  hostTrack = 10;      // a real track with a map
    int memberSeen = 0;
    // a room named the same thing every time is the loudest bot tell pick a flavoured name per run
    std::u16string randomRoomName() const {
        static const char16_t* kNames[] = {
        u"Drift or dessert",
        u"Banana peel bandits",
        u"Sugar rush GP",
        u"Full throttle chibi",
        u"Mini boost masters",
        u"Last lap legends",
        u"Turbo teacups",
        u"Powder snow drifters",
        u"Rainbow tyre marks",
        u"No brakes club",
        u"Grip and giggles",
        u"Kart before the horse",
        u"Two wheels one dream",
        u"Boost juice only",
        u"Cornering candy",
        u"Photo finish please",
        };
        const size_t n = sizeof(kNames) / sizeof(kNames[0]);
        return kNames[static_cast<size_t>(time(nullptr) + rand()) % n];
    }

    void hostCreateRoom() {
        printf("[HOST] public room, waiting for the join bot\n");
        Packet room(CMD::C_CREATE_ROOM_REQ);
        room.writeWString(randomRoomName());
        room.writeWString(u"");     // public so the join bot enters straight by id
        room.writeInt32(4);
        room.writeInt32(0);
        room.writeInt32(0);
        room.writeInt32(0);
        net.send(room);
    }
    // empty means pick from the lobby listing ourselves set it to hunt a name
    std::u16string joinTargetName;
    uint32_t bestRoom = 0;
    double   pickAt = 0;       // let the whole list arrive before choosing re ask the lobby since rooms open after we walked in
    double   askAt = 0;
    bool joinedRoom = false;
    void joinEnterLobby() {
        // the room list has roomId at offset 0 on every server pick the room by name when it arrives
        printf("[JOIN] entering lobby and asking for the room list\n");
        net.send(Packet::fromCmdFull(0x0012));   // server sends room list only when asked exactly enter packet empty
        net.send(Packet::fromCmdFull(0x00FA));
        net.send(Packet::fromCmdFull(0x008E));
        Packet tel = Packet::fromCmdFull(0x0130);
        tel.writeInt32(0);
        net.send(tel);
    }
    void joinRoomById(uint32_t id) {
        printf("[JOIN] found room %u, joining via 0x2F\n", id);
        // join is roomId then a zero u16 not the packet our own server uses
        Packet j = Packet::fromCmdFull(0x002F);
        j.writeUInt32(id);
        j.writeUInt16(0);
        net.send(j);
    }

    // chibikart sends no 0x2D lobby list so the join reads the host published id
    void tryJoinFromFile() {
        FILE* f = nullptr; fopen_s(&f, roomIdFile.c_str(), "r");
        if (!f) return;
        uint32_t rid = 0;
        if (fscanf_s(f, "%u", &rid) == 1 && rid) { joinedRoom = true; joinRoomById(rid); }
        fclose(f);
    }

    // create a private room and sit to capture the room creation wire clean
    bool roomMode = false;
    int roomTrack = 0;      // if set change the room track after creating it
    void makeRoom() {
        printf("[ROOM] public room SOUK for a manual join\n");
        Packet room(CMD::C_CREATE_ROOM_REQ);
        room.writeWString(u"SOUK");
        room.writeWString(u"");        // public no password 8 seats
        room.writeInt32(8);
        room.writeInt32(0);
        room.writeInt32(0);
        room.writeInt32(0);            // 0 public so the GUI can walk in
        net.send(room);
        if (roomTrack > 0) {
            Packet trk = Packet::fromCmdFull(CMD::S_ROOM_TRACK_SELECT);  // 0x35 both ways
            trk.writeInt32(roomTrack);
            trk.writeInt32(0);
            net.send(trk);
        }
    }

    // screen tour opens every system the client can reach and records what comes back
    bool tourMode = false;
    size_t tourStep = 0;
    double tourAt = 0;
    struct Screen { const char* name; uint16_t op; bool body; };
    void runTourStep() {
        static const Screen kScreens[] = {
            { "garage",    0x000F, false }, { "shop",      0x0010, false },
            { "license",   0x0016, false }, { "missions",  0x008F, false },
            { "carcraft",  0x010A, false }, { "roomcraft", 0x010E, false },
            { "userlist",  0x0132, true  },
            { "chapters",  0x00CD, false }, { "progress",  0x00CC, false },
            { "quest",     0x00FE, false }, { "ghost",     0x00AA, false },
            { "nickq",     0x0025, false }, { "ext132",    0x0084, false },
            { "ext133",    0x0085, false }, { "giftact",   0x009B, false },
            { "questclaim",0x00F8, false }, { "entlist",   0x0114, false },
        };
        const size_t n = sizeof(kScreens) / sizeof(kScreens[0]);
        if (tourStep >= n) { tourAt = net.now() + 999.0; return; }
        const Screen& sc = kScreens[tourStep++];
        printf("[TOUR] opening %s via 0x%02X\n", sc.name, sc.op);
        Packet p = Packet::fromCmdFull(sc.op);
        if (sc.body) p.writeInt32(0);
        net.send(p);
        tourAt = net.now() + 3.0;   // let the answer land before the next screen
    }

    bool testMode = false;
    bool chatMode = false;
    std::string chatText;
    void runTest() {
        printf("[TEST] buy, equip, and a private four seat room\n");
        // buy category baseKey priceKey name
        Packet buy = Packet::fromCmdFull(CMD::C_BUY);
        buy.writeUInt32(0);       // category 0 character base key a driver price key its permanent tier
        buy.writeUInt32(5);
        buy.writeInt32(5);
        buy.writeWString(u"test");
        net.send(buy);
        // garage install action key extra action 0 equips a kart
        Packet eq = Packet::fromCmdFull(CMD::C_GARAGE_INSTALL);
        eq.writeInt32(0);         // action 0 kart base kart key
        eq.writeInt32(10010);
        eq.writeInt32(0);
        net.send(eq);
        // private room with a password and four seats
        Packet room(CMD::C_CREATE_ROOM_REQ);
        room.writeWString(randomRoomName());
        room.writeWString(u"1234"); // password max users mode flag private
        room.writeInt32(4);
        room.writeInt32(0);
        room.writeInt32(0);
        room.writeInt32(1);
        net.send(room);
    }
    // real start button sub 40C950 toggles then sends pressed as one int32 or every other client draws nothing
    double racePace = 1.0;
    void applyCadence() {
        std::vector<double> gaps;
        FILE* f = nullptr; fopen_s(&f, "cadence.ini", "r");
        if (f) {
            char line[64];
            while (fgets(line, sizeof(line), f)) {
                if (line[0] == '#' || line[0] == '\n') continue;
                double v = atof(line);
                if (v >= 0.0) gaps.push_back(v);
            }
            fclose(f);
        }
        if (gaps.size() >= 4) {
            driver.setCheckpointPattern(gaps, racePace);
            printf("[DRIVE] replaying a human cadence, %zu gaps, pace %.2f\n",
                   gaps.size(), racePace);
        } else {
            driver.setCheckpointInterval(2800.0 * racePace);
            printf("[DRIVE] no cadence ini, flat 2800 ms\n");
        }
    }

    // every shipped track carries its own racing line and checkpoints tens pick the theme and units pick the course
    std::string worldRoot() const {
        const char* env = getenv("KNC_WORLD");
        return env && *env ? std::string(env)
                           : std::string("Data/Public/World");
    }
    std::string trackDir(int id) const {
        static const char* theme[10] = { nullptr, "Forest", "Cookie", "Desert", "Toy",
                                         "Devil", "Snow", "Palace", "Swamp", "Race" };
        const int t = id / 10, n = id % 10 + 1;
        if (t < 1 || t > 9 || !theme[t]) return std::string();
        char buf[256];
        snprintf(buf, sizeof(buf), "%s/%s/%s_%02d", worldRoot().c_str(), theme[t], theme[t], n);
        return buf;
    }
    int loadedTrack = 0;
    // pick the track the room actually selected not a hardcoded one
    bool loadTrack(int id) {
        if (id <= 0 || id == loadedTrack) return loadedTrack == id;
        const std::string dir = trackDir(id);
        if (dir.empty()) { printf("[TRACK] id %d has no known folder\n", id); return false; }
        std::vector<Waypoint> wp;
        std::string used;
        // to replay a downloaded ghost hand its line as arg 6 instead ghost mode reads that file directly
        for (const char* cand : { "follow_01.ini", "follow_02.ini", "follow_03.ini" }) {
            used = dir + "/" + cand;
            wp = loadWaypoints(used);
            if (!wp.empty()) break;
        }
        if (wp.empty()) { printf("[TRACK] no racing line under %s\n", dir.c_str()); return false; }
        const int cp = loadCheckpointCount(used);
        driver.reset(wp, 60.0f);
        driver.setRace(cp > 0 ? cp : 14, 3);
        applyCadence();
        // the shipped item box list in the racing line coordinate space so the bot collects by driving over one
        gridSlots = loadWaypoints(dir + "/start.ini");
        itemBoxes.clear(); boxTaken.clear();
        {
            auto boxes = loadWaypoints(dir + "/itembox.ini");
            itemBoxes = boxes;
            boxTaken.assign(itemBoxes.size(), 0);
        }
        loadedTrack = id;
        printf("[TRACK] %d loaded, %zu waypoints, %d checkpoints, %zu item boxes, %s\n",
               id, wp.size(), cp > 0 ? cp : 14, itemBoxes.size(), used.c_str());
        printf("[TRACK] %zu grid slots\n", gridSlots.size());
        return true;
    }

    void sendAppearance() {
        if (appearanceSent || myChar == 0) return;
        appearanceSent = true;
        Packet a = Packet::fromCmdFull(0x00D9);
        a.writeInt32(myChar);
        a.writeInt32(myKart);
        for (int i = 0; i < 60; ++i) a.writeUInt8(0);
        net.send(a);
        printf("[ME] declared appearance char=%d kart=%d\n", myChar, myKart);
    }

    void sendStart() {
        Packet s = Packet::fromCmdFull(CMD::C_ROOM_READY_TOGGLE);
        s.writeInt32(1);
        net.send(s);
    }

    void onFrame(uint16_t op, Packet& pkt) {
        (void)pkt;
        // chibikart has no lobby list poll the host published room id every frame
        if (joinMode && inLobby && !joinedRoom) {
            tryJoinFromFile();
            // the list is a snapshot answered once per request a room opened after we asked is invisible until asked again
            if (net.now() >= askAt) {
                askAt = net.now() + 5.0;
                net.send(Packet::fromCmdFull(0x0012));
                net.send(Packet::fromCmdFull(0x00FA));
                net.send(Packet::fromCmdFull(0x008E));
                Packet tel = Packet::fromCmdFull(0x0130);
                tel.writeInt32(0);
                net.send(tel);
            }
            // the lobby sends one room row per request wait a beat for them all then take the best one listed
            if (!joinedRoom && bestRoom) {
                if (pickAt == 0) pickAt = net.now() + 1.5;
                else if (net.now() >= pickAt) {
                    printf("[LOBBY] picking room %u on my own\n", bestRoom);
                    joinedRoom = true;
                    joinRoomById(bestRoom);
                }
            }
        }
        switch (op) {
        case 0xBF: {
            // driver catalogue row offset 0x0C is the driver key keep the first for a creation
            const auto& pl = pkt.payload();
            if (firstDriverKey == 0 && pl.size() >= 0x10) {
                firstDriverKey = static_cast<uint32_t>(pl[0x0C] | (pl[0x0D] << 8) | (pl[0x0E] << 16) | (pl[0x0F] << 24));
            }
            break;
        }
        case 0x03: {
            // the account has no character and the popup opened after the catalogues answer with the driver key and a nickname
            const uint32_t key = firstDriverKey ? firstDriverKey : 10u;
            std::u16string nick;
            for (char c : user) { if (nick.size() < 11) nick.push_back(static_cast<char16_t>(c)); }
            while (nick.size() < 4) nick.push_back(u'x');
            Packet create = Packet::fromCmdFull(0x0004);
            create.writeUInt32(key);
            create.writeWString(nick);
            net.send(create);
            printf("[CREATE] popup opened, creating driver %u as '%s'\n", key, user.c_str());
            break;
        }
        case 0x04: {
            const auto& pl = pkt.payload();
            const int32_t result = pl.size() >= 4 ? static_cast<int32_t>(pl[0] | (pl[1] << 8) | (pl[2] << 16) | (pl[3] << 24)) : -1;
            printf("[CREATE] result %d, %zu bytes%s\n", result, pl.size(),
                   result == 0 ? ", character made, waiting for the player phase" : "");
            break;
        }
        case 0x01: {
            // login verdict a refusal used to leave us heartbeating into a dead session for the whole timeout
            std::string msg;
            const auto& pl = pkt.payload();
            const bool wide = pl.size() > 3 && pl[1] == 0 && pl[3] == 0;
            for (size_t i = 0; i + (wide ? 1u : 0u) < pl.size(); i += wide ? 2 : 1) {
                const unsigned char c = pl[i];
                if (c == 0) break;
                if (c < 32 || c > 126) break;
                msg += static_cast<char>(c);
            }
            if (msg.rfind("MSG_REINPUT", 0) == 0 || msg.rfind("MSG_ALREADY", 0) == 0 ||
                msg.rfind("MSG_BLOCK", 0) == 0 ||
                msg.find("already logged in") != std::string::npos) {
                printf("[FLOW] login refused, %s\n", msg.c_str());
                net.disconnect();
                return;
            }
            if (!msg.empty()) printf("[FLOW] login says %s\n", msg.c_str());

            break;
        }
        case CMD::S_ACK: break;   // the login verdict is handled above 0x0B login done pick a channel
        case CMD::S_CHANNEL_LIST:
            printf("[FLOW] channel list, selecting channel 0\n");
            stage = Stage::ChannelWait;
            sendChannelSelect();
            break;
        case CMD::C_CLIENT_AUTH: {  // login accept u32 char id then the 1224 byte profile
            const auto& pl = pkt.payload();
            if (pl.size() >= 12) {
                reauthAccount = static_cast<uint32_t>(pl[4]) | (static_cast<uint32_t>(pl[5]) << 8) |
                                (static_cast<uint32_t>(pl[6]) << 16) | (static_cast<uint32_t>(pl[7]) << 24);
                reauthToken.clear();
                for (size_t k = 8; k + 1 < pl.size() && k < 8 + 2 * 64; k += 2) {
                    const uint16_t c = static_cast<uint16_t>(pl[k] | (pl[k + 1] << 8));
                    if (c == 0) break;
                    reauthToken += static_cast<char>(c < 128 ? c : '?');
                }
            }
            break;
        }
        case CMD::S_SERVER_REDIRECT: {  // reconnect to the game server
            printf("[FLOW] redirect 0x54, reconnecting to game server %s:%u\n",
                   host.c_str(), gamePort);
            stage = Stage::GameServer;
            net.disconnect();
            if (!net.connect(host, gamePort)) { printf("[FLOW] game connect failed\n"); return; }
            reauthSequence();  // the ticket or a direct login when there is none
            break;
        }
        case 0x11:  // chibikart lands here where we send the lobby enter we are in drive a room or record a ghost
        case 0x12:
            if (chatMode && !inLobby) {
                inLobby = true;
                // the text then the type the id and name are server side
                Packet c = Packet::fromCmdFull(0x00B4);
                c.writeWString(toU16(chatText));
                c.writeInt32(0);                    // chat normal
                net.send(c);
                printf("[CHAT] sent %s\n", chatText.c_str());
                break;
            }
            if (testMode && !inLobby) { inLobby = true; runTest(); break; }
            if (roomMode && !inLobby) { inLobby = true; makeRoom(); break; }
            if ((hostMode || joinMode) && !inLobby) {
                inLobby = true;
                auto wp = loadWaypoints(wpFile);
                driver.reset(std::move(wp), 90.0f);
                int cp = loadCheckpointCount(wpFile);
                driver.setRace(cp > 0 ? cp : 14, 3);
                if (hostMode) hostCreateRoom();
                else          joinEnterLobby();
                break;
            }
            if ((driveMode || ghostMode) && !inLobby) {
                inLobby = true;
                auto wp = loadWaypoints(wpFile);
                // 100 units per second stays under the anti cheat cap yet gives a human lap
                driver.reset(std::move(wp), ghostMode ? 100.0f : 80.0f);
                int cp = loadCheckpointCount(wpFile);   // first u32 of the checkpoint file
                printf("[DRIVE] track checkpoints=%d\n", cp > 0 ? cp : 14);
                driver.setRace(cp > 0 ? cp : 14, 3);
                if (ghostMode) {
                    // a time target drives until that clock else three real laps
                    driver.setGhostLaps(ghostTargetMs > 0 ? 9999 : 3);
                    ghostEnter();       // enter ghost race then start recording then drive the lap in the loop
                    driving = true;
                } else {
                    printf("[DRIVE] loaded waypoints, creating room\n");
                    sendCreateRoom();
                }
            }
            break;
        // our own equipment lands at login well before the room appearance arrives far too late
        case 0x02: {
            // it also carries fatal verdicts as wide prose waiting one out is 300 seconds of nothing
            std::string prose;
            const auto& pl2 = pkt.payload();
            for (size_t k = 0; k + 1 < pl2.size(); k += 2) {
                const unsigned char c = pl2[k];
                if (c == 0 || c < 32 || c > 126) break;
                prose += static_cast<char>(c);
            }
            if (prose.find("already logged in") != std::string::npos) {
                printf("[FLOW] %s\n", prose.c_str());
                net.disconnect();
                return;
            }
            if (joinMode && joinedRoom && !inRoomConfirmed) {
                printf("[JOIN] refused, will retry\n");
                joinedRoom = false;
            }
            break;
        }
        case 0x1B:
            if (myChar == 0 && pkt.remaining() >= 8) {
                pkt.readInt32();
                myChar = pkt.readInt32();
                printf("[ME] character key %d\n", myChar);
            }
            break;
        case 0x1C:
            if (myKart == 0 && pkt.remaining() >= 8) {
                pkt.readInt32();
                myKart = pkt.readInt32();
                printf("[ME] kart key %d\n", myKart);
            }
            break;
        case 0x21: {
            // picking our own slot matters a hardcoded one puts us on top of whoever really holds it
            const int slot = pkt.remaining() >= 4 ? pkt.readInt32() : -1;
            if (pkt.remaining() >= 8) {
                pkt.readInt32(); pkt.readInt32();
                const std::u16string nm = pkt.readWString(64);
                if (!myName.empty() && nm == myName && slot >= 0) {
                    if (gridSlot != slot) printf("[GRID] server put us on slot %d\n", slot);
                    gridSlot = slot;
                }
            }
            break;
        }
        case 0x14:
            // this is the authoritative track a room can carry an unfixed sentinel until launch
            if (pkt.remaining() >= 12) {
                pkt.readInt32(); pkt.readInt32();
                raceLaunched = true;
                const int tid = pkt.readInt32();
                if (tid > 0 && tid < 200) loadTrack(tid);
            }
            break;
        case 0x35:  // server confirms the room track two int32 id then dialog option
            if (pkt.remaining() >= 4) {
                const int id = pkt.readInt32();
                if (id > 0) loadTrack(id);
            }
            break;
        case 0x13: {  // room enter and state
            inRoomConfirmed = true;
            // the room context carries the track and is the only place a joining client learns it
            Packet probe = pkt;
            if (probe.remaining() >= 4) {
                probe.readUInt32();
                probe.readWString(64);
                if (probe.remaining() >= 16) {
                    probe.readInt32(); probe.readInt32(); probe.readInt32();
                    const int tid = probe.readInt32();
                    // the unfixed or random sentinel ignore it and wait for the launch to say what we are actually racing
                    if (tid > 0 && tid < 200) loadTrack(tid);
                }
            }
        }
            // fall through to the existing room handling

            if (roomMode) {
                uint32_t rid = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
                printf("[ROOM] created, roomId=%u  <-- join this in the GUI lobby\n", rid);
            } else if (driveMode && inLobby && !driving) {
                printf("[DRIVE] room created, starting race\n");
                driver.setRace(14, 3);   // track 1 has 14 checkpoints run 3 laps
                sendStart();
                driving = true;
            } else if (hostMode && inLobby && !roomStarted && !trackPicked) {
                // the roomId is the first int32 here not a fixed anchor
                uint32_t rid = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
                if (rid) {                       // publish the real room id for the join bot
                    FILE* f = nullptr; fopen_s(&f, roomIdFile.c_str(), "w");
                    if (f) { fprintf(f, "%u", rid); fclose(f); }
                    printf("[HOST] room %u up, picking track %d\n", rid, hostTrack);
                    hostRoomUpAt = net.now();
                }
                sendAppearance();
                // a room with no map cannot score a race pick a real track
                trackPicked = true;
                Packet trk = Packet::fromCmdFull(CMD::S_ROOM_TRACK_SELECT);  // 0x35
                trk.writeInt32(hostTrack);
                trk.writeInt32(0);   // sub 474380 the dialog three way option picks 0 1 or 2
                net.send(trk);
                // the room context still carried the old track when read above load the one just asked for instead
                loadTrack(hostTrack);
            } else if (joinMode && inLobby && !roomStarted && joinedRoom) {
                sendAppearance();
                printf("[JOIN] ready, racing in 3 2 1\n");
                Packet rdy = Packet::fromCmdFull(CMD::C_ROOM_READY_TOGGLE);  // 0x33 pressed marks the non host ready
                rdy.writeInt32(1);
                net.send(rdy);
                roomStarted = true;
                startPressAt = net.now() + 6.0;   // press start if we ended up master driving starts on the server GO never on a timer

            }
            break;
        case 0x47:  // someone fired an item relayed with the id prefixed
            if (raceGo && net.now() > stunUntil && pkt.remaining() >= 24) {
                pkt.readInt32();                       // shooter
                const int32_t item = pkt.readInt32();
                const float ix = pkt.readFloat();
                const float iy = pkt.readFloat();
                const float dx = ix - lastX, dy = iy - lastY;
                const float d2 = dx * dx + dy * dy;
                if (d2 < 3600.0f) {                    // fired within 60 units of us
                    itemRng = itemRng * 1103515245u + 12345u;
                    if ((itemRng >> 16) % 100 < 45) {  // not every shot lands codes for spin bump crash and the heavy stun

                        static const int16_t kEff[] = { 100, 200, 300, 800 };
                        const int16_t code = kEff[(itemRng >> 8) % 4];
                        Packet h = Packet::fromCmdFull(0x0069);
                        h.writeInt16(code);
                        h.writeUInt8(1);
                        net.send(h);
                        stunUntil = net.now() + (code == 800 ? 2.6 : 1.5);
                        printf("[HIT ] item %d, effect %d, stalled %.1f s\n",
                               item, code, stunUntil - net.now());
                    }
                }
            }
            break;
        case 0x3A:  // GO verified on a full captured race our own server all start sub 479A10
        case 0x34:
            printf("[RACE] server GO 0x%02X, race is scored now\n", op);
            raceGo = true;
            racePrimed = false;      // the one shot race entry chatter
            if (!gridSlots.empty()) {
                const Waypoint& g = gridSlots[gridSlot % gridSlots.size()];
                driver.placeAt(g.x, g.y, g.z);
                // the grid heading follows the game convention 0 means facing minus X
                const float rad = g.heading * 3.14159265f / 180.0f;
                driver.startNear(g.x, g.y, -cosf(rad), -sinf(rad));
            }
            tickAt = net.now();
            driving = true;
            raceGoAt = net.now() + 10.0;  // measured a real kart first moves 9dot5 to 11dot5 seconds after GO
            applyCadence();
            break;
        case 0x2D:
            // the first two counts are current and maximum players pick one ourselves rather than being handed an id
            if (joinMode && inLobby && !joinedRoom) {
                const uint32_t rid = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
                const std::u16string name = pkt.readWString(64);
                int cur = 0, max = 0;
                if (pkt.remaining() >= 8) { cur = pkt.readInt32(); max = pkt.readInt32(); }
                if (rid) {
                    std::string n8;
                    for (char16_t c : name) n8.push_back(c < 128 ? static_cast<char>(c) : '?');
                    printf("[LOBBY] room %u  %-32s %d/%d\n", rid, n8.c_str(), cur, max);
                    if (!joinTargetName.empty() && name == joinTargetName) {
                        joinedRoom = true;
                        joinRoomById(rid);
                    } else if (joinTargetName.empty() && (max == 0 || cur < max)) {
                        // no target asked for take the freshest room with a free seat
                        bestRoom = rid;
                    }
                }
            }
            break;
        case 0x33:  // ready echo the join bot is now marked ready so canStart passes
            if (hostMode && inLobby && !roomStarted) {
                printf("[HOST] guest is ready, pressing START\n");
                sendStart();
                roomStarted = true;
                startPressAt = net.now() + 6.0;   // retry the press until the GO lands do not drive yet a real client waits for the server GO

            }
            break;
        default: break;
        }
    }
};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: KnC-Headless <host> <loginPort> [user] [pass] [outLog]\n");
        return 1;
    }
    std::string host = argv[1];
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    std::string user = argc > 3 ? argv[3] : "test";
    // remember our login name so we can find our own row in the room member list
    std::string pass = argc > 4 ? argv[4] : "test";
    std::string logPath = argc > 5 ? argv[5] : "wire.log";
    std::string wpFile = argc > 6 ? argv[6] : "";

    WireLog log;
    if (!log.open(logPath)) { printf("cannot open %s\n", logPath.c_str()); return 1; }
    printf("[HEADLESS] logging wire to %s\n", logPath.c_str());

    // structured catalog dump next to the raw wire this is the data compared some rows are custom
    FILE* dec = nullptr;
    fopen_s(&dec, (logPath + ".decoded").c_str(), "w");

    NetClient net(&log);
    Flow flow{ net, host, user, pass };
    for (char c : user) flow.myName.push_back(static_cast<char16_t>(c));
    flow.testMode = (wpFile == "test");           // arg 6 "test" runs the lobby actions arg 6 "chat" says arg 8 in the lobby
    flow.chatMode = (wpFile == "chat");
    if (flow.chatMode) { flow.chatText = argc > 7 ? argv[7] : "/clan info"; wpFile.clear(); }
    flow.tourMode = (wpFile == "tour");           // arg 6 "tour" walks every screen arg 6 "room" just creates a room
    flow.roomMode = (wpFile == "room");
    if (flow.testMode || flow.roomMode) wpFile.clear();
    const std::string mode7 = argc > 7 ? argv[7] : "";
    const bool ghost = mode7 == "ghost";
    flow.boostMode = mode7 == "boost";
    flow.hostMode = mode7 == "host" && !wpFile.empty();
    flow.joinMode = mode7 == "join" && !wpFile.empty();
    flow.wpFile = wpFile;
    flow.ghostMode = ghost && !wpFile.empty();
    if (argc > 8) flow.ghostTrack = atoi(argv[8]);   // track id differs per server
    if (argc > 8 && (flow.hostMode || flow.roomMode)) flow.hostTrack = atoi(argv[8]);
    if (argc > 9) flow.ghostTargetMs = atoi(argv[9]); // exact time to submit in ms scales the replayed human cadence under 1 is faster

    if (argc > 10) { const double v = atof(argv[10]); if (v > 0.1 && v < 5.0) flow.racePace = v; }
    flow.driveMode = !wpFile.empty() && !flow.ghostMode && !flow.hostMode && !flow.joinMode;
    if (flow.hostMode) printf("[HEADLESS] host mode\n");
    else if (flow.joinMode) printf("[HEADLESS] join mode\n");
    if (flow.testMode) printf("[HEADLESS] test mode, buy equip room\n");
    else if (flow.ghostMode) printf("[HEADLESS] ghost record mode, waypoints=%s\n", wpFile.c_str());
    else if (flow.driveMode) printf("[HEADLESS] drive mode, waypoints=%s\n", wpFile.c_str());
    net.onFrame = [&](uint16_t op, Packet& p) {
        flow.onFrame(op, p);
        std::string line = decodeCatalog(op, p);
        if (!line.empty()) {
            printf("  %s\n", line.c_str());
            if (dec) { fprintf(dec, "%s\n", line.c_str()); fflush(dec); }
        }
    };

    if (!net.connect(host, port)) { printf("[HEADLESS] connect %s:%u failed\n", host.c_str(), port); return 1; }
    printf("[HEADLESS] connected to %s:%u, starting handshake\n", host.c_str(), port);
    flow.authSequence();

    double lastBeat = net.now();
    double lastMotion = net.now();
    while (net.connected()) {
        if (!net.pump(100)) break;
        if (net.now() - lastBeat >= 1.0) {  // 0xA6 every second like the real client
            Packet hb(CMD::C_HEARTBEAT);
            hb.writeInt32(0);
            net.send(hb);
            lastBeat = net.now();
            // solo host start once the server has had time to seat its bot
            if (flow.hostMode && !flow.hostAutoStarted && flow.hostRoomUpAt > 0
                && !flow.raceGo && net.now() - flow.hostRoomUpAt >= 30.0) {
                flow.hostAutoStarted = true;
                printf("[HOST] no one joined, starting with the server bot\n");
                Packet go = Packet::fromCmdFull(CMD::C_ROOM_READY_TOGGLE);
                go.writeInt32(1);
                net.send(go);
            }
        }
        // walk every screen once in the lobby one every three seconds
        if (flow.tourMode && flow.inLobby && net.now() >= flow.tourAt) {
            flow.runTourStep();
        }
        // look like a client not a packet generator sizes and bodies match a real captured client
        if (flow.raceGo && !flow.racePrimed) {
            flow.racePrimed = true;
            Packet slots = Packet::fromCmdFull(0x00CF);   // item slots empty is minus 1
            slots.writeInt32(6); slots.writeInt32(-1); slots.writeInt32(-1);
            net.send(slots);
            Packet st = Packet::fromCmdFull(0x0058);      // one byte
            st.writeUInt8(3);
            net.send(st);
            Packet pd = Packet::fromCmdFull(0x0049);
            pd.writeInt32(6); pd.writeInt32(0);
            net.send(pd);
        }
        // idle motion in the waiting room its own tiny coordinate space sending track coordinates threw the bot out
        if (flow.inRoomConfirmed && !flow.raceLaunched && !flow.raceGo && !flow.driving
            && net.now() - lastMotion >= 0.1) {
            lastMotion = net.now();
            float x = 0, y = 0, z = 0, tx = 1, ty = 0, tz = 0; uint8_t yaw = 0;
            x = -34.0f; y = 10.4f; z = 0.55f;   // inside the waiting room box parked both words identical
            tx = x; ty = y; tz = z;
            yaw = 64;
            Packet m = Packet::fromCmdFull(CMD::C_MOTION);
            auto body = buildMotionBodyPacked(x, y, z, tx, ty, tz, yaw, 0x0702);
            m.writeBytes(body.data(), body.size());
            net.send(m);
        }
        // finished the second word is a forward projection keep saying we are parked or every client dead reckons us away
        if (flow.raceGo && flow.driver.done() && net.now() - lastMotion >= 0.1) {
            lastMotion = net.now();
            float fx = 0, fy = 0, fz = 0, ftx = 0, fty = 0, ftz = 0; uint8_t fyaw = 0;
            flow.driver.peek(fx, fy, fz, ftx, fty, ftz, fyaw);
            Packet m = Packet::fromCmdFull(CMD::C_MOTION);
            auto body = buildMotionBodyPacked(fx, fy, fz, fx, fy, fz, fyaw, 0x0702);
            m.writeBytes(body.data(), body.size());
            net.send(m);
        }
        // runs at 50Hz for the whole race measured on a human session
        if (flow.raceGo && !flow.raceFinished && net.now() >= flow.tickAt) {
            Packet t = Packet::fromCmdFull(0x0067);
            t.writeInt32(0);
            net.send(t);
            flow.tickAt = net.now() + 0.020;   // real client medians 20ms 50Hz
        }
        // sub 40C950 master start press only the master press makes the server answer with the GO
        if (flow.roomStarted && !flow.raceGo && flow.startPressAt > 0
            && net.now() >= flow.startPressAt && flow.startPresses < 6) {
            ++flow.startPresses;
            printf("[JOIN] pressing START as room master, try %d\n", flow.startPresses);
            Packet st = Packet::fromCmdFull(CMD::C_ROOM_READY_TOGGLE);
            st.writeInt32(1);
            net.send(st);
            flow.startPressAt = net.now() + 6.0;
        }
        // 10Hz waypoint motion once racing and the countdown passed sit on the grid slot facing the grid way
        if (flow.raceLaunched && !flow.driving && !flow.gridSlots.empty()
            && net.now() - lastMotion >= 0.1) {
            lastMotion = net.now();
            const Waypoint& g = flow.gridSlots[flow.gridSlot % flow.gridSlots.size()];
            const uint8_t gy = static_cast<uint8_t>(
                static_cast<int>(g.heading / 360.0f * 256.0f) & 0xFF);
            Packet m = Packet::fromCmdFull(CMD::C_MOTION);
            auto body = buildMotionBodyPacked(g.x, g.y, g.z, g.x, g.y, g.z, gy, 0x0702);
            m.writeBytes(body.data(), body.size());
            net.send(m);
            flow.lastX = g.x; flow.lastY = g.y;
        }
        // a stunned kart loses real time it stops advancing and stops ticking
        if (flow.driving && net.now() >= flow.raceGoAt && net.now() >= flow.stunUntil
            && !flow.driver.done()) {
            // a stun or pause leaves the last motion stale so the next step integrates seconds of dt as a teleport
            if (!flow.motionInit) { flow.motionInit = true; lastMotion = net.now(); }
            if (net.now() - lastMotion > 0.5) lastMotion = net.now();
          if (net.now() - lastMotion >= 0.1) {
            double dt = (net.now() - lastMotion) * 1000.0;
            lastMotion = net.now();
            float x, y, z, tx, ty, tz; uint8_t yaw;
            if (flow.driver.step(dt, x, y, z, tx, ty, tz, yaw)) {
                // the server observes drift and mini turbo boost by relaying steering sign from the yaw delta
                int d = static_cast<int>(yaw) - static_cast<int>(flow.prevYaw);
                if (d > 128) d -= 256; else if (d < -128) d += 256;
                // a yaw jump of half a turn in one frame reads as facing backwards so ease into big changes
                if (d > 64 || d < -64) {
                    const int step = d > 0 ? 24 : -24;
                    yaw = static_cast<uint8_t>((static_cast<int>(flow.prevYaw) + step) & 0xFF);
                    d = step;
                }
                flow.prevYaw = yaw;
                uint8_t f17 = 0;
                if (d < 0) f17 |= 0x02;             // steering left steering right hysteresis a real drift is held through a corner for 1 to 2dot7 seconds
                else if (d > 0) f17 |= 0x04;

                const int ad = d < 0 ? -d : d;
                // keep a started drift alive for at least 0dot9s unless the road is genuinely straight
                const bool held = flow.driftSince != 0
                              && net.now() - flow.driftSince < 0.9;
                const bool turning = held || (flow.driftSince != 0 ? (ad >= 1) : (ad >= 2));
                if (turning) {
                    if (flow.driftSince == 0) flow.driftSince = net.now();
                    f17 |= 0x10;                    // drifting charged
                    if (net.now() - flow.driftSince > 0.3) f17 |= 0x08;
                } else {
                    // only a charged drift pays out the client lights the charge flag at about 0dot3s
                    if (flow.driftSince != 0 && net.now() - flow.driftSince >= 0.35) {
                        f17 |= 0x80;                // the mini boost
                        Packet ev = Packet::fromCmdFull(0x0058);
                        ev.writeUInt8(0x08);        // the drift boost event the client sends
                        net.send(ev);
                        flow.boostUntil = net.now() + 0.9;
                        printf("[DRIFT] held %.2f s, mini boost\n",
                               net.now() - flow.driftSince);
                    }
                    flow.driftSince = 0;
                }
                if (net.now() < flow.boostUntil) f17 |= 0x80;
                if (flow.boostMode) f17 |= 0x40;
                // byte 18 speed 0 to 11 low and drift charge high with 7 neutral
                const double sp = 60.0;
                uint8_t lo = static_cast<uint8_t>(sp / 85.0 * 11.0); if (lo > 11) lo = 11;
                uint8_t hi = 7;
                if (flow.driftSince != 0) {
                    const int ramp = static_cast<int>((net.now() - flow.driftSince) / 0.15);
                    hi = static_cast<uint8_t>((f17 & 0x02) ? 7 - (ramp > 5 ? 5 : ramp)
                                                          : 7 + (ramp > 5 ? 5 : ramp));
                }
                const uint16_t sb = static_cast<uint16_t>(f17 | ((hi << 4 | lo) << 8));
                Packet m = Packet::fromCmdFull(CMD::C_MOTION);
                // packed motion is the only form other players can see project a short way along the heading instead
                const float lead = 24.0f;
                auto body = flow.packedMotion
                    ? buildMotionBodyPacked(x, y, z,
                                            x + tx * lead, y + ty * lead, z + tz * lead,
                                            yaw, sb ? sb : 0x7A02)
                    : buildMotionBody(x, y, z, tx, ty, tz, yaw, sb);
                m.writeBytes(body.data(), body.size());
                net.send(m);
                flow.lastX = x; flow.lastY = y;

                // item box pickup decided client side capture shows grant then status then slot mirror within 60ms
                if (flow.heldItem < 0) {
                    for (size_t bi = 0; bi < flow.itemBoxes.size(); ++bi) {
                        if (flow.boxTaken[bi]) continue;
                        const float dx2 = flow.itemBoxes[bi].x - x;
                        const float dy2 = flow.itemBoxes[bi].y - y;
                        // boxes sit a median 17 units off the racing line a 20 unit reach catches most of them
                        if (dx2 * dx2 + dy2 * dy2 > 400.0f) continue;
                        flow.boxTaken[bi] = 1;
                        flow.itemRng = flow.itemRng * 1103515245u + 12345u;
                        static const int kItems[] = { 0, 3, 6, 10, 14, 16, 18 };
                        flow.heldItem = kItems[(flow.itemRng >> 16) % 7];
                        Packet g = Packet::fromCmdFull(0x0049);
                        g.writeInt32(flow.heldItem); g.writeInt32(0);
                        net.send(g);
                        Packet st = Packet::fromCmdFull(0x0058);
                        st.writeUInt8(0x07);
                        net.send(st);
                        Packet sl = Packet::fromCmdFull(0x00CF);
                        sl.writeInt32(flow.heldItem); sl.writeInt32(-1); sl.writeInt32(-1);
                        net.send(sl);
                        flow.useItemAt = net.now() + 2.0 + (flow.itemRng % 3000) / 1000.0;
                        printf("[ITEM] picked up %d\n", flow.heldItem);
                        break;
                    }
                } else if (net.now() >= flow.useItemAt) {
                    // item id then the world position then the heading in degrees
                    Packet u = Packet::fromCmdFull(0x0047);
                    u.writeInt32(flow.heldItem);
                    u.writeFloat(x); u.writeFloat(y); u.writeFloat(z);
                    u.writeFloat(static_cast<float>(yaw) * 360.0f / 256.0f);
                    net.send(u);
                    Packet sl = Packet::fromCmdFull(0x00CF);
                    sl.writeInt32(-1); sl.writeInt32(-1); sl.writeInt32(-1);
                    net.send(sl);
                    printf("[ITEM] used %d\n", flow.heldItem);
                    flow.heldItem = -1;
                }
                if (flow.ghostMode) {
                    flow.ghostSample(dt, x, y, z, yaw);   // record the lap as we drive stop at the target clock so the submitted time is exact

                    if (flow.ghostTargetMs > 0 && !flow.ghostDone &&
                        flow.ghostFrameCount * 200u >= static_cast<uint32_t>(flow.ghostTargetMs)) {
                        flow.ghostFinish();
                    }
                } else {
                    // report checkpoint faces so the room race counts laps
                    uint32_t cp0, cp1;
                    while (flow.driver.popCheckpoint(cp0, cp1)) {
                        Packet cp = Packet::fromCmdFull(0x41);
                        cp.writeUInt32(cp0);
                        cp.writeUInt32(cp1);
                        net.send(cp);
                    }
                    // chibikart wants an explicit lap complete per lap the stock client never sends the auto detect opcode
                    if (flow.driver.lapsRun() > flow.lastLapsSent) {
                        flow.lastLapsSent = flow.driver.lapsRun();
                        const int32_t lapMs = static_cast<int32_t>((net.now() - flow.raceGoAt) * 1000.0 / flow.lastLapsSent);
                        Packet lap = Packet::fromCmdFull(0x36);   // lap u8 lap i32 time
                        lap.writeUInt8(static_cast<uint8_t>(flow.lastLapsSent));
                        lap.writeInt32(lapMs);
                        net.send(lap);
                        printf("[DRIVE] lap %d complete %d ms\n", flow.lastLapsSent, lapMs);
                    }
                }
                if (flow.driver.done()) {
                    if (flow.ghostMode && !flow.ghostDone) flow.ghostFinish();
                    else if (!flow.ghostMode) {
                        printf("[DRIVE] race done, %d laps\n", flow.driver.lapsRun());
                        // sub 40D9D0 proves this is a room list click gated on being master the server ends races itself
                        flow.raceFinished = true;
                        printf("[DRIVE] race done, waiting for the server to score it\n");
                    }
                }
            }
          }
        }
    }
    printf("[HEADLESS] disconnected\n");
    log.close();
    if (dec) fclose(dec);
    return 0;
}
