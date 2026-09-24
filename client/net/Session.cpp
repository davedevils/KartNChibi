#include "Session.h"

#include "Utf.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace KnC::Client {

namespace CMD = ::knc::CMD;

namespace {

// the version cstr our server accepts the stock literal is 178 see the 0x0007 page
constexpr const char* kClientVersion = "2";
// the first login of a process is action 4 the direct credentials path
constexpr int32_t kLoginAction = 4;
// after a 0x0054 handoff the reauth carries stage 8 the lobby see the 0x0054 page
constexpr uint32_t kReauthStage = 8;
constexpr size_t kBlobSize = 0x4C8;

uint32_t rd32(const std::vector<uint8_t>& p, size_t at) {
    if (at + 4 > p.size()) return 0;
    return static_cast<uint32_t>(p[at]) | (static_cast<uint32_t>(p[at + 1]) << 8) |
           (static_cast<uint32_t>(p[at + 2]) << 16) | (static_cast<uint32_t>(p[at + 3]) << 24);
}

uint8_t rd8(const std::vector<uint8_t>& p, size_t at) {
    return at < p.size() ? p[at] : 0;
}

// wide string at a fixed offset stops on the zero or the cap
std::u16string rdWide(const std::vector<uint8_t>& p, size_t at, size_t maxChars) {
    std::u16string s;
    for (size_t k = 0; k < maxChars && at + 2 * k + 1 < p.size(); ++k) {
        const char16_t c = static_cast<char16_t>(p[at + 2 * k] | (p[at + 2 * k + 1] << 8));
        if (c == 0) break;
        s.push_back(c);
    }
    return s;
}

uint32_t u32(Packet& p) { return p.remaining() >= 4 ? p.readUInt32() : 0; }

// a RecordEntry blob of 0xB0 the wide name at plus 4 and the time at plus AC
GhostRecordRow readRecord(Packet& pkt) {
    GhostRecordRow row;
    if (pkt.remaining() < 0xB0) return row;
    const std::vector<uint8_t> blob = pkt.readBytes(0xB0);
    row.name = rdWide(blob, 4, (0xAC - 4) / 2);
    row.timeMs = static_cast<int32_t>(rd32(blob, 0xAC));
    return row;
}

// one ReplayFrame of 28 bytes as 0x00AF carries it
GhostFrame readFrame(Packet& pkt) {
    GhostFrame g;
    const std::vector<uint8_t> b = pkt.readBytes(28);
    std::memcpy(g.pos, b.data(), 12);
    g.yawByte = b[12];
    g.flags = rd32(b, 16);
    g.nibbles = rd32(b, 20);
    g.inputMask = b[24];
    return g;
}

void writeFrame(Packet& pkt, const GhostFrame& g) {
    uint8_t b[28] = {};
    std::memcpy(b, g.pos, 12);
    b[12] = g.yawByte;
    std::memcpy(b + 16, &g.flags, 4);
    std::memcpy(b + 20, &g.nibbles, 4);
    b[24] = g.inputMask;
    pkt.writeBytes(b, 28);
}

// the uploader chunks at 136 frames and clamps the own count to 2399
constexpr size_t kGhostUploadChunk = 136;
constexpr size_t kGhostUploadCap = 2399;

}

Session::Session(WireLog* log) : m_net(log) {
    m_net.onFrame = [this](uint16_t op, Packet& pkt) {
        if (onFrame) onFrame(op, pkt);
        handleFrame(op, pkt);
    };
}

bool Session::connect(const std::string& host, uint16_t port) {
    m_loginHost = host;
    m_loginPort = port;
    m_error.clear();
    if (!m_net.connect(host, port)) {
        m_error = "connect " + host + ":" + std::to_string(port) + " failed";
        m_stage = Stage::Closed;
        return false;
    }
    m_stage = Stage::LoginServer;
    m_lastBeat = m_net.now();
    return true;
}

void Session::login(const std::string& user, const std::string& pass) {
    m_user = user;
    m_pass = pass;
    sendScreenRequest();
    sendLoginRequest();
}

void Session::sendScreenRequest() {
    m_net.send(Packet(CMD::C_FULL_STATE));
}

void Session::sendLoginRequest() {
    Packet p(CMD::C_CLIENT_AUTH);
    p.writeString(kClientVersion);
    p.writeInt32(kLoginAction);
    p.writeWString(asciiToU16(m_user));
    p.writeWString(asciiToU16(m_pass));
    m_net.send(p);
}

void Session::sendReauth() {
    Packet p = Packet::fromCmdFull(0x00A7);
    p.writeString(kClientVersion);
    p.writeInt32(static_cast<int32_t>(kReauthStage));
    p.writeWString(m_profile.reauthToken);
    p.writeUInt32(m_profile.reauthSessionId);
    m_net.send(p);
}

uint32_t Session::stageForProfile() const {
    const uint8_t band = m_profile.band;
    const uint8_t level = m_profile.level;
    if (band == 0 || (band == 1 && level > 10) || (band == 2 && level > 40)) return 14;
    return 8;
}

void Session::requestStage(uint32_t stage, int32_t channelId) {
    Packet p(CMD::C_CHANNEL_SELECT);
    p.writeUInt32(stage);
    p.writeInt32(channelId);
    m_net.send(p);
}

void Session::openLobby() {
    // from a room our server answers with the lobby and a fresh list so the old rows go
    if (m_room.inRoom) m_rooms.clear();
    m_net.send(Packet::fromCmdFull(0x0012));
}

void Session::returnToChannels() {
    if (m_channelReturn != 0 || !m_net.connected()) return;
    Packet p = Packet::fromCmdFull(0x0019);
    p.writeString(m_loginHost);
    p.writeUInt32(m_loginPort);
    p.writeUInt32(4);
    m_net.send(p);
    m_channelReturn = 1;
    // the stock leaves one frame after the S2C 0x0019 ours gives the ack a moment then leaves
    m_channelReturnAt = m_net.now() + 0.3;
    std::printf("[session] channel return 0x0019 %s:%u mode 4\n", m_loginHost.c_str(), m_loginPort);
}

// the stock sleeps 1000 ms after the connect before its 0x00A7 ours does the same before the 0x0007
void Session::reconnectToLogin() {
    if (m_net.connected()) m_net.disconnect();
    m_stage = Stage::Idle;
    m_room = RoomState();
    m_rooms.clear();
    m_chat.clear();
    m_channels.clear();
    if (!m_net.connect(m_loginHost, m_loginPort)) {
        m_error = "connect " + m_loginHost + ":" + std::to_string(m_loginPort) + " failed";
        m_channelReturn = 0;
        closed(m_error);
        return;
    }
    m_stage = Stage::LoginServer;
    m_lastBeat = m_net.now();
    m_channelReturn = 2;
    m_channelReturnAt = m_net.now() + 1.0;
    std::printf("[session] channel return on the login server the login goes out in a second\n");
}

void Session::sendLobbyTail() {
    // the stock lobby init tail 0xFA then 0x8E then the option 11 report
    m_net.send(Packet::fromCmdFull(0x00FA));
    m_net.send(Packet::fromCmdFull(0x008E));
    sendOptionReport(m_denyInvites);
}

void Session::sendOptionReport(float denyInvites) {
    m_denyInvites = denyInvites;
    if (!m_net.connected()) return;
    Packet opt = Packet::fromCmdFull(0x0130);
    opt.writeFloat(denyInvites);
    m_net.send(opt);
}

void Session::sendChat(const std::u16string& text) {
    if (text.empty()) return;
    Packet p = Packet::fromCmdFull(0x00B4);
    p.writeWString(text);
    p.writeInt32(0);
    m_net.send(p);
}

void Session::createRoom(const std::u16string& name, uint32_t maxUsers, uint32_t mode, bool isPrivate) {
    Packet p = Packet::fromCmdFull(0x002D);
    p.writeWString(name);
    p.writeWString(u"");
    p.writeUInt32(maxUsers);
    p.writeUInt32(mode);
    p.writeUInt32(isPrivate ? 1u : 0u);
    p.writeUInt32(0);
    m_net.send(p);
}

void Session::joinRoom(uint32_t roomId, const std::u16string& password) {
    Packet p = Packet::fromCmdFull(0x002F);
    p.writeUInt32(roomId);
    p.writeWString(password.size() > 9 ? password.substr(0, 9) : password);
    m_net.send(p);
}

void Session::selectTrack(uint32_t trackId, uint32_t weather) {
    Packet p = Packet::fromCmdFull(0x0035);
    p.writeUInt32(trackId);
    p.writeUInt32(weather);
    m_net.send(p);
}

void Session::sendReadyToggle(bool pressed) {
    Packet p = Packet::fromCmdFull(0x0033);
    p.writeUInt32(pressed ? 1u : 0u);
    m_net.send(p);
}

void Session::leaveRace() {
    m_net.send(Packet::fromCmdFull(0x003B));
}

void Session::openGarage() {
    m_net.send(Packet::fromCmdFull(0x000F));
}

void Session::openShop() {
    m_net.send(Packet::fromCmdFull(0x0010));
}

void Session::openLicense() {
    m_net.send(Packet::fromCmdFull(0x0016));
}

void Session::requestRandomInvite() {
    m_net.send(Packet::fromCmdFull(0x012F));
}

void Session::answerInvite(uint32_t replyKey, uint32_t code) {
    Packet p = Packet::fromCmdFull(0x006D);
    p.writeUInt32(replyKey);
    p.writeUInt32(code);
    m_net.send(p);
}

void Session::equipUse(uint32_t category, uint32_t baseKey) {
    Packet p = Packet::fromCmdFull(0x00B9);
    p.writeUInt32(category);
    p.writeUInt32(baseKey);
    p.writeInt32(-1);
    m_net.send(p);
}

void Session::unequip(uint32_t category, uint32_t baseKey) {
    Packet p = Packet::fromCmdFull(0x00BA);
    p.writeUInt32(category);
    p.writeUInt32(baseKey);
    m_net.send(p);
}

void Session::buy(uint32_t category, uint32_t baseKey, int32_t priceKey) {
    Packet p = Packet::fromCmdFull(0x00B7);
    p.writeUInt32(category);
    p.writeUInt32(baseKey);
    p.writeInt32(priceKey);
    p.writeWString(m_profile.nickname);
    m_net.send(p);
}

void Session::createCharacter(uint32_t driverKey, const std::u16string& nickname) {
    Packet p = Packet::fromCmdFull(0x0004);
    p.writeUInt32(driverKey);
    p.writeWString(nickname.size() > 11 ? nickname.substr(0, 11) : nickname);
    m_net.send(p);
    m_createResult = -1;
}

void Session::createRoom(const std::u16string& name, const std::u16string& password, uint32_t maxUsers, uint32_t mode) {
    Packet p = Packet::fromCmdFull(0x002D);
    p.writeWString(name);
    p.writeWString(password.size() > 9 ? password.substr(0, 9) : password);
    p.writeUInt32(maxUsers);
    p.writeUInt32(mode);
    p.writeUInt32(password.empty() ? 0u : 1u);
    p.writeUInt32(0);
    m_net.send(p);
}

void Session::selectTeam(uint32_t team) {
    Packet p = Packet::fromCmdFull(0x0064);
    p.writeUInt32(team);
    m_net.send(p);
}

void Session::quickMatch(int32_t mode) {
    Packet p = Packet::fromCmdFull(0x0064);
    p.writeInt32(mode);
    m_net.send(p);
}

void Session::openMissionMenu() {
    m_net.send(Packet::fromCmdFull(0x008F));
}

void Session::sendStageTail() {
    m_net.send(Packet::fromCmdFull(0x00FA));
    m_net.send(Packet::fromCmdFull(0x008E));
}

void Session::startMission(uint32_t missionId) {
    m_missionRun = MissionRun();
    Packet p = Packet::fromCmdFull(0x0090);
    p.writeUInt32(missionId);
    m_net.send(p);
}

void Session::sendMissionCheckpoint(uint32_t index) {
    Packet p = Packet::fromCmdFull(0x0121);
    p.writeUInt32(index);
    m_net.send(p);
}

// sub 483A40 the key or minus one then the lock byte stands until the 0x0123 ack
void Session::equipPendant(int32_t key) {
    if (m_pendantLock) return;
    Packet p = Packet::fromCmdFull(0x0123);
    p.writeInt32(key);
    m_net.send(p);
    m_pendantLock = true;
    std::printf("[session] pendant equip %d sent\n", key);
}

const PendantDef* Session::pendantDef(uint32_t key) const {
    for (const PendantDef& d : m_pendantDefs) if (d.key == key) return &d;
    return nullptr;
}

bool Session::ownsPendant(uint32_t key) const {
    for (const OwnedPendant& o : m_ownedPendants) if (o.key == key) return true;
    return false;
}

void Session::storePendant(uint32_t instance, uint32_t key, bool replace) {
    if (replace) {
        // sub 451250 finds the row by key then erases the first row with that instance until none is left
        for (;;) {
            size_t found = m_ownedPendants.size();
            for (size_t i = 0; i < m_ownedPendants.size(); ++i) if (m_ownedPendants[i].key == key) { found = i; break; }
            if (found == m_ownedPendants.size()) break;
            const uint32_t inst = m_ownedPendants[found].instance;
            for (size_t i = 0; i < m_ownedPendants.size(); ++i) {
                if (m_ownedPendants[i].instance != inst) continue;
                m_ownedPendants.erase(m_ownedPendants.begin() + static_cast<long long>(i));
                break;
            }
        }
    }
    // sub 451140 appends blind the cap is 64
    if (m_ownedPendants.size() < 64) m_ownedPendants.push_back(OwnedPendant{instance, key});
    std::printf("[session] pendant %u owned instance %u rows %zu\n", key, instance, m_ownedPendants.size());
    if (onPendantChanged) onPendantChanged();
}

// sub 47E800 visible key then three ascii strings the icon base the name key the description key
void Session::parsePendantDef(Packet& pkt) {
    PendantDef d;
    d.visible = u32(pkt) != 0;
    d.key = u32(pkt);
    d.iconBase = pkt.remaining() ? pkt.readString(33) : std::string();
    d.nameKey = pkt.remaining() ? pkt.readString(33) : std::string();
    d.descKey = pkt.remaining() ? pkt.readString(34) : std::string();
    if (m_pendantDefs.size() < 64) m_pendantDefs.push_back(d);
    if (onPendantChanged) onPendantChanged();
}

void Session::sendMissionGoal(uint32_t missionId) {
    Packet p = Packet::fromCmdFull(0x008C);
    p.writeUInt32(missionId);
    m_net.send(p);
}

void Session::addFriend(const std::u16string& name) {
    Packet p = Packet::fromCmdFull(0x006F);
    p.writeWString(name.size() > 12 ? name.substr(0, 12) : name);
    m_net.send(p);
    m_socialCode = -1;
}

void Session::sendIdRequest(uint16_t opcode, uint32_t playerId) {
    Packet p = Packet::fromCmdFull(opcode);
    p.writeUInt32(playerId);
    m_net.send(p);
}

void Session::acceptFriend(uint32_t playerId) { sendIdRequest(0x0070, playerId); }
void Session::rejectFriend(uint32_t playerId) { sendIdRequest(0x0071, playerId); }
void Session::deleteFriend(uint32_t playerId) { sendIdRequest(0x0074, playerId); }

void Session::pollFriendStatus() {
    m_net.send(Packet::fromCmdFull(0x0073));
}

void Session::sendNote(const std::u16string& to, const std::u16string& body) {
    Packet p = Packet::fromCmdFull(0x0081);
    p.writeWString(to.size() > 12 ? to.substr(0, 12) : to);
    p.writeWString(body.size() > 159 ? body.substr(0, 159) : body);
    m_net.send(p);
    m_socialCode = -1;
}

void Session::markNoteRead(uint32_t noteId) { sendIdRequest(0x0084, noteId); }
void Session::deleteNote(uint32_t noteId) { sendIdRequest(0x0085, noteId); }

// the 0x1C ticket row verbatim instance base price mode value active in use
void Session::rollGacha(const OwnedItem& ticket) {
    Packet p = Packet::fromCmdFull(0x00ED);
    p.writeUInt32(ticket.instance);
    p.writeUInt32(ticket.itemKey);
    p.writeUInt32(ticket.priceKey);
    p.writeUInt32(ticket.periodMode);
    p.writeUInt32(ticket.periodValue);
    p.writeUInt32(ticket.active);
    p.writeUInt32(ticket.inUse);
    m_net.send(p);
    m_gacha = GachaResult();
    std::printf("[session] gacha roll 0x00ED base %u instance %u\n", ticket.itemKey, ticket.instance);
}

void Session::sendGift(uint32_t category, const std::u16string& recipient, uint32_t baseKey, int32_t priceKey,
                       const std::u16string& message) {
    Packet p = Packet::fromCmdFull(0x0098);
    p.writeUInt32(category);
    p.writeWString(recipient.size() > 12 ? recipient.substr(0, 12) : recipient);
    p.writeUInt32(baseKey);
    p.writeInt32(priceKey);
    p.writeWString(message.size() > 159 ? message.substr(0, 159) : message);
    m_net.send(p);
    std::printf("[session] gift 0x0098 category %u key %u price %d to %s\n", category, baseKey, priceKey,
                u16ToUtf8(recipient).c_str());
}

void Session::deleteOwned(uint32_t category, uint32_t key) {
    Packet p = Packet::fromCmdFull(0x00B8);
    p.writeUInt32(category);
    p.writeUInt32(key);
    m_net.send(p);
    std::printf("[session] delete 0x00B8 category %u key %u\n", category, key);
}

// 0x010E with no body sub 483440 the stage push follows the ack
void Session::openRoomCraft() {
    m_net.send(Packet::fromCmdFull(0x010E));
}

// 0x010F count then one 0x30 record per changed row sub 483450 sends nothing when nothing moved
void Session::saveRoomCraft(const std::vector<RoomCraftInstance>& dirty) {
    Packet p = Packet::fromCmdFull(0x010F);
    p.writeInt32(static_cast<int32_t>(dirty.size()));
    for (const RoomCraftInstance& r : dirty) {
        p.writeUInt32(r.instance);
        p.writeUInt32(r.objectKey);
        p.writeUInt32(r.category);
        p.writeFloat(r.x);
        p.writeFloat(r.y);
        p.writeFloat(r.z);
        p.writeFloat(r.yaw);
        p.writeUInt32(r.placed);
        p.writeUInt32(r.priceKey);
        p.writeUInt32(r.periodMode);
        p.writeUInt32(r.periodValue);
        p.writeUInt32(r.active);
    }
    m_net.send(p);
    std::printf("[session] room craft save 0x010F %zu rows\n", dirty.size());
}

// 0x010A with no body sub 483680 the stage opens on the empty ack
void Session::openCarCraft() {
    m_net.send(Packet::fromCmdFull(0x010A));
}

void Session::saveCarCraft(uint32_t presetId, const CarConfig& config,
                           const std::vector<CarCraftPartInstance>& parts) {
    Packet p = Packet::fromCmdFull(0x010B);
    p.writeUInt32(presetId);
    p.writeUInt32(config.kartInstance);
    for (uint32_t slot : config.slot) p.writeUInt32(slot);
    p.writeInt32(static_cast<int32_t>(parts.size()));
    for (const CarCraftPartInstance& r : parts) p.writeBytes(r.raw.data(), r.raw.size());
    m_net.send(p);
    std::printf("[session] car craft save 0x010B preset %u %zu parts\n", presetId, parts.size());
}

void Session::renameCarCraftPreset(uint32_t presetId, const std::string& name) {
    Packet p = Packet::fromCmdFull(0x0114);
    p.writeUInt32(presetId);
    p.writeString(name.size() > 9 ? name.substr(0, 9) : name);
    m_net.send(p);
}

// sub 4822D0 the user info item of the friend row menu the wide name alone
void Session::requestUserInfo(const std::u16string& name) {
    Packet p = Packet::fromCmdFull(0x0072);
    p.writeWString(name.size() > 12 ? name.substr(0, 12) : name);
    m_net.send(p);
    m_userInfo = UserInfoCard();
}

// sub 481CD0 the invite item of the friend row menu the wide name alone
void Session::inviteToRoom(const std::u16string& name) {
    Packet p = Packet::fromCmdFull(0x006C);
    p.writeWString(name.size() > 12 ? name.substr(0, 12) : name);
    m_net.send(p);
}

void Session::addBlock(const std::u16string& name) {
    Packet p = Packet::fromCmdFull(0x007A);
    p.writeWString(name.size() > 12 ? name.substr(0, 12) : name);
    m_net.send(p);
    m_socialCode = -1;
}

void Session::removeBlock(uint32_t playerId) { sendIdRequest(0x007B, playerId); }

void Session::openGhostMenu() {
    m_net.send(Packet::fromCmdFull(0x011D));
}

void Session::enterGhost(uint32_t trackId) {
    Packet p = Packet::fromCmdFull(0x00AA);
    p.writeUInt32(trackId);
    m_net.send(p);
    m_ghostSession = GhostSession();
    m_ghostResult = GhostSubmitResult();
}

void Session::ghostStageBegin() { m_net.send(Packet::fromCmdFull(0x00B1)); }
void Session::ghostFinalLap() { m_net.send(Packet::fromCmdFull(0x00B2)); }

// FUN 00425350 states 2010 to 2012 the count the chunks then the submit
void Session::uploadGhost(const std::vector<GhostFrame>& framesIn, uint32_t trackId, uint32_t timeMs, uint32_t carKind) {
    const size_t count = std::min(framesIn.size(), kGhostUploadCap);
    Packet head = Packet::fromCmdFull(0x00AE);
    head.writeUInt32(static_cast<uint32_t>(count));
    m_net.send(head);
    for (size_t at = 0; at < count; at += kGhostUploadChunk) {
        const size_t n = std::min(count - at, kGhostUploadChunk);
        Packet chunk = Packet::fromCmdFull(0x00AF);
        chunk.writeUInt32(static_cast<uint32_t>(n));
        for (size_t i = 0; i < n; ++i) writeFrame(chunk, framesIn[at + i]);
        m_net.send(chunk);
    }
    Packet submit = Packet::fromCmdFull(0x00B0);
    submit.writeUInt32(trackId);
    submit.writeUInt32(timeMs);
    submit.writeUInt32(carKind);
    m_net.send(submit);
    m_ghostResult = GhostSubmitResult();
    std::printf("[session] ghost upload %zu frames then 0x00B0 track %u %u ms kind %u\n", count, trackId, timeMs, carKind);
}

void Session::startLicenceTest() {
    m_net.send(Packet::fromCmdFull(0x0062));
}

void Session::submitLicenceTest(uint32_t key, uint32_t testParam, uint32_t rewardGold) {
    Packet p = Packet::fromCmdFull(0x00A3);
    p.writeUInt32(key);
    p.writeUInt32(testParam);
    p.writeUInt32(rewardGold);
    m_net.send(p);
    m_licenceResult = LicenceTestResult();
}

const GhostTrackBoard* Session::ghostBoardOf(uint32_t trackId) const {
    for (const GhostTrackBoard& t : m_ghostBoard) if (t.trackId == trackId) return &t;
    return nullptr;
}

const MissionDef* Session::missionDef(uint32_t missionId) const {
    for (const MissionDef& d : m_missionDefs) if (d.missionId == missionId) return &d;
    return nullptr;
}

const MissionProgress* Session::missionState(uint32_t missionId) const {
    for (const MissionProgress& p : m_missionProgress) if (p.missionId == missionId) return &p;
    return nullptr;
}

const LicenceProgressRow* Session::licenceState(uint32_t key) const {
    for (const LicenceProgressRow& r : m_licenceProgress) if (r.key == key) return &r;
    return nullptr;
}

void Session::update() {
    if (m_channelReturn == 1 && m_net.now() >= m_channelReturnAt) {
        reconnectToLogin();
        return;
    }
    if (m_channelReturn == 2 && m_net.now() >= m_channelReturnAt) {
        m_channelReturn = 0;
        sendScreenRequest();
        sendLoginRequest();
    }
    if (!m_net.connected()) return;
    const bool alive = m_net.pump(0);
    if (!alive) {
        // a game server that drops the socket on the 0x0019 is the stock path too the login leg follows
        if (m_channelReturn == 1) { reconnectToLogin(); return; }
        if (m_stage != Stage::Closed) closed(m_error.empty() ? "connection closed by the server" : m_error);
        return;
    }
    if (m_net.now() - m_lastBeat >= 1.0) {
        m_lastBeat = m_net.now();
        Packet hb(CMD::C_HEARTBEAT);
        hb.writeUInt32(m_net.takeConsumed());
        m_net.send(hb);
    }
}

void Session::disconnect() {
    if (m_net.connected()) m_net.disconnect();
    if (m_stage != Stage::Closed) closed("closed by the client");
}

void Session::closed(const std::string& reason) {
    m_stage = Stage::Closed;
    m_room = RoomState();
    if (m_error.empty()) m_error = reason;
    if (onDisconnected) onDisconnected(reason);
}

const RoomMember* Session::member(uint32_t playerId) const {
    for (const RoomMember& m : m_room.members) if (m.playerId == playerId) return &m;
    return nullptr;
}

uint32_t Session::myDriverKey() const {
    if (m_profile.characterInstance >= 0) {
        const OwnedCharacter* c = m_catalog.ownedCharacter(static_cast<uint32_t>(m_profile.characterInstance));
        if (c) return c->driverKey;
    }
    if (!m_catalog.ownedCharacters().empty()) return m_catalog.ownedCharacters().front().driverKey;
    return 0;
}

uint32_t Session::myKartKey() const {
    if (m_profile.kartInstance >= 0) {
        const OwnedKart* k = m_catalog.ownedKart(static_cast<uint32_t>(m_profile.kartInstance));
        if (k) return k->kartKey;
    }
    if (!m_catalog.ownedKarts().empty()) return m_catalog.ownedKarts().front().kartKey;
    return 0;
}

void Session::handleFrame(uint16_t op, Packet& pkt) {
    // the room start reuses 0x00BF for its racer rows only the login burst carries catalogue rows
    if (m_stage != Stage::Room && m_stage != Stage::Race && m_catalog.handle(op, pkt)) {
        // a live 0x0107 names a new chassis slot so the garage kart tab is built again
        if (op == 0x0107 && onInventoryChanged) onInventoryChanged(1);
        return;
    }
    switch (op) {
    case CMD::S_ACK:
        // 0x000B is a ping the client answers with the same empty opcode
        m_net.send(Packet(CMD::S_ACK));
        break;
    case CMD::S_LOGIN_RESPONSE:
        parseMessageKey(pkt);
        break;
    case CMD::S_DISPLAY_MESSAGE:
        parseMessageText(pkt);
        break;
    case 0x0003:
        if (onCharacterCreate) onCharacterCreate();
        break;
    case 0x0119:
        parsePendantDef(pkt);
        break;
    case 0x012F: {
        // sub 47ED90 skips the whole frame while option 11 the deny invite row is on
        if (m_denyInvites != 0.f) {
            std::printf("[session] random invite dropped the deny invite option is on\n");
            break;
        }
        const int32_t gate = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
        if (gate <= 0) break;
        RoomInvite inv;
        inv.inviter = pkt.readWString(32);
        inv.roomId = u32(pkt);
        const uint32_t hasPassword = u32(pkt);
        if (hasPassword != 0 && pkt.remaining() >= 2) inv.password = pkt.readWString(16);
        m_invite = inv;
        std::printf("[session] random invite into room %u\n", inv.roomId);
        if (onRoomInvite) onRoomInvite();
        break;
    }
    case 0x011A:
    case 0x011B: {
        // one handler for both a blind append of the instance and key row
        const uint32_t inst = u32(pkt);
        const uint32_t key = u32(pkt);
        storePendant(inst, key, false);
        break;
    }
    case 0x0123:
        // sub 47EAE0 the worn key and the lock of the pendant popup cleared
        m_profile.pendantKey = u32(pkt);
        m_pendantLock = false;
        std::printf("[session] pendant worn %d\n", static_cast<int32_t>(m_profile.pendantKey));
        if (onPendantChanged) onPendantChanged();
        if (onProfile) onProfile();
        break;
    case 0x0004:
        parseCreateResult(pkt);
        break;
    case 0x0016:
        std::printf("[session] licence stage ack 0x0016\n");
        if (onLicenseAck) onLicenseAck();
        break;
    case 0x0062:
        // sub 479580 the empty ack pushes stage 13 the picked test stays client side
        std::printf("[session] licence test ack 0x0062\n");
        if (onLicenceTestAck) onLicenceTestAck();
        break;
    case 0x00A3:
        parseLicenceTestResult(pkt);
        break;
    case 0x00A4:
        // sub 47C770 one byte into the profile grade the top bar gate reads it
        if (pkt.remaining() >= 1) m_profile.band = pkt.readUInt8();
        std::printf("[session] licence grade 0x00A4 %u\n", m_profile.band);
        if (onProfile) onProfile();
        break;
    case 0x00ED:
        parseGachaResult(pkt);
        break;
    case 0x0098:
        parseGiftOk(pkt);
        break;
    case 0x00B8:
        parseDeleteAck(pkt);
        break;
    case 0x011D:
        std::printf("[session] ghost menu ack 0x011D %zu tracks on the board\n", m_ghostBoard.size());
        if (onGhostMenuAck) onGhostMenuAck();
        break;
    case 0x00AB:
        // sub 47C9C0 stores the count and zeroes the fill index
        m_ghostBoardExpected = u32(pkt);
        m_ghostBoard.clear();
        std::printf("[session] ghost board of %u tracks\n", m_ghostBoardExpected);
        break;
    case 0x00AC:
        parseGhostBoardTrack(pkt);
        break;
    case 0x00AE: {
        const uint32_t count = u32(pkt);
        m_ghostIncoming.clear();
        m_ghostIncoming.reserve(count);
        std::printf("[session] 0x00AE %u ghost frames announced\n", count);
        break;
    }
    case 0x00AF: {
        const uint32_t n = u32(pkt);
        for (uint32_t i = 0; i < n && pkt.remaining() >= 28; ++i) m_ghostIncoming.push_back(readFrame(pkt));
        break;
    }
    case 0x00AA:
        parseGhostSession(pkt);
        break;
    case 0x00B0:
        parseGhostResult(pkt);
        break;
    case 0x0063: {
        const uint32_t id = u32(pkt);
        std::printf("[session] 0x0063 room ack id %u\n", id);
        if (onQuickRoom) onQuickRoom(id);
        break;
    }
    case 0x0064: {
        const uint32_t id = u32(pkt);
        const uint32_t team = u32(pkt);
        // the wire says 0 red 1 blue our 0x0021 rows say 0 none 1 red 2 blue the rows win
        for (RoomMember& m : m_room.members) if (m.playerId == id) m.team = team + 1;
        std::printf("[session] team %u for %u\n", team, id);
        if (onRoomChanged) onRoomChanged();
        break;
    }
    case 0x0087:
        parseMissionDef(pkt);
        break;
    case 0x0088:
        parseMissionProgress(pkt);
        break;
    case 0x008A: {
        // sub 450AB0 appends with no dedupe then sorts by the mission id
        MissionProgress row;
        row.missionId = u32(pkt);
        row.cleared = u32(pkt);
        m_missionProgress.push_back(row);
        std::sort(m_missionProgress.begin(), m_missionProgress.end(),
                  [](const MissionProgress& a, const MissionProgress& b) { return a.missionId < b.missionId; });
        break;
    }
    case 0x008C:
        parseMissionComplete(pkt);
        break;
    case 0x00A2: {
        // count then rows of key passed and an unread dword the stock never clears the list either
        const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
        for (int32_t i = 0; i < count && pkt.remaining() >= 12; ++i) {
            LicenceProgressRow row;
            row.key = pkt.readUInt32();
            row.passed = pkt.readUInt32();
            pkt.readUInt32();
            bool known = false;
            for (LicenceProgressRow& r : m_licenceProgress) if (r.key == row.key) { r.passed = row.passed; known = true; }
            if (!known) m_licenceProgress.push_back(row);
        }
        std::printf("[session] licence progress %d rows\n", count);
        break;
    }
    case 0x008F:
        std::printf("[session] mission menu ack %zu defs %zu progress rows\n", m_missionDefs.size(), m_missionProgress.size());
        if (onMissionMenu) onMissionMenu();
        break;
    case 0x0090:
        m_missionRun.active = true;
        m_missionRun.missionId = u32(pkt);
        m_missionRun.goldAfter = u32(pkt);
        m_profile.gold = m_missionRun.goldAfter;
        std::printf("[session] mission %u start ack gold %u\n", m_missionRun.missionId, m_missionRun.goldAfter);
        if (onMissionStart) onMissionStart();
        break;
    case 0x0120: {
        const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
        m_missionRun.path.clear();
        for (int32_t i = 0; i < count && pkt.remaining() >= 12; ++i) {
            MissionRun::Point pt;
            pt.x = pkt.readFloat(); pt.y = pkt.readFloat(); pt.z = pkt.readFloat();
            m_missionRun.path.push_back(pt);
        }
        std::printf("[session] mission path %zu points\n", m_missionRun.path.size());
        break;
    }
    case 0x0122:
        m_missionRun.go = true;
        std::printf("[session] mission go 0x0122\n");
        if (onMissionGo) onMissionGo();
        break;
    case 0x006F:
        parseFriendAddResult(pkt);
        break;
    case 0x0070:
    case 0x0071: {
        u32(pkt);
        const uint32_t id = u32(pkt);
        for (size_t i = 0; i < m_friendRequests.size(); ++i) {
            if (m_friendRequests[i].playerId != id) continue;
            m_friendRequests.erase(m_friendRequests.begin() + static_cast<long long>(i));
            break;
        }
        if (onSocialChanged) onSocialChanged();
        break;
    }
    case 0x0073:
    case 0x0077:
        parseFriendStatus(pkt);
        break;
    case 0x0074: {
        const uint32_t id = u32(pkt);
        m_socialCode = static_cast<int32_t>(u32(pkt));
        for (size_t i = 0; i < m_friends.size(); ++i) {
            if (m_friends[i].playerId != id) continue;
            m_friends.erase(m_friends.begin() + static_cast<long long>(i));
            break;
        }
        if (onSocialChanged) onSocialChanged();
        break;
    }
    case 0x0076:
        parseFriendList(pkt);
        break;
    case 0x0078:
        parseFriendRequests(pkt);
        break;
    case 0x0072:
        parseUserInfoBlob(pkt);
        break;
    case 0x007A:
        parseBlockAddResult(pkt);
        break;
    case 0x007B:
        parseBlockDelResult(pkt);
        break;
    case 0x010A:
        if (onCarCraftAck) onCarCraftAck();
        break;
    case 0x010B:
        parseCarCraftSaveResult(pkt);
        break;
    case 0x010E:
        if (onRoomCraftAck) onRoomCraftAck();
        break;
    case 0x010F:
        parseRoomCraftSaveAck(pkt);
        break;
    case 0x0114:
        parseCarCraftRenameAck(pkt);
        break;
    case 0x0079:
        parseBlockList(pkt);
        break;
    case 0x0081:
        m_socialCode = static_cast<int32_t>(u32(pkt));
        std::printf("[session] messenger result %d\n", m_socialCode);
        if (onSocialChanged) onSocialChanged();
        break;
    case 0x0082:
        parseNote(pkt, false);
        break;
    case 0x0083:
        parseNote(pkt, true);
        break;
    case 0x0084: {
        const uint32_t id = u32(pkt);
        for (NoteRow& n : m_notes) if (n.noteId == id) n.read = true;
        if (onSocialChanged) onSocialChanged();
        break;
    }
    case 0x0085: {
        const uint32_t id = u32(pkt);
        for (size_t i = 0; i < m_notes.size(); ++i) {
            if (m_notes[i].noteId != id) continue;
            m_notes.erase(m_notes.begin() + static_cast<long long>(i));
            break;
        }
        if (onSocialChanged) onSocialChanged();
        break;
    }
    case 0x002A:
        m_whisperPrompt = true;
        break;
    case 0x002B:
        m_whisperPrompt = false;
        break;
    case 0x00B5:
        parseWhisper(pkt);
        break;
    case 0x0126:
        parseSystemLine(pkt);
        break;
    case CMD::C_CLIENT_AUTH:
    case 0x00A7:
        parseProfile(pkt);
        break;
    case CMD::S_CONNECTION_OK:
        parseRefresh(pkt);
        break;
    case CMD::S_CHANNEL_LIST:
        parseChannelList(pkt);
        break;
    case CMD::S_SERVER_REDIRECT:
        parseRedirect(pkt);
        break;
    case 0x0019:
        // the stock answer to the channel return host port and mode where mode 4 keeps the ini port
        if (m_channelReturn == 1) {
            std::printf("[session] 0x0019 answer landed the login socket opens now\n");
            m_channelReturnAt = m_net.now();
        }
        break;
    case CMD::S_SHOW_MENU:
        if (onMenuAck) onMenuAck();
        break;
    case CMD::S_SHOW_LOBBY:
        m_stage = Stage::Lobby;
        m_room = RoomState();
        sendLobbyTail();
        if (onLobbyAck) onLobbyAck();
        break;
    case 0x002D:
        parseRoomRow(pkt);
        break;
    case 0x002E: {
        const uint32_t id = u32(pkt);
        if (m_room.inRoom) {
            // our server sends the leaver id on this opcode inside a room the page knows the lobby meaning only
            removeMember(id);
            break;
        }
        for (size_t i = 0; i < m_rooms.size(); ++i) {
            if (m_rooms[i].roomId != id) continue;
            m_rooms.erase(m_rooms.begin() + static_cast<long long>(i));
            break;
        }
        if (onRoomsChanged) onRoomsChanged();
        break;
    }
    case 0x0023: {
        // one dword into the row record at 0x74 that is the sixth value
        const uint32_t id = u32(pkt);
        const uint32_t value = u32(pkt);
        for (RoomRow& row : m_rooms) if (row.roomId == id) row.value[5] = value;
        if (onRoomsChanged) onRoomsChanged();
        break;
    }
    case 0x0031: {
        const uint32_t id = u32(pkt);
        const uint32_t count = u32(pkt);
        const uint32_t max = u32(pkt);
        for (RoomRow& row : m_rooms) if (row.roomId == id) { row.value[0] = count; row.value[1] = max; }
        if (onRoomsChanged) onRoomsChanged();
        break;
    }
    case 0x0013:
        parseRoomContext(pkt);
        break;
    case 0x006C: {
        // reply key wide inviter an ascii cstr nobody reads a u32 the room id the wide password a flag
        m_invite = RoomInvite();
        m_invite.replyKey = u32(pkt);
        m_invite.inviter = pkt.readWString(13);
        if (pkt.remaining()) pkt.readString(34);
        u32(pkt);
        m_invite.roomId = u32(pkt);
        m_invite.password = pkt.remaining() >= 2 ? pkt.readWString(9) : std::u16string();
        m_invite.flag = pkt.remaining() >= 1 ? pkt.readUInt8() : 0;
        std::printf("[session] room invite from %s to room %u\n", u16ToUtf8(m_invite.inviter).c_str(), m_invite.roomId);
        if (onRoomInvite) onRoomInvite();
        break;
    }
    case 0x0032: {
        const uint32_t slot = u32(pkt);
        const uint32_t enabled = u32(pkt);
        if (slot < 30) m_room.slotEnabled[slot] = enabled != 0;
        break;
    }
    case 0x0021:
        parseRoomMember(pkt);
        break;
    case 0x0022:
        removeMember(u32(pkt));
        break;
    case 0x0030: {
        // the master id lands in the same global the 0x0013 context fills
        const uint32_t id = u32(pkt);
        m_room.masterPlayerId = id;
        for (RoomMember& m : m_room.members) if (m.playerId == id) m.ready = 0;
        if (onRoomChanged) onRoomChanged();
        break;
    }
    case 0x0033: {
        const uint32_t id = u32(pkt);
        const uint32_t state = u32(pkt);
        if (state == 2) {
            m_room.allReady = true;
            for (RoomMember& m : m_room.members) m.ready = 2;
        } else {
            for (RoomMember& m : m_room.members) if (m.playerId == id) m.ready = state;
        }
        if (onRoomChanged) onRoomChanged();
        break;
    }
    case 0x0034:
        m_room.allReady = true;
        for (RoomMember& m : m_room.members) m.ready = 2;
        if (onRoomChanged) onRoomChanged();
        break;
    case 0x0035:
        m_room.trackId = static_cast<int32_t>(u32(pkt));
        m_room.trackWeather = u32(pkt);
        if (onRoomTrack) onRoomTrack();
        break;
    case 0x0014:
        parseSceneChange(pkt);
        break;
    case 0x000F:
        if (onGarageAck) onGarageAck();
        break;
    case 0x0010:
        if (onShopAck) onShopAck();
        break;
    case 0x00B7:
        parseBuyOk(pkt);
        break;
    case 0x00B9:
        parseEquipAck(pkt);
        break;
    case 0x00BA:
        parseUnequipAck(pkt);
        break;
    case 0x00BC:
        parseEquipmentSet(pkt);
        break;
    case 0x00D0:
        m_profile.astro = u32(pkt);
        if (onProfile) onProfile();
        break;
    case 0x00B4:
        parseChat(pkt);
        break;
    default:
        break;
    }
}

// the owned record that follows a category dword its size is the category record size
bool Session::storeOwnedRecord(uint32_t category, Packet& pkt) {
    switch (category) {
    case 0:
        if (pkt.remaining() < 0x2C) return false;
        m_catalog.putOwnedCharacter(Catalog::readOwnedCharacter(pkt));
        return true;
    case 1:
        if (pkt.remaining() < 0x38) return false;
        m_catalog.putOwnedKart(Catalog::readOwnedKart(pkt));
        return true;
    case 2:
        if (pkt.remaining() < 0x1C) return false;
        m_catalog.putOwnedItem(Catalog::readOwnedItem(pkt));
        return true;
    case 3:
        if (pkt.remaining() < 0x1C) return false;
        m_catalog.putOwnedPart(Catalog::readOwnedPart(pkt));
        return true;
    case 4:
        if (pkt.remaining() < 0x1C) return false;
        m_catalog.putOwnedPet(Catalog::readOwnedPet(pkt));
        return true;
    case 5:
        if (pkt.remaining() < 0x30) return false;
        m_catalog.putRoomCraftInstance(Catalog::readRoomCraftInstance(pkt));
        m_catalog.takeRoomCraftMaster();
        return true;
    case 6: {
        // the buy of a car craft part puts the u8 extra flag first then the 0x84 row
        if (pkt.remaining() < 1 + 0x84) return false;
        const uint8_t hasExtra = pkt.readUInt8();
        m_catalog.putCarCraftInstance(Catalog::readCarCraftInstance(pkt));
        if (hasExtra == 1 && pkt.remaining() >= 0x34) pkt.readBytes(0x34);
        return true;
    }
    default:
        return false;
    }
}

// the reward tail of 0x008C and 0x00A3 the car craft row comes before its flag here
bool Session::storeRewardRecord(uint32_t type, Packet& pkt) {
    if (type <= 5) return storeOwnedRecord(type, pkt);
    if (type == 6) {
        if (pkt.remaining() < 0x84 + 1) return false;
        m_catalog.putCarCraftInstance(Catalog::readCarCraftInstance(pkt));
        const uint8_t hasExtra = pkt.readUInt8();
        if (hasExtra == 1 && pkt.remaining() >= 0x34) pkt.readBytes(0x34);
        return true;
    }
    if (type == 7) {
        // the pendant sub 451250 removes that key then sub 451140 appends the row
        if (pkt.remaining() < 8) return false;
        const uint32_t inst = u32(pkt);
        const uint32_t key = u32(pkt);
        storePendant(inst, key, true);
        return true;
    }
    return false;
}

// 0x00B7 category then gold then astro then the record of that category
void Session::parseBuyOk(Packet& pkt) {
    const uint32_t category = u32(pkt);
    m_profile.gold = u32(pkt);
    m_profile.astro = u32(pkt);
    storeOwnedRecord(category, pkt);
    std::printf("[session] buy ok category %u gold %u astro %u\n", category, m_profile.gold, m_profile.astro);
    if (onProfile) onProfile();
    if (onInventoryChanged) onInventoryChanged(category);
    if (onBuyOk) onBuyOk(category);
}

// 0x00B9 category then the record the item case can carry the kart period tail
void Session::parseEquipAck(Packet& pkt) {
    const uint32_t category = u32(pkt);
    // sub 484770 case 4 reads the sub op a two carries the pet taken off first
    if (category == 4) {
        const uint32_t subOp = u32(pkt);
        if (subOp == 2) {
            OwnedPet previous = Catalog::readOwnedPet(pkt);
            previous.equipped = 0;
            m_catalog.putOwnedPet(previous);
        }
        OwnedPet fresh = Catalog::readOwnedPet(pkt);
        fresh.equipped = 1;
        m_catalog.putOwnedPet(fresh);
        std::printf("[session] pet equipped key %u instance %u sub op %u\n", fresh.petKey, fresh.instance, subOp);
        if (onInventoryChanged) onInventoryChanged(category);
        return;
    }
    const size_t before = pkt.remaining();
    uint32_t itemKey = 0;
    if (category == 2 && before >= 8) {
        Packet peek = pkt;
        peek.readUInt32();
        itemKey = peek.readUInt32();
    }
    storeOwnedRecord(category, pkt);
    // the tail lands only when the 0x00C1 use type is four to seven and a kart is selected
    const ItemRow* def = category == 2 ? m_catalog.item(itemKey) : nullptr;
    const bool repair = def && def->useType >= 4 && def->useType <= 7;
    if (repair && pkt.remaining() >= 16 && m_profile.kartInstance >= 0) {
        const OwnedKart* current = m_catalog.ownedKart(static_cast<uint32_t>(m_profile.kartInstance));
        if (current) {
            OwnedKart patched = *current;
            patched.priceKey = u32(pkt);
            patched.expiryKind = u32(pkt);
            patched.durability = static_cast<int32_t>(u32(pkt));
            patched.active = u32(pkt);
            m_catalog.putOwnedKart(patched);
            std::printf("[session] repair scroll %u kart durability %d\n", itemKey, patched.durability);
        }
    }
    if (onInventoryChanged) onInventoryChanged(category);
}

// 0x00BA category then a record for the categories two three and four only
void Session::parseUnequipAck(Packet& pkt) {
    const uint32_t category = u32(pkt);
    if (category == 2 || category == 3) storeOwnedRecord(category, pkt);
    if (category == 4 && pkt.remaining() >= 0x1C) {
        OwnedPet row = Catalog::readOwnedPet(pkt);
        row.equipped = 0;
        m_catalog.putOwnedPet(row);
    }
    if (onInventoryChanged) onInventoryChanged(category);
}

// 0x010F the echo of the saved rows only the id the position the yaw and the placed flag are used
void Session::parseRoomCraftSaveAck(Packet& pkt) {
    const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    for (int32_t i = 0; i < count && pkt.remaining() >= 0x30; ++i) {
        const RoomCraftInstance row = Catalog::readRoomCraftInstance(pkt);
        m_catalog.putRoomCraftInstance(row);
    }
    m_catalog.takeRoomCraftMaster();
    std::printf("[session] room craft save ack %d rows\n", count);
    if (onRoomCraftSaved) onRoomCraftSaved();
}

// 0x010B preset id slot state the selected kart then the config and the db truth part list
void Session::parseCarCraftSaveResult(Packet& pkt) {
    CarCraftPreset preset;
    preset.presetId = u32(pkt);
    preset.slotState = u32(pkt);
    const uint32_t selectedKart = u32(pkt);
    preset.kartInstance = u32(pkt);
    for (uint32_t& slot : preset.slot) slot = u32(pkt);
    const CarCraftPreset* known = nullptr;
    for (const CarCraftPreset& r : m_catalog.carCraftPresets()) if (r.presetId == preset.presetId) known = &r;
    if (known) preset.name = known->name;
    m_catalog.putCarCraftPreset(preset);
    // the third dword sets the selected kart with no lookup as the stock reader does
    m_profile.kartInstance = static_cast<int32_t>(selectedKart);
    const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    std::vector<CarCraftPartInstance> fresh;
    for (int32_t i = 0; i < count && pkt.remaining() >= 0x84; ++i)
        fresh.push_back(Catalog::readCarCraftInstance(pkt));
    if (!fresh.empty()) {
        m_catalog.carCraftInstances() = fresh;
    }
    std::printf("[session] car craft save result preset %u %d parts\n", preset.presetId, count);
    if (onProfile) onProfile();
    if (onCarCraftSaved) onCarCraftSaved();
}

// 0x0114 the preset id then the stored name
void Session::parseCarCraftRenameAck(Packet& pkt) {
    const uint32_t presetId = u32(pkt);
    const std::string name = pkt.remaining() ? pkt.readString(32) : std::string();
    for (const CarCraftPreset& r : m_catalog.carCraftPresets()) {
        if (r.presetId != presetId) continue;
        CarCraftPreset copy = r;
        copy.name = name.size() > 11 ? name.substr(0, 11) : name;
        m_catalog.putCarCraftPreset(copy);
        break;
    }
    if (onCarCraftSaved) onCarCraftSaved();
}

// same 0x68 layout as the 0x0028 blob
void Session::parseUserInfoBlob(Packet& pkt) {
    if (pkt.remaining() < 0x68) { std::printf("[session] user info blob short %zu\n", pkt.remaining()); return; }
    const std::vector<uint8_t>& p = pkt.payload();
    const size_t at = p.size() - pkt.remaining();
    UserInfoCard card;
    card.valid = true;
    card.playerId = rd32(p, at);
    card.name = rdWide(p, at + 0x04, 13);
    card.level = static_cast<uint32_t>(p[at + 0x1E] | (p[at + 0x1F] << 8));
    card.driverKey = rd32(p, at + 0x28);
    card.exp = rd32(p, at + 0x30);
    card.expFloor = rd32(p, at + 0x3C);
    card.expNext = rd32(p, at + 0x40);
    card.pendant = static_cast<int32_t>(rd32(p, at + 0x64));
    pkt.readBytes(0x68);
    m_userInfo = card;
    std::printf("[session] user info id %u level %u driver %u\n", card.playerId, card.level, card.driverKey);
    if (onSocialChanged) onSocialChanged();
}

// 0x007A zero means the block landed and a 0x20 record follows the polarity is inverted from 0x006F
void Session::parseBlockAddResult(Packet& pkt) {
    const uint32_t result = u32(pkt);
    m_socialCode = static_cast<int32_t>(result);
    if (result == 0 && pkt.remaining() >= 0x20) {
        const std::vector<uint8_t>& p = pkt.payload();
        const size_t at = p.size() - pkt.remaining();
        BlockRow row;
        row.playerId = rd32(p, at);
        row.name = rdWide(p, at + 0x04, 14);
        pkt.readBytes(0x20);
        bool known = false;
        for (BlockRow& b : m_blocks) if (b.playerId == row.playerId) { b = row; known = true; }
        if (!known) m_blocks.push_back(row);
    }
    std::printf("[session] block add result %u blocks %zu\n", result, m_blocks.size());
    if (onSocialChanged) onSocialChanged();
}

// 0x007B the player id then a result that is read and dropped the removal is unconditional
void Session::parseBlockDelResult(Packet& pkt) {
    const uint32_t playerId = u32(pkt);
    u32(pkt);
    for (size_t i = m_blocks.size(); i > 0; --i)
        if (m_blocks[i - 1].playerId == playerId) m_blocks.erase(m_blocks.begin() + static_cast<long long>(i - 1));
    std::printf("[session] block removed %u blocks %zu\n", playerId, m_blocks.size());
    if (onSocialChanged) onSocialChanged();
}

// 0x00BC the two selection instance ids then the character and kart records
void Session::parseEquipmentSet(Packet& pkt) {
    const uint32_t characterInstance = u32(pkt);
    const uint32_t kartInstance = u32(pkt);
    const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    m_profile.characterInstance = static_cast<int32_t>(characterInstance);
    m_profile.kartInstance = static_cast<int32_t>(kartInstance);
    if (count <= 0) {
        storeOwnedRecord(0, pkt);
        storeOwnedRecord(1, pkt);
    }
    std::printf("[session] equipment set character %u kart %u\n", characterInstance, kartInstance);
    if (onProfile) onProfile();
    if (onInventoryChanged) onInventoryChanged(0);
}

void Session::parseMessageKey(Packet& pkt) {
    ServerMessage m;
    m.wide = false;
    m.key = pkt.readString(255);
    m.boxType = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
    // four keys force the disconnect whatever the type says see the 0x0001 page
    const bool forced = m.key.find("MSG_DB_ACCESS_FAIL") != std::string::npos ||
                        m.key.find("MSG_REINPUT_IDPASS") != std::string::npos ||
                        m.key.find("MSG_INVALID_ID") != std::string::npos;
    m.closesConnection = m.boxType == 2 || forced;
    m_error = m.key;
    if (onMessage) onMessage(m);
    if (m.closesConnection) m_net.disconnect();
}

void Session::parseMessageText(Packet& pkt) {
    ServerMessage m;
    m.wide = true;
    m.text = pkt.readWString(159);
    m.boxType = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
    m.closesConnection = m.boxType == 2;
    // our login server sends an empty type 1 box right after the profile nothing to show
    if (m.text.empty() && !m.closesConnection) return;
    m_error = u16ToUtf8(m.text);
    if (onMessage) onMessage(m);
    if (m.closesConnection) m_net.disconnect();
}

void Session::parseProfile(Packet& pkt) {
    const std::vector<uint8_t>& p = pkt.payload();
    if (p.size() < 4 + kBlobSize) {
        std::printf("[session] profile frame is %zu bytes expected %zu\n", p.size(), 4 + kBlobSize);
        return;
    }
    const size_t b = 4;
    m_profile.valid = true;
    m_profile.playerId = rd32(p, 0);
    m_profile.reauthSessionId = rd32(p, b + 0x000);
    m_profile.reauthToken = rdWide(p, b + 0x004, 64);
    m_profile.nickname = rdWide(p, b + 0x486, 13);
    m_profile.band = rd8(p, b + 0x4A0);
    m_profile.level = rd8(p, b + 0x4A1);
    m_profile.exp = rd32(p, b + 0x4A4);
    m_profile.astro = rd32(p, b + 0x4A8);
    m_profile.gold = rd32(p, b + 0x4AC);
    m_profile.characterInstance = static_cast<int32_t>(rd32(p, b + 0x4B0));
    m_profile.kartInstance = static_cast<int32_t>(rd32(p, b + 0x4B4));
    m_profile.rolePrivilege = rd8(p, b + 0x4B9);
    m_profile.expFloor = rd32(p, b + 0x4BC);
    m_profile.expNext = rd32(p, b + 0x4C0);
    m_profile.pendantKey = rd32(p, b + 0x4C4);
    if (onProfile) onProfile();
}

void Session::parseRefresh(Packet& pkt) {
    const std::vector<uint8_t>& p = pkt.payload();
    if (p.size() < 38) return;
    m_profile.band = rd8(p, 0x00);
    m_profile.level = rd8(p, 0x01);
    m_profile.exp = rd32(p, 0x02);
    m_profile.astro = rd32(p, 0x06);
    m_profile.gold = rd32(p, 0x0A);
    m_profile.characterInstance = static_cast<int32_t>(rd32(p, 0x0E));
    m_profile.kartInstance = static_cast<int32_t>(rd32(p, 0x12));
    m_profile.rolePrivilege = rd8(p, 0x17);
    m_profile.expFloor = rd32(p, 0x1A);
    m_profile.expNext = rd32(p, 0x1E);
    m_profile.pendantKey = rd32(p, 0x22);
    if (onProfile) onProfile();
}

void Session::parseChannelList(Packet& pkt) {
    m_channels.clear();
    const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    for (int32_t i = 0; i < count && pkt.remaining() >= 4; ++i) {
        ChannelRow row;
        row.id = pkt.readUInt32();
        row.name = pkt.readWString(255);
        row.population = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
        row.capacity = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
        row.tier = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
        m_channels.push_back(row);
    }
    m_notice = pkt.readWString(1024);
    m_stage = Stage::ChannelPick;
    if (onChannelList) onChannelList();
}

void Session::parseRedirect(Packet& pkt) {
    if (pkt.remaining() >= 4) pkt.readUInt32();
    std::string ip = pkt.readString(255);
    const uint32_t port = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
    m_gamePort = static_cast<uint16_t>(port & 0xFFFF);
    m_gameHost = ip.empty() ? m_loginHost : ip;
    std::printf("[session] handoff 0x0054 to %s:%u\n", m_gameHost.c_str(), m_gamePort);
    m_net.disconnect();
    bool ok = m_net.connect(m_gameHost, m_gamePort);
    if (!ok && m_gameHost != m_loginHost) {
        // the packet ip may be one the docker host cannot reach try the login host
        std::printf("[session] %s unreachable trying %s\n", m_gameHost.c_str(), m_loginHost.c_str());
        m_gameHost = m_loginHost;
        ok = m_net.connect(m_gameHost, m_gamePort);
    }
    if (!ok) {
        m_error = "game server connect failed";
        closed(m_error);
        return;
    }
    m_stage = Stage::GameServer;
    m_lastBeat = m_net.now();
    sendScreenRequest();
    if (!m_profile.reauthToken.empty()) sendReauth();
    else sendLoginRequest();
}

void Session::parseRoomRow(Packet& pkt) {
    RoomRow row;
    row.roomId = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
    row.name = pkt.readWString(41);
    for (uint32_t& v : row.value) v = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
    bool replaced = false;
    for (RoomRow& r : m_rooms) {
        if (r.roomId != row.roomId) continue;
        r = row;
        replaced = true;
        break;
    }
    if (!replaced) m_rooms.push_back(row);
    if (onRoomsChanged) onRoomsChanged();
}

void Session::parseChat(Packet& pkt) {
    ChatLine line;
    line.playerId = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
    line.sender = pkt.readWString(13);
    line.text = pkt.readWString(8191);
    line.type = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
    // type 1 and anything above 4 are dropped by the client see the 0x00B4 page
    if (line.type == 1 || line.type > 4) return;
    m_chat.push_back(line);
    if (m_chat.size() > 200) m_chat.erase(m_chat.begin());
    if (onChat) onChat(line);
}

// 0x0013 the room context every 0x0021 needs it first a fresh one clears the seats
void Session::parseRoomContext(Packet& pkt) {
    RoomState r;
    r.inRoom = true;
    r.roomId = u32(pkt);
    r.name = pkt.readWString(41);
    r.maxPlayers = u32(pkt);
    r.gameMode = u32(pkt);
    r.hasPassword = u32(pkt);
    u32(pkt);
    u32(pkt);
    u32(pkt);
    r.masterPlayerId = u32(pkt);
    r.weather = u32(pkt);
    // the decor tail a count then 0x30 byte records the valid flag must be one
    const uint32_t decorCount = pkt.remaining() >= 4 ? u32(pkt) : 0;
    for (uint32_t i = 0; i < decorCount && pkt.remaining() >= 0x30; ++i) {
        RoomDecor d;
        d.instanceId = u32(pkt);
        d.catalogKey = u32(pkt);
        d.category = u32(pkt);
        d.x = pkt.readFloat();
        d.y = pkt.readFloat();
        d.z = pkt.readFloat();
        d.yawDeg = pkt.readFloat();
        d.placed = u32(pkt);
        u32(pkt);
        u32(pkt);
        u32(pkt);
        d.valid = u32(pkt);
        if (d.valid == 1) r.decor.push_back(d);
    }
    r.trackId = m_room.trackId;
    r.trackWeather = m_room.trackWeather;
    m_room = r;
    m_stage = Stage::Room;
    std::printf("[session] room %u %s max %u mode %u master %u\n", r.roomId, u16ToUtf8(r.name).c_str(),
                r.maxPlayers, r.gameMode, r.masterPlayerId);
    if (onRoomEnter) onRoomEnter();
}

// 0x0021 read order 4 4 4 wstr 1 1 1 4 0x2C 0x38 4 4 0x3C
void Session::parseRoomMember(Packet& pkt) {
    RoomMember m;
    m.slot = u32(pkt);
    m.team = u32(pkt);
    m.playerId = u32(pkt);
    m.name = pkt.readWString(64);
    m.level = pkt.remaining() >= 1 ? pkt.readUInt8() : 0;
    if (pkt.remaining() >= 1) pkt.readUInt8();
    if (pkt.remaining() >= 1) pkt.readUInt8();
    m.pendantKey = u32(pkt);
    const std::vector<uint8_t>& p = pkt.payload();
    const size_t at = p.size() - pkt.remaining();
    m.driverKey = rd32(p, at + 0x04);
    m.kartKey = rd32(p, at + 0x2C + 0x04);
    m.ready = rd32(p, at + 0x2C + 0x38);
    if (!m_room.inRoom) return;
    bool replaced = false;
    for (RoomMember& r : m_room.members) {
        if (r.playerId != m.playerId && r.slot != m.slot) continue;
        r = m;
        replaced = true;
        break;
    }
    if (!replaced) m_room.members.push_back(m);
    std::printf("[session] member slot %u id %u %s driver %u kart %u ready %u\n", m.slot, m.playerId,
                u16ToUtf8(m.name).c_str(), m.driverKey, m.kartKey, m.ready);
    if (onRoomChanged) onRoomChanged();
}

void Session::removeMember(uint32_t playerId) {
    for (size_t i = 0; i < m_room.members.size(); ++i) {
        if (m_room.members[i].playerId != playerId) continue;
        m_room.members.erase(m_room.members.begin() + static_cast<long long>(i));
        break;
    }
    if (onRoomChanged) onRoomChanged();
}

// 0x0014 the four dwords after the kind exist only for the kinds 3 and 8
void Session::parseSceneChange(Packet& pkt) {
    RaceLaunch l;
    l.sceneKind = u32(pkt);
    if (l.sceneKind == 3 || l.sceneKind == 8) {
        u32(pkt);
        l.trackId = u32(pkt);
        l.gameMode = u32(pkt);
        l.playerCount = u32(pkt);
        l.flag = u32(pkt);
    }
    m_launch = l;
    std::printf("[session] scene change kind %u track %u mode %u players %u\n", l.sceneKind, l.trackId,
                l.gameMode, l.playerCount);
    if (l.sceneKind == 3 || l.sceneKind == 8) {
        m_stage = Stage::Race;
        if (onRaceLaunch) onRaceLaunch();
    }
}


// 0x0004 the result then on zero the nickname and the character and kart records
void Session::parseCreateResult(Packet& pkt) {
    m_createResult = pkt.remaining() >= 4 ? pkt.readInt32() : -1;
    if (m_createResult == 0) {
        m_profile.nickname = pkt.readWString(12);
        // sub 479230 puts the two instances in the blob 0x4B0 and 0x4B4 so the lobby preview has them
        const std::vector<uint8_t>& p = pkt.payload();
        const size_t at = p.size() - pkt.remaining();
        if (pkt.remaining() >= 0x2C + 0x38) {
            m_profile.characterInstance = static_cast<int32_t>(rd32(p, at));
            m_profile.kartInstance = static_cast<int32_t>(rd32(p, at + 0x2C));
        }
        storeOwnedRecord(0, pkt);
        storeOwnedRecord(1, pkt);
        std::printf("[session] character created %s\n", u16ToUtf8(m_profile.nickname).c_str());
    } else {
        std::printf("[session] character creation refused result %d\n", m_createResult);
    }
    if (onCharacterCreated) onCharacterCreated();
}

// 0x0087 one fixed 188 byte record the strings are char 33 slots
void Session::parseMissionDef(Packet& pkt) {
    const std::vector<uint8_t>& p = pkt.payload();
    if (p.size() < 0xBC) {
        std::printf("[session] mission def is %zu bytes expected 188\n", p.size());
        return;
    }
    auto slot = [&](size_t at) {
        std::string s;
        for (size_t i = 0; i < 33 && at + i < p.size() && p[at + i] != 0; ++i) s.push_back(static_cast<char>(p[at + i]));
        return s;
    };
    MissionDef d;
    d.missionId = rd32(p, 0x04);
    d.kind = rd32(p, 0x08);
    d.goalCount = static_cast<int32_t>(rd32(p, 0x10));
    d.timeLimitMs = static_cast<int32_t>(rd32(p, 0x14));
    d.rewardExtra = rd32(p, 0x18);
    d.rewardMileage = rd32(p, 0x1C);
    d.rewardExp = rd32(p, 0x20);
    d.rewardItemType = rd32(p, 0x28);
    d.rewardItemKey = rd32(p, 0x2C);
    d.worldName = slot(0x38);
    d.titleKey = slot(0x59);
    d.subKey = slot(0x7A);
    d.descKey = slot(0x9B);
    bool replaced = false;
    for (MissionDef& old : m_missionDefs) if (old.missionId == d.missionId) { old = d; replaced = true; }
    if (!replaced) m_missionDefs.push_back(d);
    std::printf("[session] mission def %u kind %u goal %d limit %d ms world %s title %s\n", d.missionId, d.kind,
                d.goalCount, d.timeLimitMs, d.worldName.c_str(), d.titleKey.c_str());
}

// 0x0088 a full replace sorted by mission id
void Session::parseMissionProgress(Packet& pkt) {
    m_missionProgress.clear();
    const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    for (int32_t i = 0; i < count && pkt.remaining() >= 8; ++i) {
        MissionProgress row;
        row.missionId = pkt.readUInt32();
        row.cleared = pkt.readUInt32();
        m_missionProgress.push_back(row);
    }
    std::sort(m_missionProgress.begin(), m_missionProgress.end(),
              [](const MissionProgress& a, const MissionProgress& b) { return a.missionId < b.missionId; });
}

// 0x008C has reward then a u64 mission id then gold and exp the reward blob size comes from the def
void Session::parseMissionComplete(Packet& pkt) {
    const uint32_t hasReward = u32(pkt);
    const uint32_t missionId = u32(pkt);
    u32(pkt);
    // sub 47B9E0 any flag but 0 and 1 reads twelve bytes writes nothing and still moves the run on
    m_missionRun.complete = true;
    if (hasReward > 1) {
        m_missionRun.completeGold = m_profile.gold;
        m_missionRun.completeExp = m_profile.exp;
        std::printf("[session] mission %u complete refused flag %u\n", missionId, hasReward);
        if (onMissionComplete) onMissionComplete();
        return;
    }
    m_profile.gold = u32(pkt);
    m_profile.exp = u32(pkt);
    m_missionRun.completeGold = m_profile.gold;
    m_missionRun.completeExp = m_profile.exp;
    for (MissionProgress& row : m_missionProgress) if (row.missionId == missionId) row.cleared = 1;
    if (hasReward == 1) {
        const MissionDef* def = missionDef(missionId);
        const uint32_t type = def ? def->rewardItemType : 99;
        // the size comes from the def type not the wire
        if (!storeRewardRecord(type, pkt)) std::printf("[session] mission reward type %u was not read\n", type);
    }
    std::printf("[session] mission %u complete gold %u exp %u reward %u\n", missionId, m_profile.gold, m_profile.exp, hasReward);
    if (onProfile) onProfile();
    if (onMissionComplete) onMissionComplete();
}

// 0x0076 full replace of the 0x2C records
void Session::parseFriendList(Packet& pkt) {
    m_friends.clear();
    const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    const std::vector<uint8_t>& p = pkt.payload();
    for (int32_t i = 0; i < count && pkt.remaining() >= 0x2C; ++i) {
        const size_t at = p.size() - pkt.remaining();
        FriendRow row;
        row.playerId = rd32(p, at);
        row.name = rdWide(p, at + 0x04, 14);
        row.level = rd32(p, at + 0x20);
        row.statusA = static_cast<int32_t>(rd32(p, at + 0x24));
        row.statusB = static_cast<int32_t>(rd32(p, at + 0x28));
        m_friends.push_back(row);
        pkt.readBytes(0x2C);
    }
    std::printf("[session] friend list %zu rows\n", m_friends.size());
    if (onSocialChanged) onSocialChanged();
}

// 0x0078 full replace of the 0x24 records
void Session::parseFriendRequests(Packet& pkt) {
    m_friendRequests.clear();
    const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    const std::vector<uint8_t>& p = pkt.payload();
    for (int32_t i = 0; i < count && pkt.remaining() >= 0x24; ++i) {
        const size_t at = p.size() - pkt.remaining();
        FriendRequestRow row;
        row.playerId = rd32(p, at);
        row.name = rdWide(p, at + 0x04, 14);
        row.level = rd32(p, at + 0x20);
        m_friendRequests.push_back(row);
        pkt.readBytes(0x24);
    }
    std::printf("[session] friend requests %zu rows\n", m_friendRequests.size());
    if (onSocialChanged) onSocialChanged();
}

// 0x0079 full replace of the 0x20 records
void Session::parseBlockList(Packet& pkt) {
    m_blocks.clear();
    const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    const std::vector<uint8_t>& p = pkt.payload();
    for (int32_t i = 0; i < count && pkt.remaining() >= 0x20; ++i) {
        const size_t at = p.size() - pkt.remaining();
        BlockRow row;
        row.playerId = rd32(p, at);
        row.name = rdWide(p, at + 0x04, 14);
        m_blocks.push_back(row);
        pkt.readBytes(0x20);
    }
    if (onSocialChanged) onSocialChanged();
}

// 0x0073 resets every status to minus two then sets the listed ones 0x0077 touches status b only
void Session::parseFriendStatus(Packet& pkt) {
    const bool full = pkt.opcode() == 0x0073;
    if (full) for (FriendRow& f : m_friends) { f.statusA = -2; f.statusB = -2; }
    const int32_t count = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t id = u32(pkt);
        const int32_t a = full ? (pkt.remaining() >= 4 ? pkt.readInt32() : -2) : 0;
        const int32_t b = pkt.remaining() >= 4 ? pkt.readInt32() : -2;
        for (FriendRow& f : m_friends) {
            if (f.playerId != id) continue;
            if (full) f.statusA = a;
            f.statusB = b;
        }
    }
    if (onSocialChanged) onSocialChanged();
}

// 0x006F the code then a friend record on 1 or a request record on 12
void Session::parseFriendAddResult(Packet& pkt) {
    m_socialCode = static_cast<int32_t>(u32(pkt));
    const std::vector<uint8_t>& p = pkt.payload();
    const size_t at = p.size() - pkt.remaining();
    if (m_socialCode == 1 && pkt.remaining() >= 0x2C) {
        FriendRow row;
        row.playerId = rd32(p, at);
        row.name = rdWide(p, at + 0x04, 14);
        row.level = rd32(p, at + 0x20);
        row.statusA = static_cast<int32_t>(rd32(p, at + 0x24));
        row.statusB = static_cast<int32_t>(rd32(p, at + 0x28));
        bool replaced = false;
        for (FriendRow& f : m_friends) if (f.playerId == row.playerId) { f = row; replaced = true; }
        if (!replaced) m_friends.push_back(row);
    } else if (m_socialCode == 12 && pkt.remaining() >= 0x24) {
        FriendRequestRow row;
        row.playerId = rd32(p, at);
        row.name = rdWide(p, at + 0x04, 14);
        row.level = rd32(p, at + 0x20);
        bool replaced = false;
        for (FriendRequestRow& f : m_friendRequests) if (f.playerId == row.playerId) { f = row; replaced = true; }
        if (!replaced) m_friendRequests.push_back(row);
    }
    std::printf("[session] friend add result %d\n", m_socialCode);
    if (onSocialChanged) onSocialChanged();
}

// 0x0082 and 0x0083 one 396 byte record 0x0083 sorts on the two keys
void Session::parseNote(Packet& pkt, bool sorted) {
    const std::vector<uint8_t>& p = pkt.payload();
    if (p.size() < 0x18C) {
        std::printf("[session] note record is %zu bytes expected 396\n", p.size());
        return;
    }
    NoteRow n;
    n.noteId = rd32(p, 0x00);
    n.sender = rdWide(p, 0x04, 13);
    n.sortKey1 = rdWide(p, 0x1E, 11);
    n.sortKey2 = rdWide(p, 0x34, 9);
    n.read = rd8(p, 0x46) != 0;
    n.body = rdWide(p, 0x48, (0x18C - 0x48) / 2);
    if (m_notes.size() >= 30) m_notes.erase(m_notes.begin());
    m_notes.push_back(n);
    if (sorted) {
        std::sort(m_notes.begin(), m_notes.end(), [](const NoteRow& a, const NoteRow& b) {
            if (a.sortKey1 != b.sortKey1) return a.sortKey1 < b.sortKey1;
            return a.sortKey2 < b.sortKey2;
        });
    }
    std::printf("[session] note %u from %s\n", n.noteId, u16ToUtf8(n.sender).c_str());
    if (onSocialChanged) onSocialChanged();
}

// 0x00B5 two 0x4C8 user blobs then the text the partner is the one that is not us
void Session::parseWhisper(Packet& pkt) {
    const std::vector<uint8_t>& p = pkt.payload();
    if (p.size() < 2 * kBlobSize) {
        std::printf("[session] whisper is %zu bytes expected at least %zu\n", p.size(), 2 * kBlobSize);
        return;
    }
    const uint32_t senderId = rd32(p, 0);
    const std::u16string senderName = rdWide(p, 0x486, 13);
    const uint32_t receiverId = rd32(p, kBlobSize);
    const std::u16string receiverName = rdWide(p, kBlobSize + 0x486, 13);
    pkt.readBytes(2 * kBlobSize);
    ChatLine line;
    line.type = 6;
    line.text = pkt.readWString(256);
    line.outgoing = senderId == m_profile.playerId && receiverId != m_profile.playerId;
    line.playerId = line.outgoing ? receiverId : senderId;
    line.sender = line.outgoing ? receiverName : senderName;
    m_chat.push_back(line);
    if (m_chat.size() > 200) m_chat.erase(m_chat.begin());
    std::printf("[session] whisper %s %s: %s\n", line.outgoing ? "to" : "from", u16ToUtf8(line.sender).c_str(),
                u16ToUtf8(line.text).c_str());
    if (onChat) onChat(line);
}

// 0x0126 an unused dword the wide name the ascii key then the type which must be 5
void Session::parseSystemLine(Packet& pkt) {
    u32(pkt);
    ChatLine line;
    line.sender = pkt.readWString(13);
    const std::string key = pkt.readString(259);
    line.type = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
    if (line.type != 5) return;
    line.text = asciiToU16(key);
    m_chat.push_back(line);
    if (m_chat.size() > 200) m_chat.erase(m_chat.begin());
    std::printf("[session] system line %s %s\n", u16ToUtf8(line.sender).c_str(), key.c_str());
    if (onChat) onChat(line);
}

// sub 47D5E0 five dwords the echoed ticket row then the prize record of the category
void Session::parseGachaResult(Packet& pkt) {
    GachaResult r;
    r.valid = true;
    r.rareFlag = u32(pkt);
    r.category = u32(pkt);
    r.baseKey = u32(pkt);
    r.periodMode = u32(pkt);
    r.periodValue = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    if (pkt.remaining() >= 0x1C) {
        r.ticket = Catalog::readOwnedItem(pkt);
        // the client never decrements the row the echo carries the count after the roll
        m_catalog.putOwnedItem(r.ticket);
    }
    // the typed tail lands in the owned lists the same way a buy ack does
    if (r.category <= 3) storeOwnedRecord(r.category, pkt);
    m_gacha = r;
    std::printf("[session] gacha result category %u key %u rare %u ticket left %u\n", r.category, r.baseKey, r.rareFlag,
                r.ticket.periodValue);
    if (onInventoryChanged) onInventoryChanged(2);
    if (r.category <= 3 && r.category != 2 && onInventoryChanged) onInventoryChanged(r.category);
    if (onGachaResult) onGachaResult();
}

// sub 47BEF0 astro then gold then a 0xD4 blob nobody reads the order is the reverse of 0x00B7
void Session::parseGiftOk(Packet& pkt) {
    m_profile.astro = u32(pkt);
    m_profile.gold = u32(pkt);
    std::printf("[session] gift ok astro %u gold %u\n", m_profile.astro, m_profile.gold);
    if (onProfile) onProfile();
    if (onGiftOk) onGiftOk();
}

// sub 484690 category then the key the categories 0 to 3 erase by base key the others nothing
void Session::parseDeleteAck(Packet& pkt) {
    const uint32_t category = u32(pkt);
    const uint32_t key = u32(pkt);
    const bool removed = m_catalog.removeOwned(category, key);
    std::printf("[session] delete ack category %u key %u %s\n", category, key, removed ? "removed" : "no row");
    if (onInventoryChanged) onInventoryChanged(category);
}

// sub 47C9F0 a row past the announced count is dropped whole the track id is read then ignored
void Session::parseGhostBoardTrack(Packet& pkt) {
    if (m_ghostBoard.size() >= m_ghostBoardExpected) return;
    GhostTrackBoard t;
    t.trackId = u32(pkt);
    t.best = readRecord(pkt);
    const uint32_t n = u32(pkt);
    for (uint32_t i = 0; i < n && pkt.remaining() >= 0xB0; ++i) t.entries.push_back(readRecord(pkt));
    m_ghostBoard.push_back(std::move(t));
}

// sub 47CBA0 the track the wide name the car kind the record time then the two blobs
void Session::parseGhostSession(Packet& pkt) {
    GhostSession info;
    info.valid = true;
    info.frames = std::move(m_ghostIncoming);
    m_ghostIncoming.clear();
    info.trackId = u32(pkt);
    info.name = pkt.readWString(64);
    info.carKind = pkt.remaining() >= 4 ? pkt.readUInt32() : 3;
    info.recordTimeMs = pkt.remaining() >= 4 ? pkt.readInt32() : 0;
    if (pkt.remaining() >= 0x2C) info.driverKey = rd32(pkt.readBytes(0x2C), 4);
    if (pkt.remaining() >= 0x38) info.kartKey = rd32(pkt.readBytes(0x38), 4);
    m_ghostSession = info;
    std::printf("[session] 0x00AA track %u ghost %s kind %u record %d ms driver %u kart %u frames %zu\n", info.trackId,
                u16ToUtf8(info.name).c_str(), info.carKind, info.recordTimeMs, info.driverKey, info.kartKey, info.frames.size());
    if (onGhostSession) onGhostSession();
}

// sub 47CC20 the rank the own record and up to three rows
void Session::parseGhostResult(Packet& pkt) {
    GhostSubmitResult r;
    r.valid = true;
    r.rank = u32(pkt);
    r.mine = readRecord(pkt);
    const uint32_t n = u32(pkt);
    for (uint32_t i = 0; i < n && i < 3 && pkt.remaining() >= 0xB0; ++i) r.top.push_back(readRecord(pkt));
    m_ghostResult = r;
    std::printf("[session] 0x00B0 rank %u mine %s %d ms top %zu\n", r.rank, u16ToUtf8(r.mine.name).c_str(), r.mine.timeMs,
                r.top.size());
    if (onGhostResult) onGhostResult();
}

// tutorial complete recv 0x47C3C0 the two flags the progress row then the currency and item tails
void Session::parseLicenceTestResult(Packet& pkt) {
    if (pkt.remaining() < 20) return;
    LicenceTestResult r;
    r.valid = true;
    r.hasCurrency = u32(pkt) == 1;
    r.hasItem = u32(pkt) == 1;
    r.key = u32(pkt);
    r.passed = u32(pkt);
    u32(pkt);
    bool known = false;
    for (LicenceProgressRow& row : m_licenceProgress) if (row.key == r.key) { row.passed = r.passed; known = true; }
    if (!known) m_licenceProgress.push_back(LicenceProgressRow{r.key, r.passed});
    // the first flag is a pendant row 0x47C499 appends it blind the docs once named it currency
    if (r.hasCurrency) {
        const uint32_t inst = u32(pkt);
        const uint32_t key = u32(pkt);
        storePendant(inst, key, false);
    }
    if (r.hasItem) {
        const uint32_t type = u32(pkt);
        storeRewardRecord(type, pkt);
    }
    // the stock adds the def rewards to its own profile here the 0x000A refresh after it wins
    if (const LicenceTestDef* def = m_catalog.licenceTest(r.key)) {
        m_profile.exp += def->rewardExp;
        m_profile.gold += def->rewardGold;
    }
    m_licenceResult = r;
    std::printf("[session] 0x00A3 key %u passed %u currency %d item %d\n", r.key, r.passed, r.hasCurrency ? 1 : 0, r.hasItem ? 1 : 0);
    if (onProfile) onProfile();
    if (onLicenceTestResult) onLicenceTestResult();
}

}
