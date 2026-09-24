#include "packets/gen/SocialPackets.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <cstring>
#include <string>

namespace knc {

namespace {

void putU32(uint8_t* p, size_t off, uint32_t v) {
    p[off + 0] = static_cast<uint8_t>(v & 0xFF);
    p[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

void putI32(uint8_t* p, size_t off, int32_t v) {
    putU32(p, off, static_cast<uint32_t>(v));
}

// wrong size here silently desyncs the whole stream so shout
void checkSize(const Packet& pkt, size_t expected, const char* what) {
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", std::string(what) + " size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
}

// past the cap the client drops the row and keeps a stale one
size_t capCount(size_t n, size_t cap, const char* what) {
    if (n > cap) {
        LOG_ERROR("PACKET", std::string(what) + " count " + std::to_string(n) +
                            " over client cap " + std::to_string(cap) + " extra rows dropped");
        return cap;
    }
    return n;
}

// client qmemcpy has no destination bound so long means smashed stack not truncated
std::u16string clampChars(const std::u16string& s, size_t maxChars, const char* what) {
    if (s.size() <= maxChars) return s;
    LOG_ERROR("PACKET", std::string(what) + " wstring " + std::to_string(s.size()) +
                        " chars over client buffer " + std::to_string(maxChars) + " clamped");
    return s.substr(0, maxChars);
}

std::string clampAscii(const std::string& s, size_t maxLen, const char* what) {
    if (s.size() <= maxLen) return s;
    LOG_ERROR("PACKET", std::string(what) + " ascii " + std::to_string(s.size()) +
                        " chars over client buffer " + std::to_string(maxLen) + " clamped");
    size_t nulProcessedLen = 0;
    return clampAsciiCore(s, maxLen, AsciiNulMode::kIgnoreNul, nulProcessedLen);
}

// fixed width utf16le cell zero pad keep last cell null
void putFixedWName(uint8_t* p, size_t off, size_t byteSize,
                   const std::u16string& name, const char* what) {
    const size_t cells = byteSize / 2;
    const std::u16string src = clampChars(name, cells - 1, what);
    for (size_t i = 0; i < cells; ++i) {
        const uint16_t c = i < src.size() ? static_cast<uint16_t>(src[i]) : 0u;
        p[off + i * 2 + 0] = static_cast<uint8_t>(c & 0xFF);
        p[off + i * 2 + 1] = static_cast<uint8_t>((c >> 8) & 0xFF);
    }
}

// past 63 the pendant sprite lookup walks off the loaded array
int32_t clampPendant(int32_t slot) {
    if (slot > static_cast<int32_t>(SocialPackets::MAX_PENDANT_SLOT)) {
        LOG_ERROR("PACKET", "userList pendant slot " + std::to_string(slot) +
                            " over sprite range clamped to 63");
        return static_cast<int32_t>(SocialPackets::MAX_PENDANT_SLOT);
    }
    return slot;
}

// anything above 4 falls off the jump table and renders nothing
uint32_t clampChatType(uint32_t type) {
    if (type > SocialPackets::CHAT_TYPE_MAX) {
        LOG_ERROR("PACKET", "chat type " + std::to_string(type) + " past jump table dropped by client");
    }
    return type;
}

// const cursor because the parsers must not mutate the caller packet
class Cursor {
public:
    Cursor(const uint8_t* data, size_t len) : m_data(data), m_len(len) {}

    bool readU32(uint32_t& out) {
        if (m_pos + 4 > m_len) return false;
        out = static_cast<uint32_t>(m_data[m_pos]) |
              (static_cast<uint32_t>(m_data[m_pos + 1]) << 8) |
              (static_cast<uint32_t>(m_data[m_pos + 2]) << 16) |
              (static_cast<uint32_t>(m_data[m_pos + 3]) << 24);
        m_pos += 4;
        return true;
    }

    bool readWString(std::u16string& out, size_t maxChars) {
        out.clear();
        for (;;) {
            if (m_pos + 2 > m_len) return false;
            const uint16_t c = static_cast<uint16_t>(m_data[m_pos]) |
                               static_cast<uint16_t>(static_cast<uint16_t>(m_data[m_pos + 1]) << 8);
            m_pos += 2;
            if (c == 0) return true;
            if (out.size() >= maxChars) return false;
            out.push_back(static_cast<char16_t>(c));
        }
    }

    size_t remaining() const { return m_len - m_pos; }

private:
    const uint8_t* m_data;
    size_t m_len;
    size_t m_pos = 0;
};

Cursor cursorOf(const Packet& pkt) {
    return Cursor(pkt.payload().data(), pkt.payload().size());
}

void warnTrailing(const Cursor& cur, const char* what) {
    if (cur.remaining() != 0) {
        LOG_WARN("PACKET", std::string(what) + " has " + std::to_string(cur.remaining()) +
                           " trailing bytes ignored");
    }
}

// inbound bounds so a patched client cannot make the server allocate freely
constexpr size_t IN_MAX_NAME_CHARS = 64;
constexpr size_t IN_MAX_TEXT_CHARS = 512;

Packet chatBase(uint32_t playerId, const std::u16string& senderName,
                const std::u16string& text, uint32_t chatType) {
    // sender name lands in a 28 byte stack slot and name colon text lands in 256 wchar
    const std::u16string name = clampChars(senderName, SocialPackets::MAX_NICKNAME_CHARS,
                                           "chat sender name");
    const size_t textRoom = SocialPackets::MAX_CHAT_TEXT_CHARS > name.size()
                          ? SocialPackets::MAX_CHAT_TEXT_CHARS - name.size()
                          : 0;
    const std::u16string body = clampChars(text, textRoom, "chat text");

    Packet pkt = Packet::fromCmdFull(SocialPackets::OP_CHAT);
    pkt.writeUInt32(playerId);
    pkt.writeWString(name);
    pkt.writeWString(body);
    pkt.writeUInt32(clampChatType(chatType));

    const size_t expected = 4 + 2 * (name.size() + 1) + 2 * (body.size() + 1) + 4;
    checkSize(pkt, expected, "chatBroadcast");
    return pkt;
}

}  // namespace

std::array<uint8_t, 0x2C> SocialPackets::friendRecord(const FriendRow& row) {
    std::array<uint8_t, 0x2C> rec{};
    putU32(rec.data(), 0x00, row.playerId);
    putFixedWName(rec.data(), 0x04, 28, row.name, "friend record name");
    putU32(rec.data(), 0x20, row.level);
    putI32(rec.data(), 0x24, row.statusA);
    putI32(rec.data(), 0x28, row.statusB);
    return rec;
}

std::array<uint8_t, 0x24> SocialPackets::requestRecord(const RequestRow& row) {
    std::array<uint8_t, 0x24> rec{};
    putU32(rec.data(), 0x00, row.requesterId);
    putFixedWName(rec.data(), 0x04, 28, row.name, "request record name");
    putU32(rec.data(), 0x20, row.level);
    return rec;
}

std::array<uint8_t, 0x20> SocialPackets::blockRecord(const BlockRow& row) {
    std::array<uint8_t, 0x20> rec{};
    putU32(rec.data(), 0x00, row.playerId);
    putFixedWName(rec.data(), 0x04, 28, row.name, "block record name");
    return rec;
}

std::array<uint8_t, 0x68> SocialPackets::profileBlob(const Profile& profile) {
    std::array<uint8_t, 0x68> rec{};
    putU32(rec.data(), 0x00, profile.playerId);
    putFixedWName(rec.data(), 0x04, 26, profile.name, "profile name");
    rec[0x1E] = static_cast<uint8_t>(profile.level);
    // offsets 0x1F 0x20 to 0x27 0x2C 0x34 to 0x3B 0x44 0x54 have no reader and stay zero
    putU32(rec.data(), 0x28, profile.characterKey);
    putI32(rec.data(), 0x30, profile.expCurrent);
    putI32(rec.data(), 0x3C, profile.expLevelBase);
    putI32(rec.data(), 0x40, profile.expLevelNext);
    putI32(rec.data(), 0x48, profile.statA1);
    putI32(rec.data(), 0x4C, profile.statB1);
    putI32(rec.data(), 0x50, profile.statC1);
    putI32(rec.data(), 0x58, profile.statA2);
    putI32(rec.data(), 0x5C, profile.statB2);
    putI32(rec.data(), 0x60, profile.statC2);
    putU32(rec.data(), 0x64, profile.pendantKey);
    return rec;
}

std::array<uint8_t, 0x4C8> SocialPackets::userInfoStub(uint32_t playerId,
                                                       const std::u16string& nickname) {
    // only offset 0x00 and offset 0x486 are proven and only those two are read by 0x00B5
    std::array<uint8_t, 0x4C8> info{};
    putU32(info.data(), 0x000, playerId);
    putFixedWName(info.data(), 0x486, 26, nickname, "userinfo nickname");
    return info;
}

Packet SocialPackets::chatBroadcast(uint32_t playerId,
                                    const std::u16string& senderName,
                                    const std::u16string& text,
                                    uint32_t chatType) {
    return chatBase(playerId, senderName, text, chatType);
}

Packet SocialPackets::chatNotice(const std::u16string& text) {
    // type 3 ignores the name so send it empty
    return chatBase(0, std::u16string(), text, CHAT_NOTICE);
}

Packet SocialPackets::smallTalkLine(uint32_t playerId,
                                    const std::u16string& senderName,
                                    const std::u16string& text) {
    return chatBase(playerId, senderName, text, CHAT_SMALLTALK);
}

Packet SocialPackets::whisperDeliver(const std::array<uint8_t, 0x4C8>& senderUserInfo,
                                     const std::array<uint8_t, 0x4C8>& receiverUserInfo,
                                     const std::u16string& text) {
    const std::u16string body = clampChars(text, MAX_WHISPER_CHARS, "whisper text");

    Packet pkt = Packet::fromCmdFull(OP_WHISPER);
    pkt.writeBytes(senderUserInfo.data(), senderUserInfo.size());
    pkt.writeBytes(receiverUserInfo.data(), receiverUserInfo.size());
    pkt.writeWString(body);

    const size_t expected = SIZE_USERINFO * 2 + 2 * (body.size() + 1);
    checkSize(pkt, expected, "whisperDeliver");
    return pkt;
}

Packet SocialPackets::whisperPrompt() {
    // handler reads nothing any byte here would desync the next frame
    Packet pkt = Packet::fromCmdFull(OP_WHISPER_PROMPT);
    checkSize(pkt, 0, "whisperPrompt");
    return pkt;
}

Packet SocialPackets::systemChatLine(const std::u16string& name,
                                     const std::string& messageKey,
                                     uint32_t type) {
    if (type != 5) {
        LOG_WARN("PACKET", "systemChatLine type " + std::to_string(type) +
                           " is a no op in the client only 5 renders");
    }
    const std::u16string who = clampChars(name, MAX_NICKNAME_CHARS, "system line name");
    const std::string key = clampAscii(messageKey, MAX_MESSAGE_KEY_LEN, "system line key");

    Packet pkt = Packet::fromCmdFull(OP_SYSTEM_CHAT_LINE);
    pkt.writeUInt32(0);        // read then never used
    pkt.writeWString(who);
    pkt.writeString(key);      // swapping the wstring and the ascii field desyncs the rest
    pkt.writeUInt32(type);

    const size_t expected = 4 + 2 * (who.size() + 1) + (key.size() + 1) + 4;
    checkSize(pkt, expected, "systemChatLine");
    return pkt;
}

Packet SocialPackets::userListPage(uint32_t currentPage,
                                   uint32_t pageCount,
                                   const std::vector<UserListEntry>& entries) {
    const size_t used = capCount(entries.size(), USERLIST_SLOTS, "userListPage");

    Packet pkt = Packet::fromCmdFull(OP_USERLIST_PAGE);
    pkt.writeUInt32(currentPage);
    pkt.writeUInt32(pageCount);
    pkt.writeUInt32(USERLIST_VPTR);

    for (size_t i = 0; i < USERLIST_SLOTS; ++i) {
        std::array<uint8_t, SIZE_USERLIST_ENTRY> slot{};
        if (i < used) {
            const UserListEntry& e = entries[i];
            putU32(slot.data(), 0x00, e.playerId);
            putFixedWName(slot.data(), 0x04, 28, e.name, "userlist entry name");
            putU32(slot.data(), 0x20, e.level);
            putI32(slot.data(), 0x24, clampPendant(e.pendantSlot));
        }
        pkt.writeBytes(slot.data(), slot.size());
    }

    pkt.writeUInt32(static_cast<uint32_t>(used));

    checkSize(pkt, SIZE_USERLIST_PAGE, "userListPage");
    return pkt;
}

Packet SocialPackets::userInfoPopup(const Profile& profile) {
    const auto blob = profileBlob(profile);
    Packet pkt = Packet::fromCmdFull(OP_USERINFO_POPUP);
    pkt.writeBytes(blob.data(), blob.size());
    checkSize(pkt, SIZE_PROFILE, "userInfoPopup");
    return pkt;
}

Packet SocialPackets::userInfoBlob(const Profile& profile) {
    const auto blob = profileBlob(profile);
    Packet pkt = Packet::fromCmdFull(OP_USERINFO_BY_NAME);
    pkt.writeBytes(blob.data(), blob.size());
    checkSize(pkt, SIZE_PROFILE, "userInfoBlob");
    return pkt;
}

Packet SocialPackets::friendList(const std::vector<FriendRow>& rows) {
    const size_t used = capCount(rows.size(), CAP_FRIENDS, "friendList");

    Packet pkt = Packet::fromCmdFull(OP_FRIEND_LIST);
    pkt.writeInt32(static_cast<int32_t>(used));
    for (size_t i = 0; i < used; ++i) {
        const auto rec = friendRecord(rows[i]);
        pkt.writeBytes(rec.data(), rec.size());
    }

    checkSize(pkt, 4 + used * SIZE_FRIEND_REC, "friendList");
    return pkt;
}

Packet SocialPackets::friendRequestList(const std::vector<RequestRow>& rows) {
    const size_t used = capCount(rows.size(), CAP_FRIEND_REQUESTS, "friendRequestList");

    Packet pkt = Packet::fromCmdFull(OP_FRIEND_REQUEST_LIST);
    pkt.writeInt32(static_cast<int32_t>(used));
    for (size_t i = 0; i < used; ++i) {
        const auto rec = requestRecord(rows[i]);
        pkt.writeBytes(rec.data(), rec.size());
    }

    checkSize(pkt, 4 + used * SIZE_REQUEST_REC, "friendRequestList");
    return pkt;
}

Packet SocialPackets::friendStatusFull(const std::vector<StatusRow>& rows) {
    // an id missing from the client friend list null derefs sub 44F050
    Packet pkt = Packet::fromCmdFull(OP_FRIEND_STATUS_FULL);
    pkt.writeInt32(static_cast<int32_t>(rows.size()));
    for (const auto& r : rows) {
        pkt.writeUInt32(r.playerId);
        pkt.writeInt32(r.statusA);
        pkt.writeInt32(r.statusB);
    }

    checkSize(pkt, 4 + rows.size() * 12, "friendStatusFull");
    return pkt;
}

Packet SocialPackets::friendStatusPartial(const std::vector<StatusPartialRow>& rows) {
    Packet pkt = Packet::fromCmdFull(OP_FRIEND_STATUS_PART);
    pkt.writeInt32(static_cast<int32_t>(rows.size()));
    for (const auto& r : rows) {
        pkt.writeUInt32(r.playerId);
        pkt.writeInt32(r.statusB);
    }

    checkSize(pkt, 4 + rows.size() * 8, "friendStatusPartial");
    return pkt;
}

Packet SocialPackets::friendAddResult(uint32_t messageCode) {
    if (messageCode == MSG_FRIEND_ADD_OK || messageCode == MSG_FRIEND_REQ_QUEUED) {
        LOG_ERROR("PACKET", "friendAddResult code " + std::to_string(messageCode) +
                            " needs a trailing record use the accepted or queued builder");
    }
    Packet pkt = Packet::fromCmdFull(OP_FRIEND_ADD);
    pkt.writeUInt32(messageCode);
    checkSize(pkt, 4, "friendAddResult");
    return pkt;
}

Packet SocialPackets::friendAddResultAccepted(const FriendRow& row) {
    const auto rec = friendRecord(row);
    Packet pkt = Packet::fromCmdFull(OP_FRIEND_ADD);
    pkt.writeUInt32(MSG_FRIEND_ADD_OK);
    pkt.writeBytes(rec.data(), rec.size());
    checkSize(pkt, 4 + SIZE_FRIEND_REC, "friendAddResultAccepted");
    return pkt;
}

Packet SocialPackets::friendAddResultQueued(const RequestRow& row) {
    const auto rec = requestRecord(row);
    Packet pkt = Packet::fromCmdFull(OP_FRIEND_ADD);
    pkt.writeUInt32(MSG_FRIEND_REQ_QUEUED);
    pkt.writeBytes(rec.data(), rec.size());
    checkSize(pkt, 4 + SIZE_REQUEST_REC, "friendAddResultQueued");
    return pkt;
}

Packet SocialPackets::friendDelResult(uint32_t friendPlayerId, uint32_t messageCode) {
    Packet pkt = Packet::fromCmdFull(OP_FRIEND_DEL);
    pkt.writeUInt32(friendPlayerId);
    pkt.writeUInt32(messageCode);
    checkSize(pkt, 8, "friendDelResult");
    return pkt;
}

Packet SocialPackets::friendRequestResolved(uint32_t requesterId, bool rejected) {
    const uint16_t op = rejected ? OP_FRIEND_REQ_REJECT : OP_FRIEND_REQ_ACCEPT;
    Packet pkt = Packet::fromCmdFull(op);
    pkt.writeUInt32(0);  // read then never used
    pkt.writeUInt32(requesterId);
    checkSize(pkt, 8, "friendRequestResolved");
    return pkt;
}

Packet SocialPackets::blockList(const std::vector<BlockRow>& rows) {
    const size_t used = capCount(rows.size(), CAP_BLOCKS, "blockList");

    Packet pkt = Packet::fromCmdFull(OP_BLOCK_LIST);
    pkt.writeInt32(static_cast<int32_t>(used));
    for (size_t i = 0; i < used; ++i) {
        const auto rec = blockRecord(rows[i]);
        pkt.writeBytes(rec.data(), rec.size());
    }

    checkSize(pkt, 4 + used * SIZE_BLOCK_REC, "blockList");
    return pkt;
}

Packet SocialPackets::blockAddResultOk(const BlockRow& row) {
    const auto rec = blockRecord(row);
    Packet pkt = Packet::fromCmdFull(OP_BLOCK_ADD);
    pkt.writeUInt32(BLOCK_ADD_OK);
    pkt.writeBytes(rec.data(), rec.size());
    checkSize(pkt, 4 + SIZE_BLOCK_REC, "blockAddResultOk");
    return pkt;
}

Packet SocialPackets::blockAddResultError(uint32_t result) {
    // zero is success here so a zero error code would append garbage
    if (result == BLOCK_ADD_OK) {
        LOG_ERROR("PACKET", "blockAddResultError called with the success code zero");
    }
    Packet pkt = Packet::fromCmdFull(OP_BLOCK_ADD);
    pkt.writeUInt32(result);
    checkSize(pkt, 4, "blockAddResultError");
    return pkt;
}

Packet SocialPackets::blockDelResult(uint32_t playerId, uint32_t result) {
    Packet pkt = Packet::fromCmdFull(OP_BLOCK_DEL);
    pkt.writeUInt32(playerId);
    pkt.writeUInt32(result);  // read then discarded but must be present
    checkSize(pkt, 8, "blockDelResult");
    return pkt;
}

Packet SocialPackets::messengerResult(uint32_t messageCode) {
    Packet pkt = Packet::fromCmdFull(OP_NOTE);
    pkt.writeUInt32(messageCode);
    checkSize(pkt, 4, "messengerResult");
    return pkt;
}

Packet SocialPackets::smallTalkInvite(uint32_t requesterId, const std::u16string& requesterName) {
    // destination size unproven so hold to the nickname input cap
    const std::u16string who = clampChars(requesterName, MAX_NICKNAME_CHARS, "smalltalk name");

    Packet pkt = Packet::fromCmdFull(OP_SMALLTALK_REQ);
    pkt.writeUInt32(requesterId);
    pkt.writeWString(who);

    checkSize(pkt, 4 + 2 * (who.size() + 1), "smallTalkInvite");
    return pkt;
}

Packet SocialPackets::roomInvite(const RoomInvite& invite) {
    const std::u16string who = clampChars(invite.inviterName, MAX_NICKNAME_CHARS,
                                          "room invite name");
    // accept path stages this through a wchar buffer of 10 before opcode 110
    const std::u16string pass = clampChars(invite.roomPassword, 9, "room invite password");

    Packet pkt = Packet::fromCmdFull(OP_ROOM_INVITE);
    pkt.writeUInt32(invite.replyKey);
    pkt.writeWString(who);
    pkt.writeString(std::string());  // ascii slot and the next u32 are both unread
    pkt.writeUInt32(0);
    pkt.writeUInt32(invite.roomId);
    pkt.writeWString(pass);
    pkt.writeUInt8(invite.flag);

    const size_t expected = 4 + 2 * (who.size() + 1) + 1 + 4 + 4 + 2 * (pass.size() + 1) + 1;
    checkSize(pkt, expected, "roomInvite");
    return pkt;
}

Packet SocialPackets::deadDialog37(const std::u16string& name,
                                   const std::string& asciiText,
                                   uint32_t value) {
    const std::u16string who = clampChars(name, MAX_NICKNAME_CHARS, "dead dialog 37 name");
    const std::string msg = clampAscii(asciiText, MAX_DEAD_ASCII_LEN, "dead dialog 37 text");

    Packet pkt = Packet::fromCmdFull(OP_DEAD_DIALOG_37);
    pkt.writeWString(who);
    pkt.writeString(msg);
    pkt.writeUInt32(value);

    checkSize(pkt, 2 * (who.size() + 1) + (msg.size() + 1) + 4, "deadDialog37");
    return pkt;
}

Packet SocialPackets::deadDialog39(uint32_t id,
                                   const std::u16string& name,
                                   const std::string& asciiText) {
    const std::u16string who = clampChars(name, MAX_NICKNAME_CHARS, "dead dialog 39 name");
    const std::string msg = clampAscii(asciiText, MAX_DEAD_ASCII_LEN, "dead dialog 39 text");

    Packet pkt = Packet::fromCmdFull(OP_DEAD_DIALOG_39);
    pkt.writeUInt32(id);
    pkt.writeWString(who);
    pkt.writeString(msg);

    checkSize(pkt, 4 + 2 * (who.size() + 1) + (msg.size() + 1), "deadDialog39");
    return pkt;
}

bool SocialPackets::parseChatSend(const Packet& pkt, ChatSend& out) {
    Cursor cur = cursorOf(pkt);
    std::u16string text;
    uint32_t type = 0;
    if (!cur.readWString(text, IN_MAX_TEXT_CHARS)) return false;
    if (!cur.readU32(type)) return false;
    warnTrailing(cur, "chatSend");

    out.text = std::move(text);
    out.chatType = type;
    return true;
}

bool SocialPackets::parseWhisperSend(const Packet& pkt, WhisperSend& out) {
    Cursor cur = cursorOf(pkt);
    uint32_t target = 0;
    std::u16string text;
    if (!cur.readU32(target)) return false;
    if (!cur.readWString(text, IN_MAX_TEXT_CHARS)) return false;
    warnTrailing(cur, "whisperSend");

    out.targetPlayerId = target;
    out.text = std::move(text);
    return true;
}

bool SocialPackets::parseNoteSend(const Packet& pkt, NoteSend& out) {
    Cursor cur = cursorOf(pkt);
    std::u16string recipient;
    std::u16string body;
    if (!cur.readWString(recipient, IN_MAX_NAME_CHARS)) return false;
    if (!cur.readWString(body, IN_MAX_TEXT_CHARS)) return false;
    warnTrailing(cur, "noteSend");

    out.recipientName = std::move(recipient);
    out.body = std::move(body);
    return true;
}

namespace {

void putWide(uint8_t* rec, size_t off, size_t capChars, const std::u16string& s) {
    // capChars counts the terminator and the record is zero filled so it falls out
    const size_t n = s.size() < capChars ? s.size() : capChars - 1;
    for (size_t i = 0; i < n; ++i) {
        const uint16_t c = static_cast<uint16_t>(s[i]);
        rec[off + i * 2]     = static_cast<uint8_t>(c & 0xFF);
        rec[off + i * 2 + 1] = static_cast<uint8_t>(c >> 8);
    }
}

Packet noteRecord(uint16_t op, const SocialPackets::NoteRow& row) {
    uint8_t rec[SocialPackets::NOTE_RECORD_SIZE];
    std::memset(rec, 0, sizeof(rec));
    rec[0] = static_cast<uint8_t>(row.id & 0xFF);
    rec[1] = static_cast<uint8_t>((row.id >> 8) & 0xFF);
    rec[2] = static_cast<uint8_t>((row.id >> 16) & 0xFF);
    rec[3] = static_cast<uint8_t>((row.id >> 24) & 0xFF);
    putWide(rec, 0x04, 13, row.sender);
    putWide(rec, 0x1E, 11, row.date);
    putWide(rec, 0x34, 9, row.time);
    rec[0x46] = row.read ? 1 : 0;
    putWide(rec, 0x48, (SocialPackets::NOTE_RECORD_SIZE - 0x48) / 2, row.body);
    Packet pkt = Packet::fromCmdFull(op);
    pkt.writeBytes(rec, sizeof(rec));
    return pkt;
}

}  // namespace

Packet SocialPackets::noteAppend(const NoteRow& row) {
    Packet pkt = noteRecord(OP_NOTE_APPEND, row);
    checkSize(pkt, NOTE_RECORD_SIZE, "noteAppend");
    return pkt;
}

Packet SocialPackets::noteAppendSorted(const NoteRow& row) {
    Packet pkt = noteRecord(OP_NOTE_APPEND_SORTED, row);
    checkSize(pkt, NOTE_RECORD_SIZE, "noteAppendSorted");
    return pkt;
}

Packet SocialPackets::noteAck(uint16_t op, uint32_t noteId) {
    Packet pkt = Packet::fromCmdFull(op);
    pkt.writeUInt32(noteId);
    checkSize(pkt, 4, "noteAck");
    return pkt;
}

bool SocialPackets::parseRoomInviteAnswer(const Packet& pkt, RoomInviteAnswer& out) {
    if (pkt.payload().size() != 8) return false;
    Cursor cur = cursorOf(pkt);
    uint32_t key = 0;
    uint32_t answer = 0;
    if (!cur.readU32(key)) return false;
    if (!cur.readU32(answer)) return false;

    out.replyKey = key;
    out.answer = answer;
    return true;
}

bool SocialPackets::parseUserListPageReq(const Packet& pkt, uint32_t& pageIndex) {
    return parseIdRequest(pkt, pageIndex);
}

bool SocialPackets::parseUserInfoReqById(const Packet& pkt, uint32_t& playerId) {
    return parseIdRequest(pkt, playerId);
}

bool SocialPackets::parseUserInfoReqByName(const Packet& pkt, std::u16string& name) {
    return parseNameRequest(pkt, name);
}

bool SocialPackets::parseFriendStatusPoll(const Packet& pkt) {
    return pkt.payload().empty();
}

bool SocialPackets::parseNameRequest(const Packet& pkt, std::u16string& name) {
    Cursor cur = cursorOf(pkt);
    std::u16string value;
    if (!cur.readWString(value, IN_MAX_NAME_CHARS)) return false;
    warnTrailing(cur, "nameRequest");

    name = std::move(value);
    return true;
}

bool SocialPackets::parseIdRequest(const Packet& pkt, uint32_t& playerId) {
    if (pkt.payload().size() != 4) return false;
    Cursor cur = cursorOf(pkt);
    uint32_t value = 0;
    if (!cur.readU32(value)) return false;

    playerId = value;
    return true;
}

SocialPackets::ChatCommand SocialPackets::splitChatCommand(const std::u16string& raw) {
    ChatCommand cmd;
    cmd.text = raw;

    if (raw.size() < 2 || raw[0] != u'/') return cmd;
    // client only ever composes lowercase w and t prefixes
    if (raw[1] != u'w' && raw[1] != u't') return cmd;
    if (raw.size() > 2 && raw[2] != u' ') return cmd;

    const bool whisper = raw[1] == u'w';
    size_t pos = raw.size() > 2 ? 3 : raw.size();
    while (pos < raw.size() && raw[pos] == u' ') ++pos;

    if (!whisper) {
        cmd.scope = ChatScope::Team;
        cmd.target.clear();
        cmd.text = raw.substr(pos);
        return cmd;
    }

    size_t end = pos;
    while (end < raw.size() && raw[end] != u' ') ++end;
    cmd.scope = ChatScope::Whisper;
    cmd.target = raw.substr(pos, end - pos);

    while (end < raw.size() && raw[end] == u' ') ++end;
    cmd.text = raw.substr(end);
    return cmd;
}

}  // namespace knc
