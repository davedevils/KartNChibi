// the way in as one state machine login channel redirect reauth lobby chat room gacha licence and ghost
#pragma once

#include "Catalog.h"
#include "NetClient.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace KnC::Client {

// one row of the 0x000E channel list
struct ChannelRow {
    uint32_t id = 0;
    std::u16string name;
    uint32_t population = 0;
    uint32_t capacity = 0;
    uint32_t tier = 0;
};

// the proven fields of the 0x0007 profile blob offsets in the opcode page
struct Profile {
    bool valid = false;
    uint32_t playerId = 0;
    uint32_t reauthSessionId = 0;
    std::u16string reauthToken;
    std::u16string nickname;
    uint8_t band = 0;
    uint8_t level = 0;
    uint32_t exp = 0;
    uint32_t astro = 0;
    uint32_t gold = 0;
    int32_t characterInstance = -1;
    int32_t kartInstance = -1;
    uint8_t rolePrivilege = 0;
    uint32_t expFloor = 0;
    uint32_t expNext = 0;
    // the worn pendant 0x01A20B2C blob 0x4C4 0x000A plus 0x22 and the 0x0123 ack
    uint32_t pendantKey = 0;
};

// one 0x002D lobby row the seven values keep their wire order their names are inferred
struct RoomRow {
    uint32_t roomId = 0;
    std::u16string name;
    uint32_t value[7] = {0, 0, 0, 0, 0, 0, 0};
};

// one 0x00B4 chat broadcast type 5 is a 0x0126 system line type 6 a 0x00B5 whisper
struct ChatLine {
    uint32_t playerId = 0;
    std::u16string sender;
    std::u16string text;
    uint32_t type = 0;
    // a whisper the local player sent the sender is then the partner
    bool outgoing = false;
};

// a server message box 0x0001 carries an ascii key 0x0002 carries wide text
struct ServerMessage {
    bool wide = false;
    std::string key;
    std::u16string text;
    uint32_t boxType = 0;
    bool closesConnection = false;
};

// one 0x0021 member the two keys come from the character and kart blobs at their offset 4
struct RoomMember {
    uint32_t slot = 0;
    uint32_t team = 0;
    uint32_t playerId = 0;
    std::u16string name;
    uint8_t level = 0;
    uint32_t driverKey = 0;
    uint32_t kartKey = 0;
    // the five BODYSET part keys of the character blob at 0x08 O BODY to O BACK
    std::array<uint32_t, 5> accessory{};
    // paint plate and antenna part keys of the kart blob at 0x08 the def row stands in for a zero
    std::array<uint32_t, 3> kartParts{};
    // the 0x3C custom car block the chassis key then seven part key and grade pairs of a factory kart
    std::array<uint32_t, 15> customCar{};
    uint32_t ready = 0;
    // the u32 after the pccafe byte sub 499180 draws its pendant left of the room name plate
    uint32_t pendantKey = 0;
    // L plus 0x6F the worn pet sub 40D650 keeps and sub 40CC90 loads on the room stand
    uint32_t petKey = 0;
};

// one decor record of the 0x0013 tail the room craft world of the master
struct RoomDecor {
    uint32_t instanceId = 0;
    uint32_t catalogKey = 0;
    // 0 sky 1 floor 2 back object 3 prop 4 effect
    uint32_t category = 0;
    float x = 0.f, y = 0.f, z = 0.f;
    float yawDeg = 0.f;
    uint32_t placed = 0;
    uint32_t valid = 0;
};

// the 0x006C room invite the popup shows accept joins with 0x002F decline answers 0x006D
struct RoomInvite {
    uint32_t replyKey = 0;
    std::u16string inviter;
    uint32_t roomId = 0;
    std::u16string password;
    uint8_t flag = 0;
};

// the 0x0013 room context and what the room packets after it changed
struct RoomState {
    bool inRoom = false;
    uint32_t roomId = 0;
    std::u16string name;
    uint32_t maxPlayers = 8;
    uint32_t gameMode = 0;
    uint32_t hasPassword = 0;
    uint32_t masterPlayerId = 0;
    uint32_t weather = 0;
    int32_t trackId = 0;
    uint32_t trackWeather = 0;
    bool slotEnabled[30] = {};
    std::vector<RoomMember> members;
    // 0x0034 or 0x0033 state 2 every seat is marked ready the race is about to launch
    bool allReady = false;
    // the decor tail of 0x0013 the placed room craft rows of the master
    std::vector<RoomDecor> decor;
};

// the 0x0014 scene change that launches a race
struct RaceLaunch {
    uint32_t sceneKind = 0;
    uint32_t trackId = 0;
    uint32_t gameMode = 0;
    uint32_t playerCount = 0;
    uint32_t flag = 0;
};

// one 0x0087 mission definition the four strings are fixed char 33 slots inside the record
struct MissionDef {
    uint32_t missionId = 0;
    uint32_t kind = 0;
    int32_t goalCount = 0;
    int32_t timeLimitMs = 0;
    uint32_t rewardExtra = 0;
    uint32_t rewardMileage = 0;
    uint32_t rewardExp = 0;
    uint32_t rewardItemType = 0;
    uint32_t rewardItemKey = 0;
    std::string worldName;
    std::string titleKey;
    std::string subKey;
    std::string descKey;
};

// one 0x0088 or 0x008A progress row
struct MissionProgress {
    uint32_t missionId = 0;
    uint32_t cleared = 0;
};

// the 0x0090 ack and the 0x0120 path of the mission being run
struct MissionRun {
    bool active = false;
    uint32_t missionId = 0;
    uint32_t goldAfter = 0;
    struct Point { float x = 0.f, y = 0.f, z = 0.f; };
    std::vector<Point> path;
    bool go = false;
    // 0x008C landed
    bool complete = false;
    uint32_t completeGold = 0;
    uint32_t completeExp = 0;
};

// one 0x00A2 licence progress record the third dword has no reader in the stock
struct LicenceProgressRow {
    uint32_t key = 0;
    uint32_t passed = 0;
};

// one 0x0076 friend record status a below zero draws the name in colour 2
struct FriendRow {
    uint32_t playerId = 0;
    std::u16string name;
    uint32_t level = 0;
    int32_t statusA = -2;
    int32_t statusB = -2;
};

// one 0x0078 pending request record
struct FriendRequestRow {
    uint32_t playerId = 0;
    std::u16string name;
    uint32_t level = 0;
};

// one 0x0079 block record
struct BlockRow {
    uint32_t playerId = 0;
    std::u16string name;
};

// the 0x0072 answer the 0x68 profile blob of one player the messenger draws it beside the row
struct UserInfoCard {
    bool valid = false;
    uint32_t playerId = 0;
    std::u16string name;
    uint32_t level = 0;
    uint32_t driverKey = 0;
    uint32_t exp = 0;
    uint32_t expFloor = 0;
    uint32_t expNext = 0;
    int32_t pendant = 0;
};

// one 0x0119 pendant definition the icon base feeds Icon base NN and Icon base s
struct PendantDef {
    bool visible = true;
    uint32_t key = 0;
    std::string iconBase;
    std::string nameKey;
    std::string descKey;
};

// one 0x011A or 0x011B owned row sub 451250 removes by key then erases by the instance
struct OwnedPendant {
    uint32_t instance = 0;
    uint32_t key = 0;
};

// the 0x20 car config of a car craft preset the first dword is the kart instance
struct CarConfig {
    uint32_t kartInstance = 0;
    // cover tires booster bumper front fender rear fender wing
    std::array<uint32_t, 7> slot{};
};

// the 0x00ED answer sub 47D5E0 five dwords the echoed ticket row then the typed prize record
struct GachaResult {
    bool valid = false;
    uint32_t rareFlag = 0;
    // 0 driver 1 kart 2 item 3 part 4 room object 5 car craft 6 and up nothing came
    uint32_t category = 6;
    uint32_t baseKey = 0;
    uint32_t periodMode = 0;
    int32_t periodValue = 0;
    // the ticket row after the roll the count on it is what is left
    OwnedItem ticket;
};

// one 0x00AC or 0x00B0 RecordEntry the client reads the name at plus 4 and the time at plus AC
struct GhostRecordRow {
    std::u16string name;
    int32_t timeMs = 0;
};

// the 0x00AC board of one track the best row then the entries in rank order
struct GhostTrackBoard {
    uint32_t trackId = 0;
    GhostRecordRow best;
    std::vector<GhostRecordRow> entries;
};

// one 28 byte ReplayFrame of 0x00AF as the wire carries it
struct GhostFrame {
    float pos[3] = {0.f, 0.f, 0.f};
    uint8_t yawByte = 0;
    uint32_t flags = 0;
    uint32_t nibbles = 0;
    uint8_t inputMask = 0;
};

// the 0x00AA session the ghost car and the frames of 0x00AE and 0x00AF that came before it
struct GhostSession {
    bool valid = false;
    uint32_t trackId = 0;
    std::u16string name;
    uint32_t carKind = 3;
    int32_t recordTimeMs = 0;
    uint32_t driverKey = 0;
    uint32_t kartKey = 0;
    std::vector<GhostFrame> frames;
};

// the 0x00B0 answer the rank the own record and up to three rows
struct GhostSubmitResult {
    bool valid = false;
    uint32_t rank = 0;
    GhostRecordRow mine;
    std::vector<GhostRecordRow> top;
};

// the 0x00A3 answer the progress row of the test then the currency and item tails
struct LicenceTestResult {
    bool valid = false;
    uint32_t key = 0;
    uint32_t passed = 0;
    bool hasCurrency = false;
    bool hasItem = false;
};

// one 0x0082 or 0x0083 note record the strings sit inside the fixed 396 byte blob
struct NoteRow {
    uint32_t noteId = 0;
    std::u16string sender;
    std::u16string sortKey1;
    std::u16string sortKey2;
    bool read = false;
    std::u16string body;
};

enum class Stage { Idle, LoginServer, ChannelPick, GameServer, Lobby, Room, Race, Closed };

class Session {
public:
    explicit Session(WireLog* log = nullptr);
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    void setLog(WireLog* log) { m_net.setLog(log); }

    // opens the login server socket nothing is sent yet
    bool connect(const std::string& host, uint16_t port);
    // 0x00FA then 0x0007 with the version the action 4 and the two wide strings
    void login(const std::string& user, const std::string& pass);
    void selectChannel(int32_t channelId) { m_channel = channelId; }
    // 0x0018 with the stage and the channel the menu multi button path
    void requestStage(uint32_t stage, int32_t channelId);
    // the sub 405D60 routing rule 14 for the licence screen else 8 the lobby
    uint32_t stageForProfile() const;
    // 0x0012 with no body the open lobby meaning from a menu and the leave meaning from a room
    void openLobby();
    // sends 0x0019 with host port and mode 4 sub 4817E0 stock reconnects ours re logs in for fresh channel list
    void returnToChannels();
    // true while the channel return is in flight
    bool returningToChannels() const { return m_channelReturn != 0; }
    // 0x0130 the deny invitation flag as a float the lobby tail and the option box OK send it
    void sendOptionReport(float denyInvites);
    // 0x00B4 wide text then type 0
    void sendChat(const std::u16string& text);

    // 0x002D name password max users mode flag and the private flag
    void createRoom(const std::u16string& name, uint32_t maxUsers, uint32_t mode, bool isPrivate);
    // 0x002F room id then the password
    void joinRoom(uint32_t roomId, const std::u16string& password);
    // 0x0035 track id then weather the master only
    void selectTrack(uint32_t trackId, uint32_t weather);
    // 0x0033 the new ready value a guest or the start press of the master
    void sendReadyToggle(bool pressed);
    // 0x003B the race exit our server answers with the lobby mid race
    void leaveRace();
    // 0x000F the garage stage the server answers with the same empty opcode
    void openGarage();
    // 0x0016 with no body the licence screen request
    void openLicense();
    // 0x012F with no body the random invite of the room master
    void requestRandomInvite();
    // 0x006D with the reply key and the answer code 1 declined
    void answerInvite(uint32_t replyKey, uint32_t code);
    const RoomInvite& invite() const { return m_invite; }
    // 0x0010 the shop stage the price table comes first then the ack
    void openShop();
    // 0x00B9 category then the owned record base key the third dword stays minus one
    void equipUse(uint32_t category, uint32_t baseKey);
    // 0x00BA category then the owned record base key
    void unequip(uint32_t category, uint32_t baseKey);
    // 0x00B7 category base key price key then the account name as a wide string
    void buy(uint32_t category, uint32_t baseKey, int32_t priceKey);
    // 0x0004 the driver key and the nickname the creation popup answer
    void createCharacter(uint32_t driverKey, const std::u16string& nickname);
    // 0x002D with a password the private flag follows it
    void createRoom(const std::u16string& name, const std::u16string& password, uint32_t maxUsers, uint32_t mode);
    // 0x0064 inside a room the team 0 red 1 blue
    void selectTeam(uint32_t team);
    // 0x0064 from the lobby the preferred mode
    void quickMatch(int32_t mode);
    // 0x008F the mission menu the defs and the progress list come before the ack
    void openMissionMenu();
    // the init tail of stage 24 0x00FA then 0x008E as the lobby sends them
    void sendStageTail();
    // 0x0090 mission id
    void startMission(uint32_t missionId);
    // 0x0121 the checkpoint index after the increment the rally world of stage 11 only
    void sendMissionCheckpoint(uint32_t index);
    // 0x0123 a pendant key or minus one to take it off the popup stays locked until the ack
    void equipPendant(int32_t key);
    const std::vector<PendantDef>& pendantDefs() const { return m_pendantDefs; }
    const PendantDef* pendantDef(uint32_t key) const;
    const std::vector<OwnedPendant>& ownedPendants() const { return m_ownedPendants; }
    bool ownsPendant(uint32_t key) const;
    // the lock byte 0x0116E1EC of sub 483A40 only the 0x0123 ack clears it
    bool pendantLocked() const { return m_pendantLock; }
    void unlockPendant() { m_pendantLock = false; }
    // 0x008C the goal reached mission id
    void sendMissionGoal(uint32_t missionId);
    // 0x006F the target name
    void addFriend(const std::u16string& name);
    // 0x0070 accept 0x0071 reject 0x0074 delete one player id each
    void acceptFriend(uint32_t playerId);
    void rejectFriend(uint32_t playerId);
    void deleteFriend(uint32_t playerId);
    // 0x0073 empty the status poll while the messenger is open
    void pollFriendStatus();
    // 0x0081 recipient name then the body both wide
    void sendNote(const std::u16string& to, const std::u16string& body);
    // 0x0084 and 0x0085 one note id each
    void markNoteRead(uint32_t noteId);
    void deleteNote(uint32_t noteId);
    // 0x00ED the owned ticket row verbatim sub 4830C0 the answer lands on gachaResult
    void rollGacha(const OwnedItem& ticket);
    // 0x0098 category recipient base key price key and the message sub 482880
    void sendGift(uint32_t category, const std::u16string& recipient, uint32_t baseKey, int32_t priceKey,
                  const std::u16string& message);
    // 0x00B8 category then the base key sub 4844A0 the ack removes the owned rows of that key
    void deleteOwned(uint32_t category, uint32_t key);
    // 0x010E with no body the room craft stage the catalogue and the owned rows come before the ack
    void openRoomCraft();
    // 0x010F the changed 0x30 rows only sub 483450 nothing goes out when nothing moved
    void saveRoomCraft(const std::vector<RoomCraftInstance>& dirty);
    // 0x010A with no body the car craft stage the presets the defs and the instances come first
    void openCarCraft();
    // 0x010B preset id the 0x20 config the part count then the whole local part list sub 42F3E0
    void saveCarCraft(uint32_t presetId, const CarConfig& config,
                      const std::vector<CarCraftPartInstance>& parts);
    // 0x0114 preset id then the ascii name the stock caps it at nine characters
    void renameCarCraftPreset(uint32_t presetId, const std::string& name);
    // 0x0072 the name of the player sub 4822D0 the answer is the 0x68 blob of that player
    void requestUserInfo(const std::u16string& name);
    // 0x006C the name of the friend to invite into the own room sub 481CD0
    void inviteToRoom(const std::u16string& name);
    // 0x007A the name of the player to block sub 481FB0 zero means it worked
    void addBlock(const std::u16string& name);
    // 0x007B the player id of the block row sub 482230 the removal is unconditional
    void removeBlock(uint32_t playerId);
    // 0x011D with no body the ghost menu request the board comes before the ack
    void openGhostMenu();
    // 0x00AA the track id FUN 00439160 the frames then the session come back on the same opcode
    void enterGhost(uint32_t trackId);
    // 0x00B1 and 0x00B2 with no body the ghost stage begin and the final lap line
    void ghostStageBegin();
    void ghostFinalLap();
    // 0x00AE the count then the 0x00AF chunks of 136 then 0x00B0 track time and car kind
    void uploadGhost(const std::vector<GhostFrame>& frames, uint32_t trackId, uint32_t timeMs, uint32_t carKind);
    // 0x0062 with no body the licence test start the empty ack opens the run
    void startLicenceTest();
    // 0x00A3 the key and the two def echoes sub 482C40
    void submitLicenceTest(uint32_t key, uint32_t testParam, uint32_t rewardGold);
    // any packet the race side builds itself
    void send(const Packet& pkt) { m_net.send(pkt); }

    // pumps the socket answers pings and sends the 0x00A6 keepalive once a second
    void update();
    void disconnect();
    bool connected() const { return m_net.connected(); }

    Stage stage() const { return m_stage; }
    const Profile& profile() const { return m_profile; }
    const Catalog& catalog() const { return m_catalog; }
    // the car craft refcounts live on the owned part rows the factory stage mutates them locally
    Catalog& catalogForEdit() { return m_catalog; }
    const std::vector<ChannelRow>& channels() const { return m_channels; }
    const std::u16string& notice() const { return m_notice; }
    int32_t selectedChannel() const { return m_channel; }
    const std::vector<RoomRow>& rooms() const { return m_rooms; }
    const std::vector<ChatLine>& chat() const { return m_chat; }
    const RoomState& room() const { return m_room; }
    const RaceLaunch& raceLaunch() const { return m_launch; }
    const std::string& loginHost() const { return m_loginHost; }
    const std::string& gameHost() const { return m_gameHost; }
    uint16_t gamePort() const { return m_gamePort; }
    const std::string& lastError() const { return m_error; }
    double now() const { return m_net.now(); }
    bool isRoomMaster() const { return m_room.inRoom && m_room.masterPlayerId == m_profile.playerId; }
    const RoomMember* member(uint32_t playerId) const;
    const std::vector<MissionDef>& missionDefs() const { return m_missionDefs; }
    const std::vector<MissionProgress>& missionProgress() const { return m_missionProgress; }
    // the 0x00A2 rows key is test index plus ten times the grade passed 1 on a cleared test
    const std::vector<LicenceProgressRow>& licenceProgress() const { return m_licenceProgress; }
    const LicenceProgressRow* licenceState(uint32_t key) const;
    const MissionDef* missionDef(uint32_t missionId) const;
    const MissionProgress* missionState(uint32_t missionId) const;
    MissionRun& missionRun() { return m_missionRun; }
    const std::vector<FriendRow>& friends() const { return m_friends; }
    const std::vector<FriendRequestRow>& friendRequests() const { return m_friendRequests; }
    const std::vector<BlockRow>& blocks() const { return m_blocks; }
    const UserInfoCard& userInfoCard() const { return m_userInfo; }
    const std::vector<NoteRow>& notes() const { return m_notes; }
    // the last 0x006F 0x0074 or 0x0081 code minus one when none landed
    int32_t lastSocialCode() const { return m_socialCode; }
    // the last 0x0004 result minus one before the answer
    int32_t createResult() const { return m_createResult; }
    // 0x002A raised it 0x002B cleared it
    bool whisperPrompt() const { return m_whisperPrompt; }
    // the last 0x00ED answer valid once one landed since the roll
    const GachaResult& gachaResult() const { return m_gacha; }
    // the 0x00AB and 0x00AC board in arrival order 0x00AB clears it
    const std::vector<GhostTrackBoard>& ghostBoard() const { return m_ghostBoard; }
    const GhostTrackBoard* ghostBoardOf(uint32_t trackId) const;
    // the last 0x00AA session with its frames
    const GhostSession& ghostSession() const { return m_ghostSession; }
    // the last 0x00B0 answer
    const GhostSubmitResult& ghostResult() const { return m_ghostResult; }
    // the last 0x00A3 answer
    const LicenceTestResult& licenceTestResult() const { return m_licenceResult; }
    // the driver and kart keys of the local player from the owned lists and the profile selection
    uint32_t myDriverKey() const;
    uint32_t myKartKey() const;

    std::function<void()> onChannelList;
    std::function<void()> onProfile;
    std::function<void()> onMenuAck;
    std::function<void()> onLobbyAck;
    std::function<void()> onGarageAck;
    std::function<void()> onShopAck;
    // an owned list changed through 0x00B7 0x00B9 0x00BA or 0x00BC
    std::function<void(uint32_t category)> onInventoryChanged;
    std::function<void(uint32_t category)> onBuyOk;
    std::function<void()> onRoomsChanged;
    std::function<void()> onCharacterCreate;
    // the 0x0004 answer landed the result is on createResult
    std::function<void()> onCharacterCreated;
    // 0x0016 the licence stage ack
    std::function<void()> onLicenseAck;
    // 0x008F the mission menu ack the defs and the progress are stored
    std::function<void()> onMissionMenu;
    // 0x0090 the start ack then 0x0122 the go then 0x008C the complete
    std::function<void()> onMissionStart;
    std::function<void()> onMissionGo;
    std::function<void()> onMissionComplete;
    // a pendant definition an owned row or the worn key changed
    std::function<void()> onPendantChanged;
    // any friend request block or note list changed
    std::function<void()> onSocialChanged;
    // 0x0063 the quick match created a room its id is the argument
    std::function<void(uint32_t)> onQuickRoom;
    std::function<void()> onRoomInvite;
    // 0x00ED landed the result is on gachaResult
    std::function<void()> onGachaResult;
    // 0x010E the room craft stage ack and 0x010F the save ack
    std::function<void()> onRoomCraftAck;
    std::function<void()> onRoomCraftSaved;
    // 0x010A the car craft stage ack 0x010B the save result and 0x0114 the rename ack
    std::function<void()> onCarCraftAck;
    std::function<void()> onCarCraftSaved;
    // 0x0098 landed the wallet moved
    std::function<void()> onGiftOk;
    // 0x011D the ghost menu ack the board is complete
    std::function<void()> onGhostMenuAck;
    // 0x00AA landed the session and its frames are on ghostSession
    std::function<void()> onGhostSession;
    // 0x00B0 landed the answer is on ghostResult
    std::function<void()> onGhostResult;
    // 0x0062 the empty ack that opens the licence test run
    std::function<void()> onLicenceTestAck;
    // 0x00A3 landed the answer is on licenceTestResult
    std::function<void()> onLicenceTestResult;
    std::function<void()> onRoomEnter;
    std::function<void()> onRoomChanged;
    std::function<void()> onRoomTrack;
    std::function<void()> onRaceLaunch;
    std::function<void(const ChatLine&)> onChat;
    std::function<void(const ServerMessage&)> onMessage;
    std::function<void(const std::string& reason)> onDisconnected;
    std::function<void(uint16_t opcode, Packet& pkt)> onFrame;

private:
    void handleFrame(uint16_t op, Packet& pkt);
    void parseProfile(Packet& pkt);
    void parseRefresh(Packet& pkt);
    void parseChannelList(Packet& pkt);
    void parseRedirect(Packet& pkt);
    void parseRoomRow(Packet& pkt);
    void parseChat(Packet& pkt);
    void parseMessageKey(Packet& pkt);
    void parseMessageText(Packet& pkt);
    void parseRoomContext(Packet& pkt);
    void parseRoomMember(Packet& pkt);
    void parseSceneChange(Packet& pkt);
    void parseBuyOk(Packet& pkt);
    void parseEquipAck(Packet& pkt);
    void parseUnequipAck(Packet& pkt);
    void parseEquipmentSet(Packet& pkt);
    void parseCreateResult(Packet& pkt);
    void parseMissionDef(Packet& pkt);
    void parseMissionProgress(Packet& pkt);
    void parseMissionComplete(Packet& pkt);
    void parseFriendList(Packet& pkt);
    void parseFriendRequests(Packet& pkt);
    void parseBlockList(Packet& pkt);
    void parseFriendStatus(Packet& pkt);
    void parseFriendAddResult(Packet& pkt);
    void parseNote(Packet& pkt, bool sorted);
    void parseWhisper(Packet& pkt);
    void parseSystemLine(Packet& pkt);
    void parseGachaResult(Packet& pkt);
    void parseGiftOk(Packet& pkt);
    void parseDeleteAck(Packet& pkt);
    void parseRoomCraftSaveAck(Packet& pkt);
    void parseCarCraftSaveResult(Packet& pkt);
    void parseCarCraftRenameAck(Packet& pkt);
    void parseUserInfoBlob(Packet& pkt);
    void parseBlockAddResult(Packet& pkt);
    void parseBlockDelResult(Packet& pkt);
    void parseGhostBoardTrack(Packet& pkt);
    void parseGhostSession(Packet& pkt);
    void parseGhostResult(Packet& pkt);
    void parseLicenceTestResult(Packet& pkt);
    void sendIdRequest(uint16_t opcode, uint32_t playerId);
    // stores one owned record of the given category and returns true when it landed
    bool storeOwnedRecord(uint32_t category, Packet& pkt);
    void removeMember(uint32_t playerId);
    void sendScreenRequest();
    void sendLoginRequest();
    void sendReauth();
    void sendLobbyTail();
    void closed(const std::string& reason);
    // the second leg of the channel return the login socket and the credentials again
    void reconnectToLogin();

    NetClient m_net;
    Stage m_stage = Stage::Idle;
    std::string m_loginHost;
    uint16_t m_loginPort = 0;
    std::string m_gameHost;
    uint16_t m_gamePort = 0;
    std::string m_user;
    std::string m_pass;
    Profile m_profile;
    Catalog m_catalog;
    std::vector<ChannelRow> m_channels;
    std::u16string m_notice;
    int32_t m_channel = -1;
    std::vector<RoomRow> m_rooms;
    std::vector<ChatLine> m_chat;
    RoomState m_room;
    RoomInvite m_invite;
    UserInfoCard m_userInfo;
    RaceLaunch m_launch;
    std::string m_error;
    double m_lastBeat = 0.0;
    int32_t m_createResult = -1;
    std::vector<MissionDef> m_missionDefs;
    std::vector<MissionProgress> m_missionProgress;
    std::vector<LicenceProgressRow> m_licenceProgress;
    MissionRun m_missionRun;
    std::vector<FriendRow> m_friends;
    std::vector<FriendRequestRow> m_friendRequests;
    std::vector<BlockRow> m_blocks;
    std::vector<NoteRow> m_notes;
    int32_t m_socialCode = -1;
    bool m_whisperPrompt = false;
    GachaResult m_gacha;
    // 0x00AB announces how many 0x00AC follow a row past the count is dropped
    uint32_t m_ghostBoardExpected = 0;
    std::vector<GhostTrackBoard> m_ghostBoard;
    // the frames of the 0x00AF chunks gather here until their 0x00AA
    std::vector<GhostFrame> m_ghostIncoming;
    GhostSession m_ghostSession;
    GhostSubmitResult m_ghostResult;
    LicenceTestResult m_licenceResult;
    // 0 idle 1 the 0x0019 went out 2 the login socket is open and the login waits its second
    int m_channelReturn = 0;
    double m_channelReturnAt = 0.0;
    float m_denyInvites = 0.f;
    std::vector<PendantDef> m_pendantDefs;
    std::vector<OwnedPendant> m_ownedPendants;
    bool m_pendantLock = false;
    // sub 451250 the rows with this key go then sub 451140 appends the new one
    void storePendant(uint32_t instance, uint32_t key, bool replace);
    // the reward tail of 0x008C and 0x00A3 by type seven the pendant
    bool storeRewardRecord(uint32_t type, Packet& pkt);
    void parsePendantDef(Packet& pkt);
};

}
