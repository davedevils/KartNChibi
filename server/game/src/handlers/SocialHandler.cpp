// social handlers chat whisper small talk friends blocks presence notes profiles

#include "handlers/SocialHandler.h"
#include "handlers/InventoryHandler.h"
#include "handlers/ProgressionHandler.h"
#include "packets/PacketBuilder.h"
#include "packets/gen/ShopPackets.h"
#include "GameServer.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DevCommands.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <chrono>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace knc {

namespace {

using Row = std::map<std::string, std::string>;

// login widens the db name one byte per char so mirror that both ways
std::string narrow(const std::u16string& s) {
    std::string out;
    out.reserve(s.size());
    for (char16_t c : s) {
        if (c == 0) break;
        out.push_back(static_cast<char>(c & 0xFF));
    }
    return out;
}

std::u16string widen(const std::string& s) {
    std::u16string out;
    out.reserve(s.size());
    for (char c : s) out.push_back(static_cast<char16_t>(static_cast<uint8_t>(c)));
    return out;
}

std::string logName(const std::u16string& s) {
    std::string out;
    for (char16_t c : s) {
        if (c > 0 && c < 128) out.push_back(static_cast<char>(c));
    }
    return out;
}

std::string opHex(uint16_t op) {
    static const char* digits = "0123456789ABCDEF";
    std::string out = "0x";
    out.push_back(digits[(op >> 12) & 0xF]);
    out.push_back(digits[(op >> 8) & 0xF]);
    out.push_back(digits[(op >> 4) & 0xF]);
    out.push_back(digits[op & 0xF]);
    return out;
}

uint32_t colU32(const Row& row, const char* key) {
    auto it = row.find(key);
    if (it == row.end() || it->second.empty()) return 0;
    return static_cast<uint32_t>(std::strtoul(it->second.c_str(), nullptr, 10));
}

int32_t colI32(const Row& row, const char* key) {
    auto it = row.find(key);
    if (it == row.end() || it->second.empty()) return 0;
    return static_cast<int32_t>(std::strtol(it->second.c_str(), nullptr, 10));
}

std::string colStr(const Row& row, const char* key) {
    auto it = row.find(key);
    if (it == row.end()) return std::string();
    return it->second;
}

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// sub 4E1B70 localisation keys the client resolves these six ship in the Eng table with no string in the exe
const char* const KEY_NO_SUCH_USER   = "MSG_NOT_FIND_USER";
const char* const KEY_NO_WHISPER     = "MSG_NOT_AVAILABLE_WHISPER";
const char* const KEY_REJECT_TALK    = "MSG_REJECT_SMALLTALK";
const char* const KEY_REJECT_POST    = "MSG_REJECT_POSTCARD";
const char* const KEY_REJECT_GAME    = "MSG_REJECT_GAME";
const char* const KEY_REJECT_ROOM    = "MSG_REJECT_SAME_ROOM";

const char* const KEY_ANTI_CHAT      = "MSG_ANTI_CHAT";

// sub 480990 server copy of the client flood guard fifth line inside two seconds still ships then silence
constexpr size_t  FLOOD_WINDOW_SENDS = 5;
constexpr int64_t FLOOD_WINDOW_MS    = 2000;
constexpr int64_t FLOOD_MUTE_MS      = 60000;

// sub 4E15E0 word filter mirror the client drops every char of this set before matching so evasion must fail too
bool isFilterSeparator(char16_t c) {
    static const std::u16string kSet = u" ~`!@#$%^&*()_-+=\\|{}[]:;\"'<,>.?/";
    return kSet.find(c) != std::u16string::npos;
}

char16_t lowerAscii(char16_t c) {
    if (c >= u'A' && c <= u'Z') return static_cast<char16_t>(c + 0x20);
    return c;
}

const std::vector<std::u16string>& bannedChatWords() {
    static std::mutex mtx;
    static std::vector<std::u16string> words;
    static bool loaded = false;

    std::lock_guard<std::mutex> lock(mtx);
    if (loaded) return words;
    loaded = true;

    auto rows = Database::instance().queryPrepared(
        "SELECT word FROM banned_words WHERE scope IN ('chat','both')", {});

    size_t skipped = 0;
    for (const Row& r : rows) {
        const std::string raw = colStr(r, "word");
        bool ascii = true;
        std::u16string needle;
        for (char c : raw) {
            const uint8_t b = static_cast<uint8_t>(c);
            if (b >= 0x80) { ascii = false; break; }
            const char16_t u = static_cast<char16_t>(b);
            if (isFilterSeparator(u)) continue;
            needle.push_back(lowerAscii(u));
        }
        // utf8 multi byte words cannot survive the one byte widen used here
        if (!ascii || needle.empty()) { ++skipped; continue; }
        words.push_back(needle);
    }

    if (words.empty()) {
        LOG_WARN("SOCIAL", "banned_words has no chat scope rows so server side masking is off");
    } else {
        LOG_INFO("SOCIAL", "Chat word filter loaded " + std::to_string(words.size()) +
                           " needles skipped " + std::to_string(skipped));
    }
    return words;
}

bool maskBannedWords(std::u16string& text) {
    const std::vector<std::u16string>& words = bannedChatWords();
    if (words.empty() || text.empty()) return false;

    std::u16string squeezed;
    std::vector<size_t> origin;
    squeezed.reserve(text.size());
    origin.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (isFilterSeparator(text[i])) continue;
        squeezed.push_back(lowerAscii(text[i]));
        origin.push_back(i);
    }
    if (squeezed.empty()) return false;

    bool hit = false;
    for (const std::u16string& w : words) {
        size_t at = squeezed.find(w);
        while (at != std::u16string::npos) {
            for (size_t k = 0; k < w.size(); ++k) squeezed[at + k] = u'*';
            hit = true;
            at = squeezed.find(w, at + w.size());
        }
    }
    if (!hit) return false;

    for (size_t i = 0; i < squeezed.size(); ++i) {
        if (squeezed[i] == u'*') text[origin[i]] = u'*';
    }
    return true;
}

// name rides the wire but sub 47EC00 formats the resolved key alone
void sendSystemKey(const Session::Ptr& s, const std::u16string& name, const char* key) {
    s->send(SocialPackets::systemChatLine(name, key));
}

// the dead dialog frames render nothing so keep them out of production
bool deadDialogProbeEnabled() {
    const char* v = std::getenv("KNC_SOCIAL_DEAD_DIALOG_PROBE");
    return v != nullptr && v[0] == '1';
}

struct CharBrief {
    bool           found = false;
    uint32_t       id    = 0;
    std::u16string name;
    uint32_t       level = 0;
};

CharBrief briefFromRow(const Row& r) {
    CharBrief b;
    b.found = true;
    b.id    = colU32(r, "id");
    b.name  = widen(colStr(r, "name"));
    // zero based icon index see the level note on loadProfile
    b.level = colU32(r, "level") > 0 ? colU32(r, "level") - 1 : 0;
    return b;
}

// note inbox rows player notes joined to the sender name

const char* const NOTE_SELECT =
    "SELECT n.id, COALESCE(c.name, '') AS sender, "
    "DATE_FORMAT(n.created_at, '%Y-%m-%d') AS d, DATE_FORMAT(n.created_at, '%H:%i:%s') AS t, "
    "n.is_read, n.body FROM player_notes n LEFT JOIN characters c ON c.id = n.from_id ";

SocialPackets::NoteRow noteFromRow(const Row& r) {
    SocialPackets::NoteRow n;
    n.id     = colU32(r, "id");
    n.sender = widen(colStr(r, "sender"));
    n.date   = widen(colStr(r, "d"));
    n.time   = widen(colStr(r, "t"));
    n.read   = colU32(r, "is_read") != 0;
    n.body   = widen(colStr(r, "body"));
    return n;
}

bool loadNoteRow(uint32_t noteId, SocialPackets::NoteRow& out) {
    if (noteId == 0) return false;
    auto rows = Database::instance().queryPrepared(std::string(NOTE_SELECT) + "WHERE n.id = ?", {noteId});
    if (rows.empty()) return false;
    out = noteFromRow(rows[0]);
    return true;
}

CharBrief lookupByName(const std::u16string& name) {
    CharBrief b;
    // sub 447060 cuts the box text by char count so twelve is the clients own cap
    if (name.empty() || name.size() > SocialPackets::MAX_NICKNAME_CHARS) return b;

    auto rows = Database::instance().queryPrepared(
        "SELECT id, name, level FROM characters WHERE name = ?",
        {narrow(name)});
    if (rows.empty()) return b;
    return briefFromRow(rows[0]);
}

CharBrief lookupById(uint32_t id) {
    CharBrief b;
    if (id == 0) return b;

    auto rows = Database::instance().queryPrepared(
        "SELECT id, name, level FROM characters WHERE id = ?",
        {id});
    if (rows.empty()) return b;
    return briefFromRow(rows[0]);
}

bool hasBlocked(uint32_t owner, uint32_t other) {
    if (owner == 0 || other == 0) return false;
    auto rows = Database::instance().queryPrepared(
        "SELECT 1 AS hit FROM blocked_players WHERE character_id = ? AND blocked_id = ?",
        {owner, other});
    return !rows.empty();
}

bool areFriends(uint32_t a, uint32_t b) {
    auto rows = Database::instance().queryPrepared(
        "SELECT 1 AS hit FROM friends WHERE character_id = ? AND friend_id = ?",
        {a, b});
    return !rows.empty();
}

size_t countRows(const char* sql, uint32_t id) {
    auto rows = Database::instance().queryPrepared(sql, {id});
    if (rows.empty()) return 0;
    return static_cast<size_t>(colU32(rows[0], "n"));
}

size_t countFriends(uint32_t id) {
    return countRows("SELECT COUNT(*) AS n FROM friends WHERE character_id = ?", id);
}

size_t countPendingFor(uint32_t targetId) {
    return countRows(
        "SELECT COUNT(*) AS n FROM friend_requests WHERE target_id = ? AND state = 0",
        targetId);
}

size_t countBlocks(uint32_t id) {
    return countRows("SELECT COUNT(*) AS n FROM blocked_players WHERE character_id = ?", id);
}

bool pendingExists(uint32_t requesterId, uint32_t targetId) {
    auto rows = Database::instance().queryPrepared(
        "SELECT 1 AS hit FROM friend_requests "
        "WHERE requester_id = ? AND target_id = ? AND state = 0",
        {requesterId, targetId});
    return !rows.empty();
}

Session::Ptr onlineSession(GameServer* srv, uint32_t characterId) {
    if (!srv || characterId == 0) return nullptr;
    return srv->findSessionByCharacterId(static_cast<int32_t>(characterId));
}

Session::Ptr onlineByName(GameServer* srv, const std::u16string& name) {
    if (!srv || name.empty()) return nullptr;
    for (const auto& sess : srv->getSessions()) {
        if (sess && sess->characterId != 0 && sess->characterName == name) return sess;
    }
    return nullptr;
}

// deliberate server policy nothing in the client reads statusB so the encoding is ours to pick
int32_t activityCode(GameServer* srv, const Session::Ptr& sess) {
    if (!srv || !sess || sess->roomId == 0) return 0;
    auto room = srv->getRoom(sess->roomId);
    if (!room) return 0;
    return room->isPlaying() ? 2 : 1;
}

SocialPackets::FriendRow makeFriendRow(const CharBrief& c, bool online) {
    SocialPackets::FriendRow r;
    r.playerId = c.id;
    r.name     = c.name;
    r.level    = c.level;
    // negative greys the row out and minus two is the offline reset value
    r.statusA = online ? 0 : -2;
    r.statusB = online ? 0 : -2;
    return r;
}

SocialPackets::RequestRow makeRequestRow(const CharBrief& c) {
    SocialPackets::RequestRow r;
    r.requesterId = c.id;
    r.name        = c.name;
    r.level       = c.level;
    return r;
}

// sub 45A4A0 profile blob for the User Info panel four numbers games victory percent and quit percent
bool loadProfile(uint32_t characterId, SocialPackets::Profile& out) {
    auto& db = Database::instance();

    auto rows = db.queryPrepared(
        "SELECT id, name, level, experience, character_key, pendant_key "
        "FROM characters WHERE id = ?",
        {characterId});
    if (rows.empty()) return false;

    const Row& r = rows[0];
    out.playerId = colU32(r, "id");
    out.name     = widen(colStr(r, "name"));

    const int32_t level = std::max(1, colI32(r, "level"));

    out.level = static_cast<int8_t>(std::min(126, level - 1));

    // exp floor and exp next go stale once anything writes the level so derive both from the curve every time
    int32_t floorExp = 0;
    int32_t nextExp = 0;
    {
        auto cur = db.queryPrepared(
            "SELECT cum_exp FROM level_curve WHERE level = ? LIMIT 1", {level});
        if (!cur.empty()) floorExp = std::stoi(cur[0].at("cum_exp"));
        auto nxt = db.queryPrepared(
            "SELECT cum_exp FROM level_curve WHERE level = ? LIMIT 1", {level + 1});
        if (!nxt.empty()) nextExp = std::stoi(nxt[0].at("cum_exp"));
    }
    // client divides by next minus base so never let them meet
    if (nextExp <= floorExp) nextExp = floorExp + 1;

    // not clamped the lobby bar draws raw experience so a row under its own floor must stay visible
    out.expCurrent   = colI32(r, "experience");
    out.expLevelBase = floorExp;
    out.expLevelNext = nextExp;

    // portrait key zero renders nothing the stored column wins otherwise use the character the player has on the stand
    out.characterKey = colU32(r, "character_key");
    if (out.characterKey == 0) {
        InventoryPackets::CharacterRow cr;
        if (InventoryHandler::selectedCharacterRow(static_cast<int32_t>(characterId), cr))
            out.characterKey = cr.baseKey;
    }

    // race counters one query rank one is a win and rank zero never finished
    {
        auto agg = db.queryPrepared(
            "SELECT "
            "SUM(rank_in_race = 1) AS wins, "
            "SUM(rank_in_race > 1) AS placed, "
            "SUM(rank_in_race = 0) AS dnf "
            "FROM race_history WHERE character_id = ?",
            {characterId});
        // anything above rank one is a loss
        auto num = [](const Row& row, const char* c) -> int32_t {
            auto it = row.find(c);
            return it == row.end() || it->second.empty() ? 0 : std::stoi(it->second);
        };
        // statA1 is Victory statB1 is Lose statC1 drives Quit %
        if (!agg.empty()) {
            out.statA1 = num(agg[0], "wins");
            out.statB1 = num(agg[0], "placed");
            out.statC1 = num(agg[0], "dnf");
        }
    }
    out.statA2 = 0;
    out.statB2 = 0;
    out.statC2 = 0;

    out.pendantKey = colU32(r, "pendant_key");
    return true;
}

}  // namespace

// per session mirror of the client friend container

void SocialHandler::mirrorSet(uint32_t sessionId, std::unordered_set<uint32_t> ids) {
    std::lock_guard<std::mutex> lock(m_mirrorMutex);
    m_friendMirror[sessionId] = std::move(ids);
}

void SocialHandler::mirrorAdd(uint32_t sessionId, uint32_t friendId) {
    std::lock_guard<std::mutex> lock(m_mirrorMutex);
    auto it = m_friendMirror.find(sessionId);
    if (it == m_friendMirror.end()) return;
    it->second.insert(friendId);
}

void SocialHandler::mirrorErase(uint32_t sessionId, uint32_t friendId) {
    std::lock_guard<std::mutex> lock(m_mirrorMutex);
    auto it = m_friendMirror.find(sessionId);
    if (it == m_friendMirror.end()) return;
    it->second.erase(friendId);
}

bool SocialHandler::mirrorGet(uint32_t sessionId, std::unordered_set<uint32_t>& out) {
    std::lock_guard<std::mutex> lock(m_mirrorMutex);
    auto it = m_friendMirror.find(sessionId);
    if (it == m_friendMirror.end()) return false;
    out = it->second;
    return true;
}

void SocialHandler::onSessionClosed(uint32_t sessionId) {
    {
        std::lock_guard<std::mutex> lock(m_mirrorMutex);
        m_friendMirror.erase(sessionId);
    }
    {
        std::lock_guard<std::mutex> lock(m_presenceMutex);
        m_presence.erase(sessionId);
    }
    {
        std::lock_guard<std::mutex> lock(m_chatMutex);
        m_chatState.erase(sessionId);
    }
    {
        // partner keeps a stale id and the next relay clears it on lookup miss
        std::lock_guard<std::mutex> lock(m_talkMutex);
        m_talkPartner.erase(sessionId);
        m_talkInviteFrom.erase(sessionId);
    }
}

// server pushed lists nothing in the client ever asks for these

void SocialHandler::pushSocialLists(Session::Ptr s, GameServer* srv) {
    if (!s || s->characterId == 0) return;

    const uint32_t me = s->characterId;

    std::vector<SocialPackets::FriendRow> friends;
    std::unordered_set<uint32_t> mirror;
    {
        auto rows = Database::instance().queryPrepared(
            "SELECT c.id AS id, c.name AS name, c.level AS level "
            "FROM friends f JOIN characters c ON c.id = f.friend_id "
            "WHERE f.character_id = ? ORDER BY f.friend_id LIMIT 100",
            {me});
        friends.reserve(rows.size());
        for (const Row& r : rows) {
            CharBrief c = briefFromRow(r);
            friends.push_back(makeFriendRow(c, onlineSession(srv, c.id) != nullptr));
            mirror.insert(c.id);
        }
    }
    s->send(SocialPackets::friendList(friends));
    mirrorSet(s->id(), std::move(mirror));

    // list just changed under the poll so the next answer must be the full form
    {
        std::lock_guard<std::mutex> lock(m_presenceMutex);
        m_presence.erase(s->id());
    }

    std::vector<SocialPackets::RequestRow> requests;
    {
        auto rows = Database::instance().queryPrepared(
            "SELECT c.id AS id, c.name AS name, c.level AS level "
            "FROM friend_requests q JOIN characters c ON c.id = q.requester_id "
            "WHERE q.target_id = ? AND q.state = 0 ORDER BY q.id LIMIT 100",
            {me});
        requests.reserve(rows.size());
        for (const Row& r : rows) requests.push_back(makeRequestRow(briefFromRow(r)));
    }
    s->send(SocialPackets::friendRequestList(requests));

    std::vector<SocialPackets::BlockRow> blocks;
    {
        auto rows = Database::instance().queryPrepared(
            "SELECT c.id AS id, c.name AS name "
            "FROM blocked_players b JOIN characters c ON c.id = b.blocked_id "
            "WHERE b.character_id = ? ORDER BY b.id LIMIT 30",
            {me});
        blocks.reserve(rows.size());
        for (const Row& r : rows) {
            SocialPackets::BlockRow br;
            br.playerId = colU32(r, "id");
            br.name     = widen(colStr(r, "name"));
            blocks.push_back(br);
        }
    }
    s->send(SocialPackets::blockList(blocks));

    LOG_INFO("SOCIAL", "Pushed social lists to char " + std::to_string(me) +
                       " friends=" + std::to_string(friends.size()) +
                       " requests=" + std::to_string(requests.size()) +
                       " blocks=" + std::to_string(blocks.size()));

    deadDialogProbe(s);
}

// dead dialogs both client handlers end in the empty FUN 00463cd0

void SocialHandler::deadDialogProbe(Session::Ptr s) {
    if (!s || !deadDialogProbeEnabled()) return;

    LOG_WARN("SOCIAL", "Dead dialog probe armed, S2C 0x0025 and 0x0027 reach nullsub_8 "
                       "and draw nothing, this must never run in production");

    // both frames still parse so a capture can prove the reader offsets
    s->send(SocialPackets::deadDialog37(s->characterName, "MSG_PROBE_0025", 0));
    s->send(SocialPackets::deadDialog39(s->characterId, s->characterName, "MSG_PROBE_0027"));
}

// chat one opcode carries every scope the client can type

bool SocialHandler::chatGate(Session::Ptr s, std::u16string& text) {
    if (!s || text.empty()) return false;

    // a GM mute separate from the flood mute below costs no disconnect and the player is told why
    if (s->mutedUntil > 0) {
        const int64_t now = static_cast<int64_t>(::time(nullptr));
        if (now < s->mutedUntil) {
            const long long left = (s->mutedUntil - now + 59) / 60;
            std::u16string msg;
            for (char c : ("you are muted for " + std::to_string(left) + " more minutes"))
                msg.push_back(static_cast<char16_t>(static_cast<unsigned char>(c)));
            s->send(SocialPackets::chatBroadcast(0, u"GM", msg, SocialPackets::CHAT_NORMAL));
            return false;
        }
        s->mutedUntil = 0;
    }

    const int64_t now = nowMs();
    bool muted     = false;
    bool justMuted = false;

    {
        std::lock_guard<std::mutex> lock(m_chatMutex);
        ChatState& st = m_chatState[s->id()];

        if (!st.muteLoaded) {
            st.muteLoaded = true;
            auto rows = Database::instance().queryPrepared(
                "SELECT GREATEST(TIMESTAMPDIFF(SECOND, NOW(), muted_until), 0) AS left_s "
                "FROM chat_mutes WHERE character_id = ?",
                {s->characterId});
            if (!rows.empty()) {
                const int64_t left = colI32(rows[0], "left_s");
                if (left > 0) st.mutedUntilMs = now + left * 1000;
            }
        }

        if (st.mutedUntilMs > now) {
            muted = true;
        } else {
            st.sends.push_back(now);
            while (st.sends.size() > FLOOD_WINDOW_SENDS) st.sends.pop_front();
            if (st.sends.size() == FLOOD_WINDOW_SENDS &&
                now - st.sends.front() < FLOOD_WINDOW_MS) {
                st.mutedUntilMs = now + FLOOD_MUTE_MS;
                st.sends.clear();
                // client arms the same mute then still ships this line so relay it
                justMuted = true;
            }
        }
    }

    if (justMuted) {
        Database::instance().executePrepared(
            "INSERT INTO chat_mutes (character_id, muted_until, reason) "
            "VALUES (?, DATE_ADD(NOW(), INTERVAL ? SECOND), 'flood') "
            "ON DUPLICATE KEY UPDATE muted_until = VALUES(muted_until), reason = VALUES(reason)",
            {s->characterId, static_cast<int>(FLOOD_MUTE_MS / 1000)});
        LOG_INFO("SOCIAL", "Chat flood mute armed for char " +
                           std::to_string(s->characterId));
    }

    if (muted) {
        // honest client stays silent while muted so a line here means guard gone
        LOG_WARN("SOCIAL", "Chat while muted from char " + std::to_string(s->characterId));
        sendSystemKey(s, s->characterName, KEY_ANTI_CHAT);
        return false;
    }

    // honest client already masked so a hit here means the filter was patched out
    if (maskBannedWords(text)) {
        LOG_INFO("SOCIAL", "Masked banned word from char " + std::to_string(s->characterId));
    }
    return !text.empty();
}

void SocialHandler::handleChatSend(Session::Ptr s, Packet& pkt, GameServer* srv) {
    if (!s || !srv || s->characterId == 0) return;

    SocialPackets::ChatSend in;
    if (!SocialPackets::parseChatSend(pkt, in)) {
        LOG_WARN("SOCIAL", "Malformed C2S 0x00B4 from char " + std::to_string(s->characterId));
        return;
    }

    // type three is the big banner so a client that asks for it is spoofing
    if (in.chatType != SocialPackets::CHAT_NORMAL) {
        LOG_WARN("SOCIAL", "Chat type " + std::to_string(in.chatType) + " from char " +
                           std::to_string(s->characterId) + " forced back to normal");
    }

    std::u16string text = in.text;
    if (!chatGate(s, text)) return;

    if (handleDevCommand(s, text, srv)) return;
    if (handleGmCommand(s, text, srv)) return;
    if (handleNoticeProbe(s, text, srv)) return;

    const SocialPackets::ChatCommand cmd = SocialPackets::splitChatCommand(text);

    if (cmd.scope == SocialPackets::ChatScope::Whisper) {
        doWhisperByName(s, cmd.target, cmd.text, srv);
        return;
    }
    if (cmd.scope == SocialPackets::ChatScope::Team) {
        doTeamChat(s, cmd.text, srv);
        return;
    }

    if (cmd.text.empty()) return;

    // stock client cannot reach here while bound see relaySmallTalk for why
    if (relaySmallTalk(s, cmd.text, srv)) return;

    broadcastNormalChat(s, cmd.text, srv);
}

// the chat command helpers the client has no guild at all the pendant is the only badge a player wears

namespace {

std::u16string chatWiden(const std::string& s) {
    std::u16string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char16_t>(c));
    return out;
}

std::string narrowName(const std::u16string& w) {
    std::string out;
    out.reserve(w.size());
    for (char16_t c : w) if (c > 0 && c < 128) out += static_cast<char>(c);
    return out;
}

std::vector<std::string> chatWords(const std::u16string& raw) {
    std::vector<std::string> out;
    std::string cur;
    for (char16_t c : raw) {
        if (c == u' ') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else if (c > 0 && c < 128) {
            cur += static_cast<char>(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

}  // namespace

// GM commands the client ships no admin screen so chat is the surface gated on gm level and logged

bool SocialHandler::handleGmCommand(Session::Ptr s, const std::u16string& raw,
                                    GameServer* srv) {
    if (!s || s->characterId == 0 || !srv) return false;
    if (raw.size() < 3 || raw.compare(0, 3, u"/gm") != 0) return false;
    if (raw.size() > 3 && raw[3] != u' ') return false;

    auto reply = [&s](const std::string& line) {
        s->send(SocialPackets::chatBroadcast(0, u"GM", chatWiden(line),
                                             SocialPackets::CHAT_NORMAL));
    };

    if (s->gmLevel == 0) {
        // say nothing useful a normal player should not learn the surface exists
        LOG_WARN("GM", "char " + std::to_string(s->characterId) + " tried /gm without rights");
        return true;
    }

    const auto words = chatWords(raw);
    const std::string verb = words.size() > 1 ? words[1] : std::string("help");
    auto& db = Database::instance();

    auto logAction = [&](const std::string& action, const std::string& target,
                         const std::string& detail) {
        db.executePrepared(
            "INSERT INTO gm_log (gm_id, gm_name, action, target, detail) VALUES (?,?,?,?,?)",
            {std::to_string(s->characterId), narrowName(s->characterName), action, target, detail});
    };

    auto findChar = [&db](const std::string& name) -> std::string {
        auto r = db.queryPrepared("SELECT id FROM characters WHERE name = ? LIMIT 1", {name});
        return r.empty() ? std::string() : r[0].at("id");
    };

    if (verb == "who") {
        const auto all = srv->getLobbySessions();
        std::string line = "online " + std::to_string(all.size()) + ":";
        int n = 0;
        for (const auto& o : all) {
            if (!o || o->characterId == 0) continue;
            if (++n > 12) { line += " ..."; break; }
            line += " " + narrowName(o->characterName);
        }
        reply(line);
        return true;
    }

    if (verb == "announce") {
        if (words.size() < 3) { reply("usage /gm announce text"); return true; }
        std::u16string text;
        for (size_t i = 2; i < words.size(); ++i) {
            if (i > 2) text += u' ';
            text += chatWiden(words[i]);
        }
        // chat type 3 is the big on screen notice it ignores the sender name
        const auto notice = SocialPackets::chatNotice(text);
        for (const auto& o : srv->getLobbySessions()) if (o) o->send(notice);
        logAction("announce", "", narrowName(text));
        LOG_INFO("GM", "announce by " + narrowName(s->characterName));
        return true;
    }

    if (verb == "kick" || verb == "mute" || verb == "unmute" ||
        verb == "ban" || verb == "unban" || verb == "gold" ||
        verb == "cash" || verb == "level") {
        if (words.size() < 3) { reply("usage /gm " + verb + " name ..."); return true; }
        const std::string target = words[2];
        const std::string charId = findChar(target);
        if (charId.empty()) { reply("no character named " + target); return true; }
        auto online = srv->findSessionByCharacterId(std::stoi(charId));

        if (verb == "kick") {
            if (!online) { reply(target + " is not online"); return true; }
            online->send(PacketBuilder::displayMessage(u"MSG_KICKED_BY_GM", 2));
            online->stop();
            logAction("kick", target, "");
            reply("kicked " + target);
            return true;
        }

        if (verb == "mute") {
            const int minutes = words.size() > 3 ? atoi(words[3].c_str()) : 10;
            db.executePrepared(
                "UPDATE characters SET muted_until = DATE_ADD(NOW(), INTERVAL ? MINUTE) WHERE id = ?",
                {std::to_string(minutes), charId});
            if (online) online->mutedUntil = static_cast<int64_t>(::time(nullptr)) + minutes * 60;
            logAction("mute", target, std::to_string(minutes) + "m");
            reply("muted " + target + " for " + std::to_string(minutes) + " minutes");
            return true;
        }

        if (verb == "unmute") {
            db.executePrepared("UPDATE characters SET muted_until = NULL WHERE id = ?", {charId});
            if (online) online->mutedUntil = 0;
            logAction("unmute", target, "");
            reply("unmuted " + target);
            return true;
        }

        if (verb == "ban") {
            const int days = words.size() > 3 ? atoi(words[3].c_str()) : 0;
            std::string reason = "GM";
            for (size_t i = 4; i < words.size(); ++i) reason += " " + words[i];
            if (days > 0) {
                db.executePrepared(
                    "UPDATE accounts SET is_banned = 1, ban_reason = ?, "
                    "ban_expires_at = DATE_ADD(NOW(), INTERVAL ? DAY) "
                    "WHERE id = (SELECT account_id FROM characters WHERE id = ?)",
                    {reason, std::to_string(days), charId});
            } else {
                db.executePrepared(
                    "UPDATE accounts SET is_banned = 1, ban_reason = ?, ban_expires_at = NULL "
                    "WHERE id = (SELECT account_id FROM characters WHERE id = ?)",
                    {reason, charId});
            }
            if (online) {
                online->send(PacketBuilder::displayMessage(u"MSG_BANNED", 2));
                online->stop();
            }
            logAction("ban", target, days > 0 ? std::to_string(days) + "d" : "permanent");
            reply("banned " + target + (days > 0 ? " for " + std::to_string(days) + " days"
                                                : " permanently"));
            return true;
        }

        if (verb == "unban") {
            db.executePrepared(
                "UPDATE accounts SET is_banned = 0, ban_reason = NULL, ban_expires_at = NULL "
                "WHERE id = (SELECT account_id FROM characters WHERE id = ?)", {charId});
            logAction("unban", target, "");
            reply("unbanned " + target);
            return true;
        }

        // the three that hand out value level two only
        if (s->gmLevel < 2) { reply("that one needs gm level 2"); return true; }
        if (words.size() < 4) { reply("usage /gm " + verb + " name amount"); return true; }
        const long long amount = atoll(words[3].c_str());

        if (verb == "gold" || verb == "cash") {
            const char* col = verb == "gold" ? "gold" : "cash";
            db.executePrepared(std::string("UPDATE characters SET ") + col +
                               " = GREATEST(0, " + col + " + ?) WHERE id = ?",
                               {std::to_string(amount), charId});
            logAction(verb, target, std::to_string(amount));
            reply(std::string(col) + " " + (amount >= 0 ? "+" : "") +
                  std::to_string(amount) + " on " + target + ", relog to see it");
            return true;
        }

        if (verb == "level") {
            if (amount < 1 || amount > 99) { reply("level is 1 to 99"); return true; }
            db.executePrepared("UPDATE characters SET level = ? WHERE id = ?",
                               {std::to_string(amount), charId});
            logAction("level", target, std::to_string(amount));
            reply("level " + std::to_string(amount) + " on " + target + ", relog to see it");
            return true;
        }
    }

    reply("/gm who, announce, kick, mute name mins, unmute, ban name days, unban");
    if (s->gmLevel >= 2) reply("/gm gold name n, cash name n, level name n");
    return true;
}

// console for a server you run yourself wallet lands live through 0x112 experience lands through the finish stats
bool SocialHandler::handleDevCommand(Session::Ptr s, const std::u16string& raw,
                                     GameServer* srv) {
    if (!s || s->characterId == 0 || !srv) return false;
    if (raw.empty() || raw[0] != u'/') return false;
    const auto words = chatWords(raw);
    if (words.empty()) return false;
    const std::string verb = words[0];
    const bool money = verb == "/getmoney" || verb == "/getgold";
    const bool astro = verb == "/getastro" || verb == "/getcash";
    const bool exp   = verb == "/getexp";
    if (!money && !astro && !exp && verb != "/help") return false;

    auto reply = [&s](const std::string& line) {
        s->send(SocialPackets::chatBroadcast(0, u"Server", chatWiden(line),
                                             SocialPackets::CHAT_NORMAL));
    };
    // they were on unless the env said 0 so any player could print gold on a public server
    const bool enabled = devCommandsEnabled(std::getenv("KNC_DEV_COMMANDS"), s->gmLevel);
    if (verb == "/help") {
        reply(enabled ? "/getmoney n, /getastro n, /getexp n, /w name text, /t text"
                      : "/w name text, /t text");
        if (s->gmLevel > 0) reply("/gm for the operator commands");
        return true;
    }
    if (!enabled) {
        reply("commands are off on this server");
        return true;
    }
    if (words.size() < 2) { reply("usage " + verb + " amount"); return true; }
    long long amount = atoll(words[1].c_str());
    if (amount < 1) { reply("amount must be above zero"); return true; }
    if (amount > 99999999LL) amount = 99999999LL;

    auto& db = Database::instance();
    const std::string charId = std::to_string(s->characterId);
    const char* col = money ? "gold" : (astro ? "cash" : "experience");
    db.executePrepared(std::string("UPDATE characters SET ") + col + " = LEAST(99999999, " +
                       col + " + ?) WHERE id = ?", {std::to_string(amount), charId});
    auto row = db.queryPrepared(
        "SELECT gold, cash, experience, level FROM characters WHERE id = ? LIMIT 1", {charId});
    if (row.empty()) { reply("no character row"); return true; }
    const int32_t gold  = atoi(row[0].at("gold").c_str());
    const int32_t cash  = atoi(row[0].at("cash").c_str());
    const int32_t xp    = atoi(row[0].at("experience").c_str());
    int32_t level       = atoi(row[0].at("level").c_str());

    if (exp) {
        const int32_t want = ProgressionHandler::levelForExp(xp);
        if (want > level) {
            db.executePrepared("UPDATE characters SET level = ? WHERE id = ?",
                               {std::to_string(want), charId});
            level = want;
            s->send(PacketBuilder::msgLevelUp(s->characterName));
        }
        s->send(PacketBuilder::playerStatsUpdate(static_cast<int32_t>(s->characterId), xp, level, 0));
        reply("experience " + std::to_string(xp) + ", level " + std::to_string(level));
        return true;
    }

    ShopPackets::Wallet w;
    w.gp = gold;
    w.cash = cash;
    s->send(ShopPackets::extendOk(0, w));
    reply("gold " + std::to_string(gold) + ", astro " + std::to_string(cash));
    LOG_INFO("DEV", verb + " " + std::to_string(amount) + " for char " + charId);
    return true;
}

bool SocialHandler::handleNoticeProbe(Session::Ptr s, const std::u16string& raw,
                                      GameServer* srv) {
    (void)srv;
    if (!s || s->characterId == 0) return false;
    if (raw.size() < 7 || raw.compare(0, 7, u"/notice") != 0) return false;

    // S2C 0x116 is reversed but never rendered body is player id a line then a type byte eight are kept
    const auto words = chatWords(raw);
    const uint8_t type = words.size() > 1 ? static_cast<uint8_t>(atoi(words[1].c_str())) : 0;
    std::u16string text = u"notice test";
    if (words.size() > 2) {
        text.clear();
        for (size_t i = 2; i < words.size(); ++i) {
            if (i > 2) text += u' ';
            text += chatWiden(words[i]);
        }
    }
    if (type >= 100) {
        // 100 and up probe the invite popup instead 100 clears it
        s->send(PacketBuilder::invitePopupShort(type == 100 ? 0 : 1, s->displayName(),
                                                static_cast<int32_t>(s->characterId), text));
        s->send(SocialPackets::chatBroadcast(0, u"Probe",
            chatWiden("sent 0x12F count " + std::string(type == 100 ? "0" : "1")),
            SocialPackets::CHAT_NORMAL));
        LOG_INFO("SOCIAL", "0x12F probe to char " + std::to_string(s->characterId));
        return true;
    }
    s->send(PacketBuilder::playerNotice(static_cast<int32_t>(s->characterId), text, type));
    s->send(SocialPackets::chatBroadcast(0, u"Probe",
        chatWiden("sent 0x116 type " + std::to_string(type)), SocialPackets::CHAT_NORMAL));
    LOG_INFO("SOCIAL", "0x116 probe type " + std::to_string(type) +
                       " to char " + std::to_string(s->characterId));
    return true;
}

void SocialHandler::broadcastNormalChat(Session::Ptr s, const std::u16string& text,
                                        GameServer* srv) {
    // id must match the 0x0021 spawn id else stage nine raises no bubble displayName is the plain nickname
    auto line = SocialPackets::chatBroadcast(s->characterId, s->displayName(), text,
                                            SocialPackets::CHAT_NORMAL);

    if (s->roomId != 0) {
        auto room = srv->getRoom(s->roomId);
        if (room) {
            LOG_INFO("SOCIAL", "Room " + std::to_string(room->id()) + " chat from char " +
                               std::to_string(s->characterId));
            room->broadcast(line);
            return;
        }
    }

    LOG_INFO("SOCIAL", "Lobby chat from char " + std::to_string(s->characterId));
    for (const auto& sess : srv->getLobbySessions()) {
        if (sess) sess->send(line);
    }
}

void SocialHandler::doTeamChat(Session::Ptr s, const std::u16string& text, GameServer* srv) {
    if (text.empty()) return;

    // client prefills the team prefix on a hotkey even in the lobby
    if (s->roomId == 0) {
        broadcastNormalChat(s, text, srv);
        return;
    }
    auto room = srv->getRoom(s->roomId);
    if (!room) {
        broadcastNormalChat(s, text, srv);
        return;
    }

    const RoomPlayer* me = room->getPlayer(s->id());
    auto line = SocialPackets::chatBroadcast(s->characterId, s->displayName(), text,
                                             SocialPackets::CHAT_NORMAL);

    if (!me || !room->settings().teamMode || me->team == 0) {
        room->broadcast(line);
        return;
    }

    const uint8_t team = me->team;
    for (const auto& sess : room->sessions()) {
        if (!sess) continue;
        const RoomPlayer* p = room->getPlayer(sess->id());
        if (p && p->team == team) sess->send(line);
    }
    LOG_INFO("SOCIAL", "Team " + std::to_string(static_cast<int>(team)) + " chat in room " +
                       std::to_string(room->id()) + " from char " +
                       std::to_string(s->characterId));
}

// whisper the client owns no name based whisper opcode

void SocialHandler::deliverWhisper(Session::Ptr sender, Session::Ptr target,
                                   const std::u16string& text) {
    // session holds no login blob so the two proven fields are all we can fill
    const auto senderInfo = SocialPackets::userInfoStub(sender->characterId,
                                                        sender->displayName());
    const auto targetInfo = SocialPackets::userInfoStub(target->characterId,
                                                        target->displayName());

    // both sides read the same bytes and pick their own window from receiver zero
    auto pkt = SocialPackets::whisperDeliver(senderInfo, targetInfo, text);
    target->send(pkt);
    sender->send(pkt);

    LOG_INFO("SOCIAL", "Whisper char " + std::to_string(sender->characterId) + " to " +
                       std::to_string(target->characterId));
}

void SocialHandler::doWhisperByName(Session::Ptr s, const std::u16string& targetName,
                                    const std::u16string& text, GameServer* srv) {
    // bare prefix means the user only wanted the compose box back
    if (targetName.empty()) {
        s->send(SocialPackets::whisperPrompt());
        return;
    }

    Session::Ptr target = onlineByName(srv, targetName);
    if (!target || target->characterId == s->characterId) {
        sendSystemKey(s, targetName, KEY_NO_WHISPER);
        LOG_WARN("SOCIAL", "Whisper target offline " + logName(targetName));
        return;
    }
    if (hasBlocked(target->characterId, s->characterId)) {
        // never tell the sender it was a block
        sendSystemKey(s, targetName, KEY_NO_WHISPER);
        return;
    }
    if (text.empty()) {
        s->send(SocialPackets::whisperPrompt());
        return;
    }

    deliverWhisper(s, target, text);
    // rearm compose so the next line keeps the same target
    s->send(SocialPackets::whisperPrompt());
}

void SocialHandler::handleWhisperSend(Session::Ptr s, Packet& pkt, GameServer* srv) {
    if (!s || !srv || s->characterId == 0) return;

    SocialPackets::WhisperSend in;
    if (!SocialPackets::parseWhisperSend(pkt, in)) {
        LOG_WARN("SOCIAL", "Malformed C2S 0x00B5 from char " + std::to_string(s->characterId));
        return;
    }

    std::u16string text = in.text;
    if (!chatGate(s, text)) return;

    if (in.targetPlayerId == 0 || in.targetPlayerId == s->characterId) return;

    Session::Ptr target = onlineSession(srv, in.targetPlayerId);
    if (!target) {
        const CharBrief who = lookupById(in.targetPlayerId);
        sendSystemKey(s, who.found ? who.name : std::u16string(), KEY_NO_WHISPER);
        return;
    }
    if (hasBlocked(target->characterId, s->characterId)) {
        sendSystemKey(s, target->characterName, KEY_NO_WHISPER);
        return;
    }

    // window already exists on this path so no compose prompt
    deliverWhisper(s, target, text);
}

bool SocialHandler::relaySmallTalk(Session::Ptr s, const std::u16string& text, GameServer* srv) {
    uint32_t partnerChar = 0;
    {
        std::lock_guard<std::mutex> lock(m_talkMutex);
        auto it = m_talkPartner.find(s->id());
        if (it == m_talkPartner.end()) return false;
        partnerChar = it->second;
    }

    Session::Ptr partner = onlineSession(srv, partnerChar);
    if (!partner) {
        std::lock_guard<std::mutex> lock(m_talkMutex);
        m_talkPartner.erase(s->id());
        return false;
    }

    LOG_WARN("SOCIAL", "Small talk relay from char " + std::to_string(s->characterId) +
                       " stock client cannot send while bound so this one is patched");

    // type four draws raw with no name prefix so prepend the name ourselves
    std::u16string line = s->characterName;
    line += u": ";
    line += text;

    auto pkt = SocialPackets::smallTalkLine(s->characterId, s->characterName, line);
    partner->send(pkt);
    s->send(pkt);
    return true;
}

// name verbs room invite friend add block add small talk

void SocialHandler::handleNameRequest(Session::Ptr s, uint16_t op,
                                      const std::u16string& name, GameServer* srv) {
    if (!s || s->characterId == 0) return;

    switch (op) {
        case SocialPackets::OP_ROOM_INVITE:   doRoomInvite(s, name, srv);       break;
        case SocialPackets::OP_FRIEND_ADD:    doFriendAdd(s, name, srv);        break;
        case SocialPackets::OP_BLOCK_ADD:     doBlockAdd(s, name, srv);         break;
        case SocialPackets::OP_SMALLTALK_REQ: doSmallTalkRequest(s, name, srv); break;
        default:
            LOG_WARN("SOCIAL", "Name request on unmapped opcode " + opHex(op));
            break;
    }
}

void SocialHandler::doRoomInvite(Session::Ptr s, const std::u16string& name, GameServer* srv) {
    if (!srv) return;

    RoomInviteCheck check;
    check.senderInRoom = s->roomId != 0;
    auto room = check.senderInRoom ? srv->getRoom(s->roomId) : nullptr;
    check.roomAlive = room != nullptr;
    Session::Ptr target = onlineByName(srv, name);
    check.targetOnline = target != nullptr;
    check.targetIsSender = target && target->characterId == s->characterId;
    check.sameRoom = target && check.senderInRoom && target->roomId == s->roomId;
    check.blocked = target && hasBlocked(target->characterId, s->characterId);

    // every refusal answers the inviter on S2C 0x126 the stock client prints it in the lobby and the room
    if (const char* refusal = roomInviteRefusal(check)) {
        sendSystemKey(s, name, refusal);
        LOG_INFO("SOCIAL", "Room invite from char " + std::to_string(s->characterId) + " to " + logName(name) +
                           " refused " + refusal + (check.senderInRoom ? "" : ", the sender is not in a room"));
        return;
    }

    SocialPackets::RoomInvite inv;
    // only value the answer echoes back so route it on my own id
    inv.replyKey     = s->characterId;
    inv.inviterName  = s->characterName;
    inv.roomId       = s->roomId;
    inv.roomPassword = widen(room->settings().password);
    // zero enables the already in that room answer branch
    inv.flag = 0;

    target->send(SocialPackets::roomInvite(inv));
    LOG_INFO("SOCIAL", "Room invite char " + std::to_string(s->characterId) +
                       " to " + logName(name) + " room " + std::to_string(s->roomId));
}

void SocialHandler::doFriendAdd(Session::Ptr s, const std::u16string& name, GameServer* srv) {
    const uint32_t me = s->characterId;
    const CharBrief target = lookupByName(name);

    if (!target.found || target.id == me) {
        s->send(SocialPackets::friendAddResult(SocialPackets::MSG_FRIEND_NOUSER));
        return;
    }
    // never leak the block back to the requester
    if (hasBlocked(target.id, me) || hasBlocked(me, target.id)) {
        s->send(SocialPackets::friendAddResult(SocialPackets::MSG_FRIEND_NOUSER));
        return;
    }
    if (areFriends(me, target.id)) {
        s->send(SocialPackets::friendAddResult(SocialPackets::MSG_FRIEND_DUP));
        return;
    }
    if (countFriends(me) >= SocialPackets::CAP_FRIENDS) {
        s->send(SocialPackets::friendAddResult(SocialPackets::MSG_FRIEND_MAX));
        return;
    }
    if (countFriends(target.id) >= SocialPackets::CAP_FRIENDS ||
        countPendingFor(target.id) >= SocialPackets::CAP_FRIEND_REQUESTS) {
        s->send(SocialPackets::friendAddResult(SocialPackets::MSG_FRIEND_ACCEPTER_MAX));
        return;
    }

    // they already asked me so this click closes the pair
    if (pendingExists(target.id, me)) {
        Database::instance().executePrepared(
            "UPDATE friend_requests SET state = 1 WHERE requester_id = ? AND target_id = ?",
            {target.id, me});
        bindFriendPair(s, target.id, srv);
        return;
    }

    if (pendingExists(me, target.id)) {
        s->send(SocialPackets::friendAddResult(SocialPackets::MSG_FRIEND_DUP));
        return;
    }

    const bool ok = Database::instance().executePrepared(
        "INSERT INTO friend_requests (requester_id, target_id, state) VALUES (?, ?, 0) "
        "ON DUPLICATE KEY UPDATE state = 0",
        {me, target.id});
    if (!ok) {
        s->send(SocialPackets::friendAddResult(SocialPackets::MSG_FRIEND_NOUSER));
        LOG_ERROR("SOCIAL", "Friend request insert failed for char " + std::to_string(me));
        return;
    }

    s->send(SocialPackets::friendAddResult(SocialPackets::MSG_FRIEND_ADD));

    // result twelve appends the row on the side that must answer it
    if (auto other = onlineSession(srv, target.id)) {
        const CharBrief self = lookupById(me);
        if (self.found) other->send(SocialPackets::friendAddResultQueued(makeRequestRow(self)));
    }

    LOG_INFO("SOCIAL", "Friend request char " + std::to_string(me) +
                       " to " + std::to_string(target.id));
}

void SocialHandler::doBlockAdd(Session::Ptr s, const std::u16string& name, GameServer* srv) {
    (void)srv;
    const uint32_t me = s->characterId;
    const CharBrief target = lookupByName(name);

    // zero is the success code here so every failure must be non zero
    if (!target.found || target.id == me) {
        s->send(SocialPackets::blockAddResultError(SocialPackets::MSG_FRIEND_NOUSER));
        return;
    }
    if (hasBlocked(me, target.id)) {
        s->send(SocialPackets::blockAddResultError(SocialPackets::MSG_FRIEND_DUP));
        return;
    }
    if (countBlocks(me) >= SocialPackets::CAP_BLOCKS) {
        s->send(SocialPackets::blockAddResultError(SocialPackets::MSG_FRIEND_MAX));
        return;
    }

    const bool ok = Database::instance().executePrepared(
        "INSERT INTO blocked_players (character_id, blocked_id) VALUES (?, ?) "
        "ON DUPLICATE KEY UPDATE blocked_id = blocked_id",
        {me, target.id});
    if (!ok) {
        s->send(SocialPackets::blockAddResultError(SocialPackets::MSG_FRIEND_NOUSER));
        LOG_ERROR("SOCIAL", "Block insert failed for char " + std::to_string(me));
        return;
    }

    SocialPackets::BlockRow row;
    row.playerId = target.id;
    row.name     = target.name;
    s->send(SocialPackets::blockAddResultOk(row));

    LOG_INFO("SOCIAL", "Char " + std::to_string(me) + " blocked " + std::to_string(target.id));
}

void SocialHandler::doSmallTalkRequest(Session::Ptr s, const std::u16string& name,
                                       GameServer* srv) {
    if (!srv) return;

    Session::Ptr target = onlineByName(srv, name);
    if (!target || target->characterId == s->characterId) {
        sendSystemKey(s, name, KEY_NO_SUCH_USER);
        LOG_WARN("SOCIAL", "Small talk target offline: " + logName(name));
        return;
    }
    if (hasBlocked(target->characterId, s->characterId)) {
        // busy line hides the block and matches the client own refusal text
        sendSystemKey(s, name, KEY_REJECT_TALK);
        LOG_INFO("SOCIAL", "Small talk dropped, target blocked char " +
                           std::to_string(s->characterId));
        return;
    }

    bool busy = false;
    {
        std::lock_guard<std::mutex> lock(m_talkMutex);
        // one window per client so a second invite would be dropped anyway
        busy = m_talkPartner.find(target->id()) != m_talkPartner.end() ||
               m_talkInviteFrom.find(target->id()) != m_talkInviteFrom.end();
        if (!busy) m_talkInviteFrom[target->id()] = s->characterId;
    }
    if (busy) {
        sendSystemKey(s, name, KEY_REJECT_TALK);
        LOG_INFO("SOCIAL", "Small talk target busy char " +
                           std::to_string(target->characterId));
        return;
    }

    target->send(SocialPackets::smallTalkInvite(s->characterId, s->characterName));
    LOG_INFO("SOCIAL", "Small talk invite char " + std::to_string(s->characterId) +
                       " to " + std::to_string(target->characterId));
}

// id verbs accept reject friend del block del small talk

void SocialHandler::handleIdRequest(Session::Ptr s, uint16_t op, uint32_t playerId,
                                    GameServer* srv) {
    if (!s || s->characterId == 0) return;

    switch (op) {
        // sprite Aceiter on button zero proves 0x70 is accept see the header
        case SocialPackets::OP_FRIEND_REQ_ACCEPT: doFriendAccept(s, playerId, srv); break;
        case SocialPackets::OP_FRIEND_REQ_REJECT: doFriendReject(s, playerId, srv); break;
        case SocialPackets::OP_FRIEND_DEL:        doFriendDelete(s, playerId, srv); break;
        case SocialPackets::OP_BLOCK_DEL:         doBlockDelete(s, playerId, srv);  break;

        case SocialPackets::OP_SMALLTALK_ACCEPT:  doSmallTalkAccept(s, playerId, srv);  break;
        case SocialPackets::OP_SMALLTALK_DECLINE: doSmallTalkDecline(s, playerId, srv); break;
        case SocialPackets::OP_SMALLTALK_CLOSE:   doSmallTalkClose(s, playerId, srv);   break;

        default:
            LOG_WARN("SOCIAL", "Id request on unmapped opcode " + opHex(op));
            break;
    }
}

void SocialHandler::doSmallTalkAccept(Session::Ptr s, uint32_t peerId, GameServer* srv) {
    if (!srv || peerId == 0 || peerId == s->characterId) return;

    Session::Ptr peer = onlineSession(srv, peerId);
    if (!peer) {
        LOG_WARN("SOCIAL", "Small talk accept but peer " + std::to_string(peerId) + " is gone");
        return;
    }

    bool peerNeedsPopup = false;
    {
        std::lock_guard<std::mutex> lock(m_talkMutex);
        auto it = m_talkInviteFrom.find(s->id());
        // an invite on my side means the peer asked first and has no window yet
        const bool invited = it != m_talkInviteFrom.end() && it->second == peerId;
        auto bound = m_talkPartner.find(peer->id());
        // peer already bound means peer already clicked yes so no second popup
        const bool peerHasWindow = bound != m_talkPartner.end() &&
                                   bound->second == s->characterId;
        peerNeedsPopup = invited && !peerHasWindow;
        m_talkInviteFrom.erase(s->id());
        m_talkPartner[s->id()]    = peerId;
        m_talkPartner[peer->id()] = s->characterId;
        if (peerNeedsPopup) m_talkInviteFrom[peer->id()] = s->characterId;
    }

    if (peerNeedsPopup) {
        // sub 475CE0 is the only chat window maker so the requester needs its own mirrored invite
        peer->send(SocialPackets::smallTalkInvite(s->characterId, s->characterName));
    }

    LOG_INFO("SOCIAL", "Small talk bound " + std::to_string(s->characterId) + " and " +
                       std::to_string(peerId));
}

void SocialHandler::doSmallTalkDecline(Session::Ptr s, uint32_t peerId, GameServer* srv) {
    if (!srv) return;

    {
        std::lock_guard<std::mutex> lock(m_talkMutex);
        m_talkInviteFrom.erase(s->id());
        m_talkPartner.erase(s->id());
    }

    Session::Ptr peer = onlineSession(srv, peerId);
    if (peer) {
        {
            std::lock_guard<std::mutex> lock(m_talkMutex);
            m_talkPartner.erase(peer->id());
        }
        // type two draws raw text with no name prefix
        std::u16string line = s->characterName;
        line += u" declined the talk";
        peer->send(SocialPackets::chatBroadcast(0, std::u16string(), line,
                                                SocialPackets::CHAT_RAW));
    }

    LOG_INFO("SOCIAL", "Small talk declined by char " + std::to_string(s->characterId));
}

void SocialHandler::doSmallTalkClose(Session::Ptr s, uint32_t peerId, GameServer* srv) {
    if (!srv) return;

    {
        std::lock_guard<std::mutex> lock(m_talkMutex);
        m_talkPartner.erase(s->id());
        m_talkInviteFrom.erase(s->id());
    }

    Session::Ptr peer = onlineSession(srv, peerId);
    if (peer) {
        {
            std::lock_guard<std::mutex> lock(m_talkMutex);
            m_talkPartner.erase(peer->id());
        }
        // their box is still open so drop the notice inside it
        std::u16string line = s->characterName;
        line += u" left the talk";
        peer->send(SocialPackets::smallTalkLine(s->characterId, s->characterName, line));
    }

    LOG_INFO("SOCIAL", "Small talk closed by char " + std::to_string(s->characterId));
}

void SocialHandler::doFriendAccept(Session::Ptr s, uint32_t requesterId, GameServer* srv) {
    const uint32_t me = s->characterId;

    if (requesterId == 0 || requesterId == me) {
        s->send(SocialPackets::friendRequestResolved(requesterId, false));
        return;
    }

    if (!pendingExists(requesterId, me)) {
        // client drops the row whatever we say so clear the stale one
        s->send(SocialPackets::friendRequestResolved(requesterId, false));
        LOG_WARN("SOCIAL", "Accept without pending request char " + std::to_string(me) +
                           " from " + std::to_string(requesterId));
        return;
    }

    if (countFriends(me) >= SocialPackets::CAP_FRIENDS) {
        s->send(SocialPackets::friendRequestResolved(requesterId, false));
        s->send(SocialPackets::friendAddResult(SocialPackets::MSG_FRIEND_MAX));
        return;
    }

    Database::instance().executePrepared(
        "UPDATE friend_requests SET state = 1 WHERE requester_id = ? AND target_id = ?",
        {requesterId, me});

    s->send(SocialPackets::friendRequestResolved(requesterId, false));
    bindFriendPair(s, requesterId, srv);
}

void SocialHandler::doFriendReject(Session::Ptr s, uint32_t requesterId, GameServer* srv) {
    (void)srv;
    const uint32_t me = s->characterId;

    Database::instance().executePrepared(
        "UPDATE friend_requests SET state = 2 WHERE requester_id = ? AND target_id = ? AND state = 0",
        {requesterId, me});

    s->send(SocialPackets::friendRequestResolved(requesterId, true));
    LOG_INFO("SOCIAL", "Char " + std::to_string(me) + " rejected request from " +
                       std::to_string(requesterId));
}

void SocialHandler::bindFriendPair(Session::Ptr s, uint32_t otherId, GameServer* srv) {
    const uint32_t me = s->characterId;

    auto& db = Database::instance();
    db.executePrepared(
        "INSERT INTO friends (character_id, friend_id) VALUES (?, ?) "
        "ON DUPLICATE KEY UPDATE friend_id = friend_id",
        {me, otherId});
    db.executePrepared(
        "INSERT INTO friends (character_id, friend_id) VALUES (?, ?) "
        "ON DUPLICATE KEY UPDATE friend_id = friend_id",
        {otherId, me});

    const CharBrief other = lookupById(otherId);
    const CharBrief self  = lookupById(me);
    Session::Ptr otherSession = onlineSession(srv, otherId);

    if (other.found) {
        s->send(SocialPackets::friendAddResultAccepted(
            makeFriendRow(other, otherSession != nullptr)));
        mirrorAdd(s->id(), otherId);
    }

    if (otherSession && self.found) {
        otherSession->send(SocialPackets::friendAddResultAccepted(makeFriendRow(self, true)));
        mirrorAdd(otherSession->id(), me);
    }

    // the mirror grew so the next poll must be the full form
    {
        std::lock_guard<std::mutex> lock(m_presenceMutex);
        m_presence.erase(s->id());
        if (otherSession) m_presence.erase(otherSession->id());
    }

    LOG_INFO("SOCIAL", "Friend pair bound " + std::to_string(me) + " and " +
                       std::to_string(otherId));
}

void SocialHandler::doFriendDelete(Session::Ptr s, uint32_t friendId, GameServer* srv) {
    const uint32_t me = s->characterId;

    auto& db = Database::instance();
    db.executePrepared("DELETE FROM friends WHERE character_id = ? AND friend_id = ?",
                       {me, friendId});
    db.executePrepared("DELETE FROM friends WHERE character_id = ? AND friend_id = ?",
                       {friendId, me});
    db.executePrepared(
        "UPDATE friend_requests SET state = 2 "
        "WHERE state = 0 AND ((requester_id = ? AND target_id = ?) "
        "OR (requester_id = ? AND target_id = ?))",
        {me, friendId, friendId, me});

    s->send(SocialPackets::friendDelResult(friendId, SocialPackets::MSG_FRIEND_DEL));
    mirrorErase(s->id(), friendId);

    // other side keeps a dead row unless it is told too
    Session::Ptr other = onlineSession(srv, friendId);
    if (other) {
        other->send(SocialPackets::friendDelResult(me, SocialPackets::MSG_FRIEND_DEL));
        mirrorErase(other->id(), me);
    }

    // mirror shrank so the next poll must be the full form
    {
        std::lock_guard<std::mutex> lock(m_presenceMutex);
        m_presence.erase(s->id());
        if (other) m_presence.erase(other->id());
    }

    LOG_INFO("SOCIAL", "Char " + std::to_string(me) + " removed friend " +
                       std::to_string(friendId));
}

void SocialHandler::doBlockDelete(Session::Ptr s, uint32_t blockedId, GameServer* srv) {
    (void)srv;
    const uint32_t me = s->characterId;

    Database::instance().executePrepared(
        "DELETE FROM blocked_players WHERE character_id = ? AND blocked_id = ?",
        {me, blockedId});

    s->send(SocialPackets::blockDelResult(blockedId, 0));
    LOG_INFO("SOCIAL", "Char " + std::to_string(me) + " unblocked " + std::to_string(blockedId));
}

void SocialHandler::handleRoomInviteAnswer(Session::Ptr s,
                                           const SocialPackets::RoomInviteAnswer& a,
                                           GameServer* srv) {
    if (!s || !srv) return;

    // sub 47B030 picks the code one is the popup no button and the table ships no key for it
    const char*          reason = "declined";
    const char*          key    = nullptr;
    const std::u16string tail   = u" declined the invite";
    switch (a.answer) {
        case SocialPackets::INVITE_ANSWER_BUSY_SMALLTALK:
            reason = "busy in small talk"; key = KEY_REJECT_TALK;  break;
        case SocialPackets::INVITE_ANSWER_DECLINED:
            reason = "postcard note open"; key = KEY_REJECT_POST;  break;
        case SocialPackets::INVITE_ANSWER_IN_RACE:
            reason = "in race";            key = KEY_REJECT_GAME;  break;
        case SocialPackets::INVITE_ANSWER_SAME_ROOM:
            reason = "already in room";    key = KEY_REJECT_ROOM;  break;
        default: break;
    }

    // replyKey is the inviter id this server chose when it built the 0x6C
    Session::Ptr inviter = onlineSession(srv, a.replyKey);
    if (inviter) {
        if (key != nullptr) {
            sendSystemKey(inviter, s->characterName, key);
        } else {
            std::u16string line = s->characterName;
            line += tail;
            inviter->send(SocialPackets::chatNotice(line));
        }
    }

    LOG_INFO("SOCIAL", "Room invite answer from char " + std::to_string(s->characterId) +
                       " replyKey " + std::to_string(a.replyKey) +
                       " code " + std::to_string(a.answer) + " " + reason);
}

// profiles

void SocialHandler::handleUserInfoByName(Session::Ptr s, const std::u16string& name,
                                         GameServer* srv) {
    (void)srv;
    if (!s) return;

    const CharBrief target = lookupByName(name);
    if (!target.found) {
        LOG_WARN("SOCIAL", "Profile by name miss: " + logName(name));
        return;
    }

    SocialPackets::Profile profile;
    if (!loadProfile(target.id, profile)) return;

    s->send(SocialPackets::userInfoBlob(profile));
}

void SocialHandler::handleUserInfoById(Session::Ptr s, uint32_t playerId, GameServer* srv) {
    (void)srv;
    if (!s) return;

    SocialPackets::Profile profile;
    if (!loadProfile(playerId, profile)) {
        LOG_WARN("SOCIAL", "Profile by id miss: " + std::to_string(playerId));
        return;
    }

    s->send(SocialPackets::userInfoPopup(profile));
}

// presence poll every three seconds so keep it off the database

void SocialHandler::handleStatusPoll(Session::Ptr s, GameServer* srv) {
    if (!s || s->characterId == 0 || !srv) return;

    std::unordered_set<uint32_t> mirror;
    if (!mirrorGet(s->id(), mirror)) {
        // list must land first else the status write null derefs
        pushSocialLists(s, srv);
        if (!mirrorGet(s->id(), mirror)) return;
    }

    std::unordered_set<uint32_t>          online;
    std::unordered_map<uint32_t, int32_t> activity;
    if (!mirror.empty()) {
        for (const auto& sess : srv->getSessions()) {
            if (!sess || sess->characterId == 0) continue;
            if (mirror.find(sess->characterId) == mirror.end()) continue;
            if (!online.insert(sess->characterId).second) continue;
            activity[sess->characterId] = activityCode(srv, sess);
        }
    }

    std::lock_guard<std::mutex> lock(m_presenceMutex);
    PresenceSnapshot& snap = m_presence[s->id()];

    // only 0x0073 resets the rows it omits so a set change needs the full form
    if (!snap.valid || snap.online != online) {
        std::vector<SocialPackets::StatusRow> rows;
        rows.reserve(online.size());
        for (uint32_t id : online) {
            SocialPackets::StatusRow row;
            row.playerId = id;
            // sign is the only thing the row draw reads so zero means online
            row.statusA = 0;
            row.statusB = activity[id];
            rows.push_back(row);
        }
        s->send(SocialPackets::friendStatusFull(rows));

        snap.valid    = true;
        snap.online   = std::move(online);
        snap.activity = std::move(activity);
        return;
    }

    // same set so only the inert activity word can have moved
    std::vector<SocialPackets::StatusPartialRow> delta;
    for (uint32_t id : online) {
        auto prev = snap.activity.find(id);
        if (prev != snap.activity.end() && prev->second == activity[id]) continue;
        SocialPackets::StatusPartialRow row;
        row.playerId = id;
        row.statusB  = activity[id];
        delta.push_back(row);
    }
    if (delta.empty()) return;

    // deliberate policy this frame only moves statusB which no client code reads so the delta is inert kept for fidelity
    s->send(SocialPackets::friendStatusPartial(delta));
    snap.activity = std::move(activity);
}

// note

void SocialHandler::handleNoteSend(Session::Ptr s, const SocialPackets::NoteSend& n,
                                   GameServer* srv) {
    if (!s || s->characterId == 0) return;

    const uint32_t me = s->characterId;

    if (n.recipientName.empty()) {
        s->send(SocialPackets::messengerResult(SocialPackets::MSG_NOTE_INPUTUSER));
        return;
    }
    if (n.body.empty()) {
        s->send(SocialPackets::messengerResult(SocialPackets::MSG_NOTE_INPUTNOTE));
        return;
    }

    const CharBrief target = lookupByName(n.recipientName);
    if (!target.found) {
        s->send(SocialPackets::messengerResult(SocialPackets::MSG_NOTE_NOUSER));
        return;
    }
    if (hasBlocked(target.id, me)) {
        s->send(SocialPackets::messengerResult(SocialPackets::MSG_NOTE_NOSEND));
        return;
    }

    std::u16string body = n.body;
    if (body.size() > SocialPackets::MAX_NOTE_BODY_CHARS) {
        body.resize(SocialPackets::MAX_NOTE_BODY_CHARS);
    }
    maskBannedWords(body);

    // quotes arrive already doubled by the client so store the body verbatim one pinned connection matches the insert id
    Transaction tx = Database::instance().beginTransaction();
    const bool ok = tx.valid() && tx.execute(
        "INSERT INTO player_notes (from_id, to_id, to_name, body) VALUES (?, ?, ?, ?)",
        {me, target.id, narrow(target.name), narrow(body)});
    const uint32_t noteId = ok ? static_cast<uint32_t>(tx.lastInsertId()) : 0;
    if (!ok || !tx.commit()) {
        if (tx.valid()) tx.rollback();
        s->send(SocialPackets::messengerResult(SocialPackets::MSG_NOTE_NOSEND));
        LOG_ERROR("SOCIAL", "Note insert failed from char " + std::to_string(me));
        return;
    }

    s->send(SocialPackets::messengerResult(SocialPackets::MSG_NOTE_SEND));
    LOG_INFO("SOCIAL", "Note char " + std::to_string(me) + " to " + std::to_string(target.id));

    // sub 47B710 an online recipient sees it at once on 0x82 offline ones get it in the 0x83 backlog
    if (auto other = onlineSession(srv, target.id)) {
        SocialPackets::NoteRow row;
        if (loadNoteRow(noteId, row)) {
            other->send(SocialPackets::noteAppend(row));
        }
    }
}

void SocialHandler::pushNoteBacklog(Session::Ptr s, GameServer* srv) {
    (void)srv;
    if (!s || s->characterId == 0) return;

    // the client holds 30 slots and drops the oldest so ship the newest oldest first and let 0x83 settle it
    auto rows = Database::instance().queryPrepared(
        std::string(NOTE_SELECT) + "WHERE n.to_id = ? ORDER BY n.created_at DESC, n.id DESC LIMIT 30",
        {s->characterId});
    if (rows.empty()) return;

    for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
        s->send(SocialPackets::noteAppendSorted(noteFromRow(*it)));
    }
    LOG_INFO("SOCIAL", "Note backlog " + std::to_string(rows.size()) + " rows to char " +
             std::to_string(s->characterId));
}

void SocialHandler::handleNoteMarkRead(Session::Ptr s, uint32_t noteId) {
    if (!s || s->characterId == 0) return;
    // sub 47B8D0 scoped to the owner so a forged id cannot touch another inbox the ack still returns
    Database::instance().executePrepared(
        "UPDATE player_notes SET is_read = 1 WHERE id = ? AND to_id = ?",
        {noteId, s->characterId});
    s->send(SocialPackets::noteAck(SocialPackets::OP_NOTE_MARK_READ, noteId));
}

void SocialHandler::handleNoteDelete(Session::Ptr s, uint32_t noteId) {
    if (!s || s->characterId == 0) return;
    Database::instance().executePrepared(
        "DELETE FROM player_notes WHERE id = ? AND to_id = ?",
        {noteId, s->characterId});
    s->send(SocialPackets::noteAck(SocialPackets::OP_NOTE_DELETE, noteId));
}

// lobby user list fixed seven slot page

void SocialHandler::handleUserListPage(Session::Ptr s, uint32_t page, GameServer* srv) {
    if (!s || !srv) return;

    const size_t slots = SocialPackets::USERLIST_SLOTS;

    std::vector<uint32_t> online;
    {
        std::unordered_set<uint32_t> seen;
        for (const auto& sess : srv->getSessions()) {
            if (!sess || sess->characterId == 0) continue;
            if (!seen.insert(sess->characterId).second) continue;
            online.push_back(sess->characterId);
        }
    }
    // stable order else paging shuffles rows between the ten second refreshes
    std::sort(online.begin(), online.end());

    uint32_t pageCount = static_cast<uint32_t>((online.size() + slots - 1) / slots);
    if (pageCount == 0) pageCount = 1;
    if (page >= pageCount) page = pageCount - 1;

    const size_t first = static_cast<size_t>(page) * slots;
    std::vector<uint32_t> slice;
    for (size_t i = first; i < online.size() && slice.size() < slots; ++i) {
        slice.push_back(online[i]);
    }

    std::vector<SocialPackets::UserListEntry> entries;
    if (!slice.empty()) {
        std::string marks;
        DbParams params;
        params.reserve(slice.size());
        for (size_t i = 0; i < slice.size(); ++i) {
            if (i != 0) marks += ",";
            marks += "?";
            params.push_back(slice[i]);
        }

        auto rows = Database::instance().queryPrepared(
            "SELECT id, name, level, COALESCE(pendant_key, 0) AS pendant_key "
            "FROM characters WHERE id IN (" + marks + ")",
            params);

        std::map<uint32_t, Row> byId;
        for (const Row& r : rows) byId[colU32(r, "id")] = r;

        for (uint32_t id : slice) {
            auto it = byId.find(id);
            if (it == byId.end()) continue;
            SocialPackets::UserListEntry e;
            e.playerId    = id;
            e.name        = widen(colStr(it->second, "name"));
            // same zero based icon index as every other level field
            e.level       = colU32(it->second, "level") > 0
                                ? colU32(it->second, "level") - 1 : 0;
            // sub 429D30 draws UserList pendant NN 00 by this number so it is the worn key
            e.pendantSlot = colI32(it->second, "pendant_key");
            entries.push_back(e);
        }
    }

    s->send(SocialPackets::userListPage(page, pageCount, entries));
}

} // namespace knc
