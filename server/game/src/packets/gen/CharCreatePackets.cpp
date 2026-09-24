#include "packets/gen/CharCreatePackets.h"
#include "logging/Logger.h"

#include <string>

namespace knc {

namespace {

// wrong size here silently desyncs the whole stream loud is better
void assertSize(const Packet& pkt, size_t expected, const char* what) {
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", std::string(what) + " size " +
                            std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
}

void putU32(uint8_t* p, size_t off, uint32_t v) {
    p[off + 0] = static_cast<uint8_t>(v & 0xFF);
    p[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

// in place utf16le the caller buffer is already zero so the NUL is free
void putWStringAt(uint8_t* p, size_t off, size_t capBytes, const std::u16string& s) {
    const size_t capChars = capBytes / 2;
    if (capChars == 0) return;
    size_t n = s.size();
    if (n > capChars - 1) n = capChars - 1;
    for (size_t i = 0; i < n; ++i) {
        p[off + i * 2 + 0] = static_cast<uint8_t>(s[i] & 0xFF);
        p[off + i * 2 + 1] = static_cast<uint8_t>((s[i] >> 8) & 0xFF);
    }
}

// reader refuses to run past the payload end so a short frame is caught here
class Cursor {
public:
    explicit Cursor(const std::vector<uint8_t>& buf) : m_buf(buf) {}

    bool bad() const { return m_bad; }

    uint32_t readU32() {
        if (m_pos + 4 > m_buf.size()) { m_bad = true; return 0; }
        const uint32_t v = static_cast<uint32_t>(m_buf[m_pos + 0]) |
                          (static_cast<uint32_t>(m_buf[m_pos + 1]) << 8) |
                          (static_cast<uint32_t>(m_buf[m_pos + 2]) << 16) |
                          (static_cast<uint32_t>(m_buf[m_pos + 3]) << 24);
        m_pos += 4;
        return v;
    }

    std::string readCString(size_t maxChars) {
        std::string out;
        while (m_pos < m_buf.size()) {
            const uint8_t c = m_buf[m_pos++];
            if (c == 0) return out;
            if (out.size() >= maxChars) { m_bad = true; return std::string(); }
            out.push_back(static_cast<char>(c));
        }
        m_bad = true;
        return std::string();
    }

    std::u16string readWString(size_t maxChars) {
        std::u16string out;
        while (m_pos + 1 < m_buf.size()) {
            const char16_t c = static_cast<char16_t>(
                m_buf[m_pos] | (static_cast<uint16_t>(m_buf[m_pos + 1]) << 8));
            m_pos += 2;
            if (c == 0) return out;
            if (out.size() >= maxChars) { m_bad = true; return std::u16string(); }
            out.push_back(c);
        }
        m_bad = true;
        return std::u16string();
    }

private:
    const std::vector<uint8_t>& m_buf;
    size_t m_pos = 0;
    bool m_bad = false;
};

} // namespace

std::array<uint8_t, CharCreatePackets::PROFILE_BLOB_SIZE>
CharCreatePackets::profileBlob(const LoginProfile& profile) {
    std::array<uint8_t, PROFILE_BLOB_SIZE> blob{};
    uint8_t* d = blob.data();

    putU32(d, 0x000, profile.playerId);
    putU32(d, 0x004, profile.reauthSessionId);

    // token lives in place inside the dead region rest of it stays zero
    if (profile.reauthToken.size() > REAUTH_TOKEN_MAX_CHARS) {
        LOG_ERROR("PACKET", "profileBlob reauth token too long chars " +
                            std::to_string(profile.reauthToken.size()));
    }
    putWStringAt(d, 0x008, 0x482, profile.reauthToken);

    // longer name would eat the level band and both selection ids
    if (profile.nickname.size() > NICKNAME_MAX_CHARS) {
        LOG_WARN("PACKET", "profileBlob nickname truncated chars " +
                           std::to_string(profile.nickname.size()));
    }
    putWStringAt(d, 0x48A, 26, profile.nickname);

    d[0x4A4] = profile.channelLevelBand;
    d[0x4A5] = profile.level;
    // pad at 0x4A6 and 0x4A7 exists only inside this blob

    putU32(d, 0x4A8, profile.expCurrent);
    putU32(d, 0x4AC, profile.astro);
    putU32(d, 0x4B0, profile.gold);
    putU32(d, 0x4B4, profile.selectedCharacterId);
    putU32(d, 0x4B8, profile.selectedKartId);

    d[0x4BC] = 0;  // unknown no reader found
    d[0x4BD] = profile.sessionRole;
    d[0x4BE] = 0;
    d[0x4BF] = 0;  // unknown client seeds it itself at startup

    putU32(d, 0x4C0, profile.expFloorCurrentLevel);
    putU32(d, 0x4C4, profile.expRequiredNextLevel);
    putU32(d, 0x4C8, profile.equippedTitleKey);

    return blob;
}

Packet CharCreatePackets::loginAccept(const LoginProfile& profile) {
    Packet pkt(OP_S_LOGIN_ACCEPT);
    const auto blob = profileBlob(profile);
    pkt.writeBytes(blob.data(), blob.size());
    assertSize(pkt, PROFILE_BLOB_SIZE, "loginAccept");
    return pkt;
}

Packet CharCreatePackets::reauthAccept(const LoginProfile& profile) {
    Packet pkt(OP_S_REAUTH_ACCEPT);
    const auto blob = profileBlob(profile);
    pkt.writeBytes(blob.data(), blob.size());
    assertSize(pkt, PROFILE_BLOB_SIZE, "reauthAccept");
    return pkt;
}

Packet CharCreatePackets::profileRefresh(const LoginProfile& profile) {
    // same fields as the blob tail but the two pad bytes are NOT on this wire
    Packet pkt(OP_S_PROFILE_REFRESH);

    pkt.writeUInt8(profile.channelLevelBand);
    pkt.writeUInt8(profile.level);
    pkt.writeUInt32(profile.expCurrent);
    pkt.writeUInt32(profile.astro);
    pkt.writeUInt32(profile.gold);
    pkt.writeUInt32(profile.selectedCharacterId);
    pkt.writeUInt32(profile.selectedKartId);
    pkt.writeUInt8(0);  // unknown no reader found
    pkt.writeUInt8(profile.sessionRole);
    pkt.writeUInt8(0);
    pkt.writeUInt8(0);  // unknown no reader found
    pkt.writeUInt32(profile.expFloorCurrentLevel);
    pkt.writeUInt32(profile.expRequiredNextLevel);
    pkt.writeUInt32(profile.equippedTitleKey);

    assertSize(pkt, PROFILE_REFRESH_SIZE, "profileRefresh");
    return pkt;
}

Packet CharCreatePackets::messageKeyBox(const std::string& messageKey, uint32_t boxType) {
    // type zero has no buttons and the client can never close it
    if (boxType == BOX_TYPE_WAIT_NO_BUTTON) {
        LOG_WARN("PACKET", "messageKeyBox type zero hangs the client key " + messageKey);
    }
    // client destination is a fixed stack buffer so a long key smashes it
    if (messageKey.size() > 254) {
        LOG_ERROR("PACKET", "messageKeyBox key too long chars " +
                            std::to_string(messageKey.size()));
    }

    Packet pkt(OP_S_MESSAGE_KEY);
    pkt.writeString(messageKey);
    pkt.writeUInt32(boxType);

    assertSize(pkt, messageKey.size() + 1 + 4, "messageKeyBox");
    return pkt;
}

Packet CharCreatePackets::messageTextBox(const std::u16string& text, uint32_t boxType) {
    if (boxType == BOX_TYPE_WAIT_NO_BUTTON) {
        LOG_WARN("PACKET", "messageTextBox type zero hangs the client");
    }
    // unbounded wide read into a stack buffer on the client side
    if (text.size() > 158) {
        LOG_ERROR("PACKET", "messageTextBox text too long chars " +
                            std::to_string(text.size()));
    }

    Packet pkt(OP_S_MESSAGE_TEXT);
    pkt.writeWString(text);
    pkt.writeUInt32(boxType);

    assertSize(pkt, 2 * (text.size() + 1) + 4, "messageTextBox");
    return pkt;
}

Packet CharCreatePackets::loginFailInvalidId() {
    // key alone forces the disconnect so the type only picks the box art
    return messageKeyBox(KEY_INVALID_ID, BOX_TYPE_OK);
}

Packet CharCreatePackets::loginFailBadCredentials() {
    return messageKeyBox(KEY_REINPUT_IDPASS, BOX_TYPE_OK);
}

Packet CharCreatePackets::loginFailDbDown() {
    return messageKeyBox(KEY_DB_ACCESS_FAIL, BOX_TYPE_OK);
}

Packet CharCreatePackets::loginFailServerNotReady() {
    // no disconnect on this one so the socket stays and the client can retry
    return messageKeyBox(KEY_SERVER_NOT_READY, BOX_TYPE_OK);
}

Packet CharCreatePackets::loginFailBanned() {
    // no special case on this key so type two is what closes the socket
    return messageKeyBox(KEY_BLOCK_USER, BOX_TYPE_FATAL);
}

Packet CharCreatePackets::loginFailDuplicate() {
    return messageKeyBox(KEY_USED_ID, BOX_TYPE_FATAL);
}

Packet CharCreatePackets::loginFailKicked() {
    return messageKeyBox(KEY_USER_BAN, BOX_TYPE_FATAL);
}

Packet CharCreatePackets::loginFailGatewayFull() {
    return messageKeyBox(KEY_GATEWAY_FULL, BOX_TYPE_OK);
}

Packet CharCreatePackets::loginFailVersion() {
    return messageKeyBox(KEY_INVALID_VERSION, BOX_TYPE_OK);
}

Packet CharCreatePackets::openCharacterCreate() {
    // driver catalog must already be on the client else divide by zero crash
    Packet pkt(OP_S_OPEN_CHAR_CREATE);
    assertSize(pkt, 0, "openCharacterCreate");
    return pkt;
}

Packet CharCreatePackets::clearCatalogs() {
    Packet pkt(OP_S_CLEAR_CATALOGS);
    assertSize(pkt, 0, "clearCatalogs");
    return pkt;
}

Packet CharCreatePackets::createCharacterReject(uint32_t result) {
    // anything outside 1 2 3 hits unknown error and hard disconnects
    if (result != CREATE_INVALID_NICK &&
        result != CREATE_ALREADY_REGIST &&
        result != CREATE_INVALID_NICK_ALT) {
        LOG_ERROR("PACKET", "createCharacterReject bad result " + std::to_string(result));
    }

    Packet pkt(OP_S_CREATE_RESULT);
    pkt.writeUInt32(result);

    assertSize(pkt, 4, "createCharacterReject");
    return pkt;
}

Packet CharCreatePackets::createCharacterSuccess(
        const std::u16string& nickname,
        const std::array<uint8_t, CHARACTER_RECORD_SIZE>& characterRecord,
        const std::array<uint8_t, KART_RECORD_SIZE>& kartRecord) {
    std::u16string name = nickname;
    // destination slot is the same one the profile blob uses
    if (name.size() > NICKNAME_MAX_CHARS) {
        LOG_WARN("PACKET", "createCharacterSuccess nickname truncated chars " +
                           std::to_string(name.size()));
        name.resize(NICKNAME_MAX_CHARS);
    }

    Packet pkt(OP_S_CREATE_RESULT);
    pkt.writeUInt32(CREATE_OK);
    pkt.writeWString(name);
    pkt.writeBytes(characterRecord.data(), characterRecord.size());
    pkt.writeBytes(kartRecord.data(), kartRecord.size());

    const size_t expected = 4 + 2 * (name.size() + 1) +
                            CHARACTER_RECORD_SIZE + KART_RECORD_SIZE;
    assertSize(pkt, expected, "createCharacterSuccess");
    return pkt;
}

uint32_t CharCreatePackets::validateNickname(const std::u16string& nickname) {
    if (nickname.size() < NICKNAME_MIN_CHARS) return CREATE_INVALID_NICK;
    if (nickname.size() > NICKNAME_EDIT_MAX_CHARS) return CREATE_INVALID_NICK;
    // the stock edit box of mode 0 refuses the percent sign and sub 446280 formats with the name
    for (char16_t c : nickname) {
        if (c == u'%') return CREATE_INVALID_NICK;
    }
    return CREATE_OK;
}

std::u16string CharCreatePackets::tabooFold(const std::u16string& text) {
    static const char16_t kMarks[] = u" ~`!@#$%^&*()_-+=\\|{}[]:;\"'<,>.?/";
    std::u16string out;
    out.reserve(text.size());
    for (char16_t c : text) {
        bool mark = false;
        for (const char16_t* m = kMarks; *m != 0; ++m) {
            if (*m == c) { mark = true; break; }
        }
        if (mark) continue;
        // the server folds the list too so KKK and JesusChrist match what the client misses
        if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c - u'A' + u'a');
        out.push_back(c);
    }
    return out;
}

bool CharCreatePackets::tabooHit(const std::u16string& nickname, const std::vector<std::u16string>& words) {
    const std::u16string name = tabooFold(nickname);
    for (const std::u16string& raw : words) {
        const std::u16string word = tabooFold(raw);
        if (word.empty()) continue;
        if (name.find(word) != std::u16string::npos) return true;
    }
    return false;
}

Packet CharCreatePackets::channelList(const std::vector<ChannelEntry>& channels,
                                      const std::u16string& notice) {
    std::vector<ChannelEntry> rows = channels;
    // over the cap the client refuses the entry then disconnects itself
    if (rows.size() > CHANNEL_LIST_MAX) {
        LOG_ERROR("PACKET", "channelList over cap entries " + std::to_string(rows.size()));
        rows.resize(CHANNEL_LIST_MAX);
    }
    for (auto& r : rows) {
        if (r.name.size() > CHANNEL_NAME_MAX_CHARS) r.name.resize(CHANNEL_NAME_MAX_CHARS);
    }
    // notice goes into a fixed global with no bound at all on the client
    if (notice.size() > 255) {
        LOG_WARN("PACKET", "channelList notice very long chars " +
                           std::to_string(notice.size()));
    }

    Packet pkt(OP_S_CHANNEL_LIST);
    pkt.writeUInt32(static_cast<uint32_t>(rows.size()));

    size_t expected = 4;
    for (const auto& r : rows) {
        pkt.writeUInt32(r.id);
        pkt.writeWString(r.name);
        pkt.writeUInt32(r.population);
        pkt.writeUInt32(r.capacity);
        pkt.writeUInt32(r.tier);
        expected += 16 + 2 * (r.name.size() + 1);
    }

    // notice is read even when the count is zero
    pkt.writeWString(notice);
    expected += 2 * (notice.size() + 1);

    assertSize(pkt, expected, "channelList");
    return pkt;
}

Packet CharCreatePackets::stageGarage() {
    Packet pkt(OP_S_STAGE_GARAGE);
    assertSize(pkt, 0, "stageGarage");
    return pkt;
}

Packet CharCreatePackets::stageShop() {
    Packet pkt(OP_S_STAGE_SHOP);
    assertSize(pkt, 0, "stageShop");
    return pkt;
}

Packet CharCreatePackets::stageMenu() {
    Packet pkt(OP_S_STAGE_MENU);
    assertSize(pkt, 0, "stageMenu");
    return pkt;
}

Packet CharCreatePackets::stageLobby() {
    Packet pkt(OP_S_STAGE_LOBBY);
    assertSize(pkt, 0, "stageLobby");
    return pkt;
}

Packet CharCreatePackets::stageTutorial() {
    // this one closes the modal before the stage swap unlike the other four
    Packet pkt(OP_S_STAGE_TUTORIAL);
    assertSize(pkt, 0, "stageTutorial");
    return pkt;
}

Packet CharCreatePackets::serverRedirect(const std::string& host, uint32_t port, uint32_t mode) {
    // client copies host into a fixed stack buffer then into a smaller one
    if (host.size() > 127) {
        LOG_ERROR("PACKET", "serverRedirect host too long chars " +
                            std::to_string(host.size()));
    }

    Packet pkt(OP_S_SERVER_REDIRECT);
    pkt.writeString(host);
    pkt.writeUInt32(port);
    pkt.writeUInt32(mode);

    assertSize(pkt, host.size() + 1 + 8, "serverRedirect");
    return pkt;
}

CharCreatePackets::LoginRequest CharCreatePackets::parseLogin(const Packet& pkt) {
    LoginRequest req;
    Cursor cur(pkt.payload());

    req.clientVersion = cur.readCString(32);
    req.targetStage   = cur.readU32();
    req.accountId     = cur.readWString(ACCOUNT_FIELD_MAX_CHARS);
    req.password      = cur.readWString(ACCOUNT_FIELD_MAX_CHARS);

    req.ok = !cur.bad();
    if (!req.ok) {
        LOG_WARN("PACKET", "parseLogin malformed payload bytes " +
                           std::to_string(pkt.payload().size()));
    }
    return req;
}

CharCreatePackets::ReauthRequest CharCreatePackets::parseReauth(const Packet& pkt) {
    ReauthRequest req;
    Cursor cur(pkt.payload());

    // field order is cstr then u32 then wstring then u32
    req.clientVersion   = cur.readCString(32);
    req.targetStage     = cur.readU32();
    req.reauthToken     = cur.readWString(REAUTH_TOKEN_MAX_CHARS);
    req.reauthSessionId = cur.readU32();

    req.ok = !cur.bad();
    if (!req.ok) {
        LOG_WARN("PACKET", "parseReauth malformed payload bytes " +
                           std::to_string(pkt.payload().size()));
    }
    return req;
}

CharCreatePackets::CreateCharacterRequest
CharCreatePackets::parseCreateCharacter(const Packet& pkt) {
    CreateCharacterRequest req;
    Cursor cur(pkt.payload());

    req.driverKey = cur.readU32();
    // read wide then let validateNickname answer else client waits forever
    req.nickname  = cur.readWString(64);

    req.ok = !cur.bad();
    if (!req.ok) {
        LOG_WARN("PACKET", "parseCreateCharacter malformed payload bytes " +
                           std::to_string(pkt.payload().size()));
    }
    return req;
}

CharCreatePackets::ChannelSelectRequest
CharCreatePackets::parseChannelSelect(const Packet& pkt) {
    ChannelSelectRequest req;
    Cursor cur(pkt.payload());

    req.requestedStage = cur.readU32();
    req.channelId      = static_cast<int32_t>(cur.readU32());

    req.ok = !cur.bad();
    if (!req.ok) {
        LOG_WARN("PACKET", "parseChannelSelect malformed payload bytes " +
                           std::to_string(pkt.payload().size()));
    }
    return req;
}

bool CharCreatePackets::isSupportedVersion(const std::string& clientVersion) {
    return clientVersion == CLIENT_VERSION;
}

} // namespace knc
