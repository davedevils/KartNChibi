#include "GameServerInternal.h"
#include "handlers/CharCreateHandler.h"
#include "handlers/ProgressionHandler.h"
#include "handlers/InventoryHandler.h"
#include "handlers/ShopHandler.h"
#include "packets/PacketBuilder.h"
#include "packets/gen/InventoryPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"
#include "GameServer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace knc {

namespace {

const char* const TAG = "CHARCREATE";

using DbRow = std::map<std::string, std::string>;

// client recv buffer is a fixed 8192 so payload plus header must stay under it
constexpr size_t kFrameLimit = 0x2000;
constexpr size_t kFrameHeader = 8;

// starter grant used when vehicle templates has no enabled starter row
constexpr uint32_t kFallbackKartBaseKey = 10001;
constexpr uint32_t kFallbackExpNext = 1000;
// the reference burst of a character with only its starter grant holds no gold at all
constexpr uint32_t kStarterGold = 0;
// the two welcome gifts of the reference mailbox the gacha coins of the astro and gold tabs
constexpr uint32_t kWelcomeCoinKeys[2] = {2001, 2000};
constexpr uint32_t kGiftCategoryItem = 2;

std::string colStr(const DbRow& row, const char* key) {
    return rowStrCore(row, key);
}

uint32_t colU32(const DbRow& row, const char* key) {
    return static_cast<uint32_t>(rowUInt64Throwing(row, key, 0));
}

// db is utf eight so encode properly instead of dropping the high bytes
std::string toUtf8(const std::u16string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t cp = static_cast<uint32_t>(s[i]);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size()) {
            const uint32_t lo = static_cast<uint32_t>(s[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                ++i;
            }
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::u16string fromUtf8(const std::string& s) {
    std::u16string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const uint8_t b0 = static_cast<uint8_t>(s[i]);
        uint32_t cp = 0;
        size_t extra = 0;
        if (b0 < 0x80)                { cp = b0;         extra = 0; }
        else if ((b0 & 0xE0) == 0xC0) { cp = b0 & 0x1F;  extra = 1; }
        else if ((b0 & 0xF0) == 0xE0) { cp = b0 & 0x0F;  extra = 2; }
        else if ((b0 & 0xF8) == 0xF0) { cp = b0 & 0x07;  extra = 3; }
        else { ++i; continue; }

        if (i + extra >= s.size()) break;
        for (size_t k = 1; k <= extra; ++k) {
            cp = (cp << 6) | (static_cast<uint8_t>(s[i + k]) & 0x3F);
        }
        i += extra + 1;

        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<char16_t>(cp));
        }
    }
    return out;
}

std::string hex8(uint8_t v) {
    static const char* d = "0123456789ABCDEF";
    std::string out;
    out.push_back(d[(v >> 4) & 0xF]);
    out.push_back(d[v & 0xF]);
    return out;
}


const char* opcodeName(uint16_t op) {
    switch (op) {
        case CharCreatePackets::OP_S_MESSAGE_KEY:      return "S2C 0x0001 message key";
        case CharCreatePackets::OP_S_MESSAGE_TEXT:     return "S2C 0x0002 message text";
        case CharCreatePackets::OP_S_OPEN_CHAR_CREATE: return "S2C 0x0003 open create";
        case CharCreatePackets::OP_S_CREATE_RESULT:    return "S2C 0x0004 create result";
        case CharCreatePackets::OP_S_LOGIN_ACCEPT:     return "S2C 0x0007 login accept";
        case CharCreatePackets::OP_S_PROFILE_REFRESH:  return "S2C 0x000A profile refresh";
        case CharCreatePackets::OP_S_CHANNEL_LIST:     return "S2C 0x000E channel list";
        case CharCreatePackets::OP_S_STAGE_GARAGE:     return "S2C 0x000F stage garage";
        case CharCreatePackets::OP_S_STAGE_SHOP:       return "S2C 0x0010 stage shop";
        case CharCreatePackets::OP_S_STAGE_MENU:       return "S2C 0x0011 stage menu";
        case CharCreatePackets::OP_S_STAGE_LOBBY:      return "S2C 0x0012 stage lobby";
        case CharCreatePackets::OP_S_SERVER_REDIRECT:  return "S2C 0x0019 redirect";
        case CharCreatePackets::OP_S_STAGE_TUTORIAL:   return "S2C 0x0062 stage tutorial";
        case CharCreatePackets::OP_S_REAUTH_ACCEPT:    return "S2C 0x00A7 reauth accept";
        case CharCreatePackets::OP_S_CLEAR_CATALOGS:   return "S2C 0x00BE clear catalogs";
        default:                                       return "S2C unnamed";
    }
}

const char* targetStageName(uint32_t stage) {
    switch (stage) {
        case CharCreatePackets::TARGET_STAGE_LOGIN:    return "login";
        case CharCreatePackets::TARGET_STAGE_REDIRECT: return "redirect";
        case CharCreatePackets::TARGET_STAGE_ROOM:     return "room";
        case CharCreatePackets::TARGET_STAGE_GAME:     return "game";
        default:                                       return "forwarded";
    }
}

// one oversize frame kills the client parser so refuse instead of sending it
bool sendChecked(const Session::Ptr& session, const Packet& pkt) {
    const size_t total = pkt.payload().size() + kFrameHeader;
    if (total >= kFrameLimit) {
        LOG_ERROR(TAG, std::string(opcodeName(pkt.opcode())) + " frame " +
                       std::to_string(total) + " over the client recv limit " +
                       std::to_string(kFrameLimit) + " not sent");
        return false;
    }
    session->send(pkt);
    return true;
}

// batches whole packets into one write so the client parse loop never splits a burst
class Burst {
public:
    void add(const Packet& pkt) {
        const size_t total = pkt.payload().size() + kFrameHeader;
        if (total >= kFrameLimit) {
            LOG_ERROR(TAG, std::string(opcodeName(pkt.opcode())) + " frame " +
                           std::to_string(total) + " over the client recv limit, dropped");
            return;
        }
        auto data = pkt.serialize();
        if (!m_buf.empty() && m_buf.size() + data.size() > kFrameLimit) {
            m_chunks.push_back(std::move(m_buf));
            m_buf.clear();
        }
        m_buf.insert(m_buf.end(), data.begin(), data.end());
        ++m_count;
    }

    void flush(const Session::Ptr& session) {
        if (!m_buf.empty()) {
            m_chunks.push_back(std::move(m_buf));
            m_buf.clear();
        }
        for (auto& chunk : m_chunks) session->send(chunk);
        m_chunks.clear();
    }

    size_t count() const { return m_count; }

private:
    std::vector<std::vector<uint8_t>> m_chunks;
    std::vector<uint8_t> m_buf;
    size_t m_count = 0;
};

uint64_t nextRandom() {
    thread_local std::mt19937_64 engine{std::random_device{}()};
    return engine();
}

std::string randomToken(size_t chars) {
    static const char* d = "0123456789abcdef";
    std::string out;
    out.reserve(chars);
    while (out.size() < chars) {
        uint64_t v = nextRandom();
        for (int i = 0; i < 16 && out.size() < chars; ++i) {
            out.push_back(d[v & 0xF]);
            v >>= 4;
        }
    }
    return out;
}

// reauth token round trip closes the 0xA7 hijack hole the ip latch alone leaves open
bool issueReauthToken(uint32_t accountId, std::string& tokenOut, uint32_t& sessionIdOut) {
    tokenOut = randomToken(32);
    sessionIdOut = static_cast<uint32_t>(nextRandom() & 0x7FFFFFFFu);
    if (sessionIdOut == 0) sessionIdOut = 1;

    if (tokenOut.size() > CharCreatePackets::REAUTH_TOKEN_MAX_CHARS) {
        LOG_ERROR(TAG, "reauth token " + std::to_string(tokenOut.size()) +
                       " chars over the client fixed region cap " +
                       std::to_string(CharCreatePackets::REAUTH_TOKEN_MAX_CHARS));
        tokenOut.resize(CharCreatePackets::REAUTH_TOKEN_MAX_CHARS);
    }

    const bool ok = Database::instance().executePrepared(
        "REPLACE INTO reauth_token (account_id, session_id, token) VALUES (?, ?, ?)",
        { accountId, sessionIdOut, tokenOut });
    if (!ok) {
        LOG_ERROR(TAG, "reauth token store failed for account " + std::to_string(accountId));
    }
    return ok;
}

bool loadReauthToken(uint32_t accountId, std::string& tokenOut, uint32_t& sessionIdOut) {
    auto rows = Database::instance().queryPrepared(
        "SELECT session_id, token FROM reauth_token WHERE account_id = ? LIMIT 1",
        { accountId });
    if (rows.empty()) return false;
    sessionIdOut = colU32(rows[0], "session_id");
    tokenOut = colStr(rows[0], "token");
    return !tokenOut.empty();
}

// exp bar denominator is the cumulative exp at the START of the next level
uint32_t expNextForLevel(uint32_t level) {
    auto rows = Database::instance().queryPrepared(
        "SELECT cum_exp FROM level_curve WHERE level = ? LIMIT 1", { level + 1 });
    if (rows.empty()) {
        LOG_WARN(TAG, "level_curve has no row for level " + std::to_string(level + 1) +
                      " so the exp bar denominator falls back to " +
                      std::to_string(kFallbackExpNext));
        return kFallbackExpNext;
    }
    const uint32_t v = colU32(rows[0], "cum_exp");
    return v == 0 ? kFallbackExpNext : v;
}

uint32_t starterKartBaseKey() {
    auto rows = Database::instance().queryPrepared(
        "SELECT id FROM vehicle_templates WHERE COALESCE(is_enabled, 1) = 1 "
        "AND category = 'starter' ORDER BY required_level, id LIMIT 1", {});
    if (rows.empty()) {
        LOG_WARN(TAG, "no enabled starter kart template so the fallback base key " +
                      std::to_string(kFallbackKartBaseKey) + " is granted");
        return kFallbackKartBaseKey;
    }
    return colU32(rows[0], "id");
}

}  // namespace

bool CharCreateHandler::accountCharacterId(uint32_t accountId, uint32_t& characterIdOut) {
    characterIdOut = 0;
    if (accountId == 0) return false;

    auto rows = Database::instance().queryPrepared(
        "SELECT id FROM characters WHERE account_id = ? ORDER BY id LIMIT 1", { accountId });
    if (rows.empty()) return false;

    characterIdOut = colU32(rows[0], "id");
    return characterIdOut != 0;
}

bool CharCreateHandler::buildLoginProfile(uint32_t accountId, uint32_t characterId,
                                          CharCreatePackets::LoginProfile& out) {
    out = CharCreatePackets::LoginProfile();
    out.playerId = characterId;
    out.selectedCharacterId = CharCreatePackets::NO_SELECTION;
    out.selectedKartId = CharCreatePackets::NO_SELECTION;

    std::string token;
    uint32_t sessionId = 0;
    if (!loadReauthToken(accountId, token, sessionId)) {
        issueReauthToken(accountId, token, sessionId);
    }
    out.reauthSessionId = sessionId;
    out.reauthToken = fromUtf8(token);

    if (characterId == 0) {
        // fresh account has no nickname and no owned rows so every selection stays unset
        out.expRequiredNextLevel = expNextForLevel(1);
        return true;
    }

    auto rows = Database::instance().queryPrepared(
        "SELECT name FROM characters WHERE id = ? LIMIT 1", { characterId });
    if (rows.empty()) {
        LOG_ERROR(TAG, "login profile for missing character " + std::to_string(characterId));
        return false;
    }

    std::u16string nickname = fromUtf8(colStr(rows[0], "name"));
    if (nickname.size() > CharCreatePackets::NICKNAME_MAX_CHARS) {
        LOG_WARN(TAG, "nickname of character " + std::to_string(characterId) + " is " +
                      std::to_string(nickname.size()) +
                      " chars and would overwrite the level bytes so it is cut to " +
                      std::to_string(CharCreatePackets::NICKNAME_MAX_CHARS));
        nickname.resize(CharCreatePackets::NICKNAME_MAX_CHARS);
    }
    out.nickname = nickname;

    if (!ProgressionHandler::fillLoginProfile(characterId, out)) {
        LOG_WARN(TAG, "progression fields unavailable for character " +
                      std::to_string(characterId) +
                      " so the exp bar denominator is forced to a safe value");
        // sub 429990 divides by next minus floor so an equal pair draws a junk exp bar
        out.expFloorCurrentLevel = 0;
        out.expRequiredNextLevel = expNextForLevel(1);
    }
    if (out.expRequiredNextLevel <= out.expFloorCurrentLevel) {
        LOG_WARN(TAG, "exp next " + std::to_string(out.expRequiredNextLevel) +
                      " is not above exp floor " + std::to_string(out.expFloorCurrentLevel) +
                      " so the sub_429990 bar span would be zero, forcing floor plus one");
        out.expRequiredNextLevel = out.expFloorCurrentLevel + 1;
    }
    return true;
}

uint32_t CharCreateHandler::validateNickname(const std::u16string& nickname) {
    const uint32_t lengthVerdict = CharCreatePackets::validateNickname(nickname);
    if (lengthVerdict != CharCreatePackets::CREATE_OK) {
        LOG_INFO(TAG, "nickname rejected on length " + std::to_string(nickname.size()) +
                      " allowed " + std::to_string(CharCreatePackets::NICKNAME_MIN_CHARS) +
                      " to " + std::to_string(CharCreatePackets::NICKNAME_EDIT_MAX_CHARS));
        return lengthVerdict;
    }

    for (char16_t c : nickname) {
        // control chars would break the fixed wide slot the lobby hud draws from
        if (c < 0x20) {
            LOG_WARN(TAG, "nickname carries a control char so it is refused");
            return CharCreatePackets::CREATE_INVALID_NICK;
        }
    }

    const std::string utf8 = toUtf8(nickname);

    // the word list is def taboo of the client folded the way sub 4E15E0 folds the name
    auto banned = Database::instance().queryPrepared(
        "SELECT word FROM banned_words WHERE scope IN ('nickname','both')", {});
    std::vector<std::u16string> words;
    words.reserve(banned.size());
    for (const auto& row : banned) words.push_back(fromUtf8(colStr(row, "word")));
    if (CharCreatePackets::tabooHit(nickname, words)) {
        LOG_INFO(TAG, "nickname refused on the banned word list");
        return CharCreatePackets::CREATE_INVALID_NICK;
    }

    auto taken = Database::instance().queryPrepared(
        "SELECT id FROM characters WHERE name = ? LIMIT 1", { utf8 });
    if (!taken.empty()) {
        return CharCreatePackets::CREATE_ALREADY_REGIST;
    }

    return CharCreatePackets::CREATE_OK;
}

std::string CharCreateHandler::profileBlobHex(const CharCreatePackets::LoginProfile& profile) {
    const std::array<uint8_t, CharCreatePackets::PROFILE_BLOB_SIZE> blob =
        CharCreatePackets::profileBlob(profile);

    std::string out;
    out.reserve(blob.size() * 3);
    for (size_t i = 0; i < blob.size(); ++i) {
        if (i) out.push_back(' ');
        out += hex8(blob[i]);
    }
    return out;
}

void CharCreateHandler::sendLoginFailure(Session::Ptr session, LoginFailure reason) {
    Packet pkt;
    const char* label = "";
    switch (reason) {
        case LoginFailure::InvalidId:
            pkt = CharCreatePackets::loginFailInvalidId();
            label = CharCreatePackets::KEY_INVALID_ID;
            break;
        case LoginFailure::BadCredentials:
            pkt = CharCreatePackets::loginFailBadCredentials();
            label = CharCreatePackets::KEY_REINPUT_IDPASS;
            break;
        case LoginFailure::DbDown:
            pkt = CharCreatePackets::loginFailDbDown();
            label = CharCreatePackets::KEY_DB_ACCESS_FAIL;
            break;
        case LoginFailure::ServerNotReady:
            pkt = CharCreatePackets::loginFailServerNotReady();
            label = CharCreatePackets::KEY_SERVER_NOT_READY;
            break;
        case LoginFailure::Banned:
            pkt = CharCreatePackets::loginFailBanned();
            label = CharCreatePackets::KEY_BLOCK_USER;
            break;
        case LoginFailure::Duplicate:
            pkt = CharCreatePackets::loginFailDuplicate();
            label = CharCreatePackets::KEY_USED_ID;
            break;
        case LoginFailure::Kicked:
            pkt = CharCreatePackets::loginFailKicked();
            label = CharCreatePackets::KEY_USER_BAN;
            break;
        case LoginFailure::GatewayFull:
            pkt = CharCreatePackets::loginFailGatewayFull();
            label = CharCreatePackets::KEY_GATEWAY_FULL;
            break;
        case LoginFailure::Version:
            pkt = CharCreatePackets::loginFailVersion();
            label = CharCreatePackets::KEY_INVALID_VERSION;
            break;
    }

    LOG_INFO(TAG, std::string("login refused with ") + label + " for account " +
                  std::to_string(session->accountId));
    sendChecked(session, pkt);
}

void CharCreateHandler::sendNotice(Session::Ptr session, const std::u16string& text,
                                   uint32_t boxType) {
    uint32_t type = boxType;
    if (type == CharCreatePackets::BOX_TYPE_WAIT_NO_BUTTON) {
        // sub 463DA0 draws no button for type zero and self closes after twenty seconds
        LOG_ERROR(TAG, "notice asked for the buttonless box type, forcing the OK type");
        type = CharCreatePackets::BOX_TYPE_OK;
    }
    if (type != CharCreatePackets::BOX_TYPE_OK && type != CharCreatePackets::BOX_TYPE_FATAL) {
        // sub 464030 only tells 0 from 2 apart so anything else already behaves like OK
        LOG_WARN(TAG, "notice box type " + std::to_string(type) +
                      " is not one of the three sub_464030 tells apart, forcing the OK type");
        type = CharCreatePackets::BOX_TYPE_OK;
    }

    std::u16string body = text;
    if (body.size() > 159) {
        // destination is a wchar stack array of 160 so a longer text smashes the frame
        LOG_WARN(TAG, "notice text " + std::to_string(body.size()) + " chars cut to 159");
        body.resize(159);
    }
    sendChecked(session, CharCreatePackets::messageTextBox(body, type));
}

void CharCreateHandler::sendLoginAccept(Session::Ptr session, uint32_t characterId) {
    CharCreatePackets::LoginProfile profile;
    if (!buildLoginProfile(session->accountId, characterId, profile)) {
        sendLoginFailure(session, LoginFailure::DbDown);
        return;
    }

    Packet pkt = CharCreatePackets::loginAccept(profile);
    if (pkt.payload().size() != CharCreatePackets::PROFILE_BLOB_SIZE) {
        LOG_ERROR(TAG, "login accept blob " + std::to_string(pkt.payload().size()) +
                       " expected " + std::to_string(CharCreatePackets::PROFILE_BLOB_SIZE) +
                       " bytes [" + profileBlobHex(profile) + "]");
    }
    if (sendChecked(session, pkt)) {
        LOG_INFO(TAG, "login accept sent for character " + std::to_string(characterId) +
                      " account " + std::to_string(session->accountId));
    }
}

void CharCreateHandler::sendReauthAccept(Session::Ptr session, uint32_t characterId) {
    CharCreatePackets::LoginProfile profile;
    if (!buildLoginProfile(session->accountId, characterId, profile)) {
        sendLoginFailure(session, LoginFailure::DbDown);
        return;
    }

    Packet pkt = CharCreatePackets::reauthAccept(profile);
    if (pkt.payload().size() != CharCreatePackets::PROFILE_BLOB_SIZE) {
        LOG_ERROR(TAG, "reauth accept blob " + std::to_string(pkt.payload().size()) +
                       " expected " + std::to_string(CharCreatePackets::PROFILE_BLOB_SIZE) +
                       " bytes [" + profileBlobHex(profile) + "]");
    }
    sendChecked(session, pkt);
}

void CharCreateHandler::sendProfileRefresh(Session::Ptr session, uint32_t characterId) {
    CharCreatePackets::LoginProfile profile;
    if (!buildLoginProfile(session->accountId, characterId, profile)) return;

    Packet pkt = CharCreatePackets::profileRefresh(profile);
    if (pkt.payload().size() != CharCreatePackets::PROFILE_REFRESH_SIZE) {
        LOG_ERROR(TAG, "profile refresh " + std::to_string(pkt.payload().size()) +
                       " expected " + std::to_string(CharCreatePackets::PROFILE_REFRESH_SIZE));
    }
    sendChecked(session, pkt);
}

size_t CharCreateHandler::sendDriverCatalog(Session::Ptr session) {
    auto rows = Database::instance().queryPrepared(
        "SELECT id, name FROM drivers WHERE COALESCE(is_enabled, 1) = 1 ORDER BY id", {});

    Burst burst;
    // 0xBE wipes every catalog so it must lead the burst and never trail it
    burst.add(CharCreatePackets::clearCatalogs());

    size_t sent = 0;
    for (const auto& row : rows) {
        const uint32_t id = colU32(row, "id");
        if (id == 0) {
            // record offset 0x00 and offset 0x04 zero makes the create screen rand loop skip the row
            LOG_ERROR(TAG, "driver row with id zero skipped, the create screen cannot pick it");
            continue;
        }
        burst.add(PacketBuilder::driverCatalog(static_cast<int32_t>(id),
                                               driverBodyAsset(colStr(row, "name"))));
        ++sent;
    }

    burst.flush(session);
    LOG_INFO(TAG, "driver catalog published " + std::to_string(sent) + " rows");
    return sent;
}

void CharCreateHandler::openCreateScreen(Session::Ptr session, GameServer* server) {
    (void)server;

    const size_t drivers = sendDriverCatalog(session);
    if (drivers == 0) {
        // sub 473730 divides by the entry count so an empty catalog is a hard crash
        LOG_ERROR(TAG, "driver catalog is empty so S2C 0x0003 is refused, it would divide by zero");
        sendLoginFailure(session, LoginFailure::ServerNotReady);
        return;
    }

    Burst burst;
    burst.add(CharCreatePackets::stageMenu());
    burst.add(CharCreatePackets::openCharacterCreate());
    burst.flush(session);

    session->handshakeState = Session::HandshakeState::AwaitingCharacterCreation;
    LOG_INFO(TAG, "RegistDriver popup opened for account " +
                  std::to_string(session->accountId) + " with " +
                  std::to_string(drivers) + " drivers");
}

void CharCreateHandler::sendChannelList(Session::Ptr session) {
    auto rows = Database::instance().queryPrepared(
        "SELECT id, name, population, capacity, tier FROM channels "
        "ORDER BY sort_order, id LIMIT 80", {});

    std::vector<CharCreatePackets::ChannelEntry> entries;
    entries.reserve(rows.size());
    for (const auto& row : rows) {
        CharCreatePackets::ChannelEntry e;
        e.id = colU32(row, "id");
        e.name = fromUtf8(colStr(row, "name"));
        e.population = colU32(row, "population");
        e.capacity = colU32(row, "capacity");
        e.tier = colU32(row, "tier");
        if (e.name.size() > CharCreatePackets::CHANNEL_NAME_MAX_CHARS) {
            LOG_WARN(TAG, "channel " + std::to_string(e.id) + " name cut to " +
                          std::to_string(CharCreatePackets::CHANNEL_NAME_MAX_CHARS));
            e.name.resize(CharCreatePackets::CHANNEL_NAME_MAX_CHARS);
        }
        if (e.capacity == 0) {
            LOG_WARN(TAG, "channel " + std::to_string(e.id) +
                          " has capacity zero so the gauge divides by zero, forcing one");
            e.capacity = 1;
        }
        entries.push_back(std::move(e));
    }

    if (entries.size() > CharCreatePackets::CHANNEL_LIST_MAX) {
        // entry 81 makes the client raise a type two box and close the socket
        LOG_ERROR(TAG, "channel list " + std::to_string(entries.size()) + " over cap " +
                       std::to_string(CharCreatePackets::CHANNEL_LIST_MAX));
        entries.resize(CharCreatePackets::CHANNEL_LIST_MAX);
    }

    std::u16string notice;
    auto noticeRows = Database::instance().queryPrepared(
        "SELECT notice FROM server_notice WHERE id = 1 LIMIT 1", {});
    if (!noticeRows.empty()) notice = fromUtf8(colStr(noticeRows[0], "notice"));

    Packet pkt = CharCreatePackets::channelList(entries, notice);
    if (!sendChecked(session, pkt)) return;

    session->handshakeState = Session::HandshakeState::ChannelListSent;
    LOG_INFO(TAG, "channel list sent " + std::to_string(entries.size()) + " rows notice " +
                  std::to_string(notice.size()) + " chars");
}

void CharCreateHandler::sendStage(Session::Ptr session, uint32_t requestedStage) {
    switch (requestedStage) {
        case CharCreatePackets::REQ_STAGE_GARAGE:
            sendChecked(session, CharCreatePackets::stageGarage());
            break;
        case CharCreatePackets::REQ_STAGE_SHOP:
            sendChecked(session, CharCreatePackets::stageShop());
            break;
        case CharCreatePackets::REQ_STAGE_LOBBY:
            sendChecked(session, CharCreatePackets::stageLobby());
            break;
        case CharCreatePackets::REQ_STAGE_TUTORIAL_MENU:
            sendChecked(session, CharCreatePackets::stageTutorial());
            break;
        default:
            // S2C 0x0011 raises stage 5 but no known caller of sub 4806F0 ever requests that stage
            LOG_WARN(TAG, "requested stage " + std::to_string(requestedStage) +
                          " is outside the sub_4806F0 literal set so the lobby is sent instead");
            sendChecked(session, CharCreatePackets::stageLobby());
            break;
    }
}

void CharCreateHandler::sendRedirect(Session::Ptr session, const std::string& host,
                                     uint32_t port, uint32_t mode) {
    if (host.size() >= 128) {
        // host lands in a 128 byte stack buffer so a longer one smashes the frame
        LOG_ERROR(TAG, "redirect host " + std::to_string(host.size()) +
                       " chars over the client stack buffer, redirect refused");
        return;
    }
    if (mode == CharCreatePackets::TARGET_STAGE_LOGIN) {
        LOG_WARN(TAG, "redirect mode 4 makes the client ignore port " + std::to_string(port) +
                      " and use its Network2 ini port instead");
    }

    if (sendChecked(session, CharCreatePackets::serverRedirect(host, port, mode))) {
        session->handshakeState = Session::HandshakeState::Redirected;
        LOG_INFO(TAG, "redirect to " + host + " port " + std::to_string(port) +
                      " mode " + std::to_string(mode) + " " + targetStageName(mode));
    }
}

bool CharCreateHandler::handleLogin(Session::Ptr session, Packet& packet, GameServer* server) {
    const CharCreatePackets::LoginRequest req = CharCreatePackets::parseLogin(packet);
    if (!req.ok) {
        LOG_ERROR(TAG, "C2S 0x0007 malformed from " + session->remoteAddress());
        sendLoginFailure(session, LoginFailure::BadCredentials);
        return false;
    }

    if (req.accountId.size() > CharCreatePackets::ACCOUNT_FIELD_MAX_CHARS ||
        req.password.size() > CharCreatePackets::ACCOUNT_FIELD_MAX_CHARS) {
        LOG_WARN(TAG, "C2S 0x0007 account field over the client edit cap so the frame is forged");
    }

    LOG_INFO(TAG, "C2S 0x0007 version '" + req.clientVersion + "' target stage " +
                  std::to_string(req.targetStage) + " " + targetStageName(req.targetStage) +
                  " account '" + toUtf8(req.accountId) + "' from " + session->remoteAddress());

    if (!CharCreatePackets::isSupportedVersion(req.clientVersion)) {
        LOG_ERROR(TAG, "client build '" + req.clientVersion + "' expected '" +
                       std::string(CharCreatePackets::CLIENT_VERSION) + "'");
        sendLoginFailure(session, LoginFailure::Version);
        return false;
    }

    if (session->accountId == 0) {
        // gateway latches the account before this packet so a zero here means no session row
        LOG_ERROR(TAG, "C2S 0x0007 with no latched account from " + session->remoteAddress());
        sendLoginFailure(session, LoginFailure::InvalidId);
        return false;
    }

    uint32_t characterId = 0;
    if (accountCharacterId(session->accountId, characterId)) {
        session->characterId = characterId;
        session->handshakeState = Session::HandshakeState::AwaitingAuth;
        return true;
    }

    sendLoginAccept(session, 0);
    openCreateScreen(session, server);
    return false;
}

bool CharCreateHandler::handleReauth(Session::Ptr session, Packet& packet, GameServer* server) {
    const CharCreatePackets::ReauthRequest req = CharCreatePackets::parseReauth(packet);
    if (!req.ok) {
        LOG_ERROR(TAG, "C2S 0x00A7 malformed from " + session->remoteAddress());
        sendLoginFailure(session, LoginFailure::BadCredentials);
        return false;
    }

    LOG_INFO(TAG, "C2S 0x00A7 version '" + req.clientVersion + "' target stage " +
                  std::to_string(req.targetStage) + " " + targetStageName(req.targetStage) +
                  " session id " + std::to_string(req.reauthSessionId));

    if (!CharCreatePackets::isSupportedVersion(req.clientVersion)) {
        sendLoginFailure(session, LoginFailure::Version);
        return false;
    }

    if (session->accountId == 0) {
        LOG_ERROR(TAG, "C2S 0x00A7 with no latched account from " + session->remoteAddress());
        sendLoginFailure(session, LoginFailure::InvalidId);
        return false;
    }

    std::string storedToken;
    uint32_t storedSession = 0;
    if (!loadReauthToken(session->accountId, storedToken, storedSession)) {
        // no stored token means the 0x0007 leg never ran on this server
        LOG_WARN(TAG, "no reauth token stored for account " +
                      std::to_string(session->accountId) + " so the echo cannot be verified");
    } else {
        const std::string echoed = toUtf8(req.reauthToken);
        if (echoed != storedToken || req.reauthSessionId != storedSession) {
            LOG_ERROR(TAG, "reauth echo mismatch for account " +
                           std::to_string(session->accountId) + " session id " +
                           std::to_string(req.reauthSessionId) + " expected " +
                           std::to_string(storedSession));
            sendLoginFailure(session, LoginFailure::BadCredentials);
            return false;
        }
        session->sessionToken = storedToken;
        session->launcherAuthenticated = true;
    }

    uint32_t characterId = 0;
    if (accountCharacterId(session->accountId, characterId)) {
        session->characterId = characterId;
        return true;
    }

    sendReauthAccept(session, 0);
    openCreateScreen(session, server);
    return false;
}

void CharCreateHandler::handleCreateCharacter(Session::Ptr session, Packet& packet,
                                              GameServer* server) {

    const CharCreatePackets::CreateCharacterRequest req =
        CharCreatePackets::parseCreateCharacter(packet);
    if (!req.ok) {
        LOG_ERROR(TAG, "C2S 0x0004 malformed from " + session->remoteAddress());
        sendChecked(session, CharCreatePackets::createCharacterReject(
                                 CharCreatePackets::CREATE_INVALID_NICK_ALT));
        return;
    }

    LOG_INFO(TAG, "C2S 0x0004 driver key " + std::to_string(req.driverKey) + " nickname '" +
                  toUtf8(req.nickname) + "' account " + std::to_string(session->accountId));

    if (session->accountId == 0) {
        LOG_ERROR(TAG, "C2S 0x0004 with no latched account from " + session->remoteAddress());
        sendChecked(session, CharCreatePackets::createCharacterReject(
                                 CharCreatePackets::CREATE_INVALID_NICK_ALT));
        return;
    }

    uint32_t existing = 0;
    if (accountCharacterId(session->accountId, existing)) {
        LOG_WARN(TAG, "account " + std::to_string(session->accountId) +
                      " already owns character " + std::to_string(existing));
        sendChecked(session, CharCreatePackets::createCharacterReject(
                                 CharCreatePackets::CREATE_ALREADY_REGIST));
        return;
    }

    auto driverRows = Database::instance().queryPrepared(
        "SELECT id FROM drivers WHERE id = ? AND COALESCE(is_enabled, 1) = 1 "
        "AND COALESCE(creation_pick, 0) = 1 LIMIT 1",
        { req.driverKey });
    if (driverRows.empty()) {
        // sub 479230 knows only 1 2 3 any other kills the socket sub 473950 gates the send on its catalog
        LOG_WARN(TAG, "driver key " + std::to_string(req.driverKey) +
                      " is not a drivers table row so creation is refused");
        sendChecked(session, CharCreatePackets::createCharacterReject(
                                 CharCreatePackets::CREATE_INVALID_NICK_ALT));
        return;
    }

    const uint32_t verdict = validateNickname(req.nickname);
    if (verdict != CharCreatePackets::CREATE_OK) {
        // popup reopens by itself in stages two three and five so never resend 0x0003
        sendChecked(session, CharCreatePackets::createCharacterReject(verdict));
        return;
    }

    const std::string nameUtf8 = toUtf8(req.nickname);
    const uint32_t kartBaseKey = starterKartBaseKey();
    const uint32_t expNext = expNextForLevel(1);

    Transaction tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR(TAG, "character creation cannot open a transaction");
        sendChecked(session, CharCreatePackets::createCharacterReject(
                                 CharCreatePackets::CREATE_INVALID_NICK_ALT));
        return;
    }

    if (!tx.execute(
            "INSERT INTO characters (account_id, name, level, experience, gold, cash, "
            "equipped_driver_id, driver_base_key, exp_floor, exp_next) "
            "VALUES (?, ?, 1, 0, ?, 0, ?, ?, 0, ?)",
            { session->accountId, nameUtf8, kStarterGold, req.driverKey, req.driverKey, expNext })) {
        LOG_ERROR(TAG, "character insert failed for account " + std::to_string(session->accountId));
        tx.rollback();
        sendChecked(session, CharCreatePackets::createCharacterReject(
                                 CharCreatePackets::CREATE_ALREADY_REGIST));
        return;
    }
    const uint32_t characterId = static_cast<uint32_t>(tx.lastInsertId());

    bool ok = true;
    ok = tx.execute(
        "INSERT INTO player_drivers (character_id, driver_id, is_equipped) VALUES (?, ?, 1)",
        { characterId, req.driverKey }) && ok;

    ok = tx.execute(
        "INSERT INTO owned_character (character_id, base_key, period_mode, period_value, "
        "active_flag) VALUES (?, ?, 0, 0, 1)",
        { characterId, req.driverKey }) && ok;
    const uint32_t charInstanceId = static_cast<uint32_t>(tx.lastInsertId());

    // the body face and head keys go in now so the 0x0004 record is the one 0x1B publishes later
    ok = tx.execute(
        "UPDATE owned_character oc JOIN drivers d ON d.id = oc.base_key SET "
        "oc.acc_body = COALESCE((SELECT MIN(s.skin_key) FROM def_kart_skin s WHERE s.category = 2 "
        "AND s.name LIKE CONCAT(d.name, '\\_char\\_body\\_%')), 0), "
        "oc.acc_face = COALESCE((SELECT MIN(s.skin_key) FROM def_kart_skin s WHERE s.category = 2 "
        "AND s.name LIKE CONCAT(d.name, '\\_char\\_face\\_%')), 0), "
        "oc.acc_head = COALESCE((SELECT MIN(s.skin_key) FROM def_kart_skin s WHERE s.category = 2 "
        "AND s.name LIKE CONCAT(d.name, '\\_char\\_head\\_%')), 0) "
        "WHERE oc.id = ? AND oc.character_id = ?",
        { charInstanceId, characterId }) && ok;

    ok = tx.execute(
        // expiry kind 0 means the starter kart never wears out matching chibikart
        "INSERT INTO owned_kart (character_id, base_key, period_mode, period_value, "
        "active_flag, skin_primary, skin_secondary) VALUES (?, ?, 0, 0, 1, ?, ?)",
        { characterId, kartBaseKey, kDefaultPaintKey, kDefaultPlateKey }) && ok;
    const uint32_t kartInstanceId = static_cast<uint32_t>(tx.lastInsertId());

    // the starter kart needs no stat block its stats come from the catalogue row it names
    ok = tx.execute(
        "UPDATE characters SET selected_kart_instance_id = ? WHERE id = ?",
        { kartInstanceId, characterId }) && ok;

    if (!ok || charInstanceId == 0 || kartInstanceId == 0) {
        LOG_ERROR(TAG, "character creation rows incomplete for account " +
                       std::to_string(session->accountId) + " so everything rolls back");
        tx.rollback();
        sendChecked(session, CharCreatePackets::createCharacterReject(
                                 CharCreatePackets::CREATE_INVALID_NICK_ALT));
        return;
    }

    if (!tx.commit()) {
        LOG_ERROR(TAG, "character creation commit failed for account " +
                       std::to_string(session->accountId));
        sendChecked(session, CharCreatePackets::createCharacterReject(
                                 CharCreatePackets::CREATE_INVALID_NICK_ALT));
        return;
    }

    // the two records are read back from the rows so 0x0004 is byte identical to 0x1B and 0x1C
    InventoryPackets::CharacterRow charRow;
    if (!InventoryHandler::selectedCharacterRow(static_cast<int32_t>(characterId), charRow)) {
        charRow.instanceId = charInstanceId;
        charRow.baseKey = req.driverKey;
        charRow.periodMode = InventoryPackets::PERIOD_PERMANENT;
        charRow.activeFlag = 1;
    }
    InventoryPackets::KartRow kartRow;
    if (!InventoryHandler::selectedKartRow(static_cast<int32_t>(characterId), kartRow)) {
        kartRow.instanceId = kartInstanceId;
        kartRow.baseKey = kartBaseKey;
        kartRow.periodMode = InventoryPackets::PERIOD_PERMANENT;
        kartRow.activeFlag = 1;
    }

    const std::array<uint8_t, CharCreatePackets::CHARACTER_RECORD_SIZE> charBlob =
        InventoryPackets::characterBlob(charRow);
    const std::array<uint8_t, CharCreatePackets::KART_RECORD_SIZE> kartBlob =
        InventoryPackets::kartBlob(kartRow);

    Packet success = CharCreatePackets::createCharacterSuccess(req.nickname, charBlob, kartBlob);
    const size_t expected = 104 + 2 * (req.nickname.size() + 1);
    if (success.payload().size() != expected) {
        LOG_ERROR(TAG, "create success " + std::to_string(success.payload().size()) +
                       " expected " + std::to_string(expected));
    }
    if (!sendChecked(session, success)) return;

    session->characterId = characterId;
    session->characterName = req.nickname;

    // the welcome mail of the reference burst two gacha coins from the house in the mailbox
    for (uint32_t coin : kWelcomeCoinKeys) {
        std::vector<uint8_t> record;
        if (!ShopHandler::grantReward(characterId, kGiftCategoryItem, coin, record)) continue;
        Database::instance().executePrepared(
            "INSERT INTO gift_log (sender_id, target_name, target_id, category, base_key, price_key, message) "
            "VALUES (0, ?, ?, ?, ?, -1, ?)",
            { nameUtf8, characterId, kGiftCategoryItem, coin, std::string("Welcome to KnC") });
    }
    LOG_INFO(TAG, "character " + std::to_string(characterId) + " '" + nameUtf8 +
                  "' created for account " + std::to_string(session->accountId) +
                  " driver " + std::to_string(req.driverKey) + " kart " +
                  std::to_string(kartBaseKey));

    // sendPlayerData sends the player phase 0x07 0x1B 0x1C the tail then the 0x16 stage ack band 0 is the licence
    if (server) server->sendPlayerData(session);
    else sendChannelList(session);
}

void CharCreateHandler::handleChannelSelect(Session::Ptr session, Packet& packet,
                                            GameServer* server) {
    (void)server;

    const CharCreatePackets::ChannelSelectRequest req =
        CharCreatePackets::parseChannelSelect(packet);
    if (!req.ok) {
        LOG_ERROR(TAG, "C2S 0x0018 malformed from " + session->remoteAddress());
        return;
    }

    LOG_INFO(TAG, "C2S 0x0018 requested stage " + std::to_string(req.requestedStage) +
                  " channel " + std::to_string(req.channelId));

    if (req.channelId >= 0) {
        auto rows = Database::instance().queryPrepared(
            "SELECT name, population, capacity FROM channels WHERE id = ? LIMIT 1",
            { static_cast<uint32_t>(req.channelId) });
        if (rows.empty()) {
            LOG_WARN(TAG, "channel " + std::to_string(req.channelId) + " is unknown");
        } else {
            const uint32_t population = colU32(rows[0], "population");
            const uint32_t capacity = colU32(rows[0], "capacity");
            if (capacity != 0 && population >= capacity) {
                std::u16string text = fromUtf8("Channel " + colStr(rows[0], "name") + " is full");
                sendNotice(session, text, CharCreatePackets::BOX_TYPE_OK);
                return;
            }
        }
    }

    sendStage(session, req.requestedStage);
}

} // namespace knc
