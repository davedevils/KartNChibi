/// quest catalog and per character quest state client stage 26

#include "handlers/QuestHandler.h"
#include "packets/gen/ShopPackets.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace knc {

namespace {

using DbRow = std::map<std::string, std::string>;
using QuestDef = QuestPackets::QuestDefWire;
using QuestState = QuestPackets::QuestStateEntry;

// client recv buffer is a fixed 0x2000 and a bigger frame kills its parse state
constexpr size_t kFrameCap = 0x2000;

// only these four indices are ever reported by the race state machine
constexpr uint32_t kReportedIndexMax = 3;

// one catalog row plus the exact 112 bytes stage 26 will receive
struct QuestRow {
    QuestDef def;
    std::array<uint8_t, QuestPackets::QUEST_DEF_SIZE> record{};
};

std::mutex g_catalogMutex;
std::vector<QuestRow> g_rows;
bool g_catalogLoaded = false;

uint32_t rowU32(const DbRow& row, const char* key) {
    return static_cast<uint32_t>(rowUInt64NoThrow(row, key, 0));
}

uint32_t readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

void sendChecked(const Session::Ptr& session, const Packet& pkt, const char* what) {
    if (pkt.payload().size() + 8 >= kFrameCap) {
        LOG_ERROR("QUEST", std::string(what) + " frame " +
                           std::to_string(pkt.payload().size() + 8) +
                           " past the client buffer " + std::to_string(kFrameCap) + " dropped");
        return;
    }
    session->send(pkt);
}

/// refuses the whole set on any breach a partial publish is what crashes stage 26
bool validateCatalog(const std::vector<QuestDef>& defs) {
    if (defs.size() > QuestPackets::QUEST_DEF_MAX) {
        LOG_ERROR("QUEST", "catalog has " + std::to_string(defs.size()) +
                           " rows past the client cap " +
                           std::to_string(QuestPackets::QUEST_DEF_MAX) + " refused");
        return false;
    }

    bool seen[QuestPackets::QUEST_THEME_MAX][QuestPackets::QUEST_ROWS_PER_THEME] = {};

    for (const auto& d : defs) {
        if (d.themeId >= QuestPackets::QUEST_THEME_MAX) {
            LOG_ERROR("QUEST", "quest index " + std::to_string(d.questIndex) + " theme " +
                               std::to_string(d.themeId) + " past 3 refused");
            return false;
        }
        if (d.questIndex > QuestPackets::QUEST_INDEX_MAX) {
            LOG_ERROR("QUEST", "quest index " + std::to_string(d.questIndex) +
                               " past 15 refused");
            return false;
        }
        if (QuestPackets::themeOf(d.questIndex) != d.themeId) {
            LOG_ERROR("QUEST", "quest index " + std::to_string(d.questIndex) + " theme " +
                               std::to_string(d.themeId) +
                               " breaks four times theme plus row refused");
            return false;
        }

        const uint32_t row = QuestPackets::rowOf(d.questIndex);
        if (row >= QuestPackets::QUEST_ROWS_PER_THEME) {
            LOG_ERROR("QUEST", "quest index " + std::to_string(d.questIndex) + " row " +
                               std::to_string(row) + " past 3 refused");
            return false;
        }
        if (seen[d.themeId][row]) {
            LOG_ERROR("QUEST", "quest index " + std::to_string(d.questIndex) +
                               " duplicated refused");
            return false;
        }
        if (d.strKeyTitle.empty()) {
            LOG_ERROR("QUEST", "quest index " + std::to_string(d.questIndex) +
                               " has an empty string key refused");
            return false;
        }
        if (d.strKeyTitle.size() > QuestPackets::QUEST_STR_KEY_SLOT - 1) {
            LOG_ERROR("QUEST", "quest index " + std::to_string(d.questIndex) +
                               " string key " + std::to_string(d.strKeyTitle.size()) +
                               " past 67 refused");
            return false;
        }
        for (char c : d.strKeyTitle) {
            if (c < 0x20 || c > 0x7E) {
                LOG_ERROR("QUEST", "quest index " + std::to_string(d.questIndex) +
                                   " string key is not printable ascii refused");
                return false;
            }
        }
        seen[d.themeId][row] = true;
    }

    // a gap makes sub 452240 return zero and two call sites deref plus 44 unchecked
    for (uint32_t theme = 0; theme < QuestPackets::QUEST_THEME_MAX; ++theme) {
        bool ended = false;
        for (uint32_t row = 0; row < QuestPackets::QUEST_ROWS_PER_THEME; ++row) {
            if (!seen[theme][row]) {
                ended = true;
                continue;
            }
            if (ended) {
                LOG_ERROR("QUEST", "theme " + std::to_string(theme) + " has a gap before row " +
                                   std::to_string(row) + " refused");
                return false;
            }
        }
    }
    return true;
}

std::unordered_set<uint32_t> disabledIndices() {
    std::unordered_set<uint32_t> off;
    auto rows = Database::instance().queryPrepared(
        "SELECT quest_index FROM quest_def WHERE COALESCE(is_enabled, 1) = 0 LIMIT 64", {});
    for (const auto& r : rows) off.insert(rowU32(r, "quest_index"));
    return off;
}

std::vector<QuestRow> loadRows() {
    std::vector<QuestDef> defs = QuestPackets::loadQuestDefs();
    if (defs.empty()) {
        LOG_WARN("QUEST", "quest_def is empty so stage 26 has nothing to draw");
        return {};
    }

    // the gen loader has no is enabled filter so disabled keys are dropped here
    const std::unordered_set<uint32_t> off = disabledIndices();
    if (!off.empty()) {
        std::vector<QuestDef> kept;
        kept.reserve(defs.size());
        for (const auto& d : defs) {
            if (off.find(d.questIndex) != off.end()) continue;
            kept.push_back(d);
        }
        defs.swap(kept);
    }

    if (defs.empty() || !validateCatalog(defs)) {
        return {};
    }

    // build the wire record once here so every login only replays the bytes
    const std::vector<Packet> frames = QuestPackets::questCatalog(defs);
    if (frames.size() != defs.size()) {
        LOG_ERROR("QUEST", "builder kept " + std::to_string(frames.size()) + " of " +
                           std::to_string(defs.size()) + " rows so nothing is published");
        return {};
    }

    std::vector<QuestRow> rows;
    rows.reserve(defs.size());
    for (size_t i = 0; i < defs.size(); ++i) {
        const std::vector<uint8_t>& pl = frames[i].payload();
        if (pl.size() != QuestPackets::QUEST_DEF_SIZE) {
            LOG_ERROR("QUEST", "definition frame " + std::to_string(i) + " is " +
                               std::to_string(pl.size()) + " bytes not 112 refused");
            return {};
        }
        // dword one is the key so the pairing is checked never assumed
        if (readU32(pl.data() + 4) != defs[i].questIndex) {
            LOG_ERROR("QUEST", "definition frame " + std::to_string(i) +
                               " carries a different quest index refused");
            return {};
        }
        QuestRow r;
        r.def = defs[i];
        std::memcpy(r.record.data(), pl.data(), QuestPackets::QUEST_DEF_SIZE);
        rows.push_back(std::move(r));
    }

    return rows;
}

std::vector<QuestRow> cachedRows() {
    std::lock_guard<std::mutex> lock(g_catalogMutex);
    if (!g_catalogLoaded) {
        g_rows = loadRows();
        g_catalogLoaded = true;
    }
    return g_rows;
}

const QuestRow* findRow(const std::vector<QuestRow>& rows, uint32_t questIndex) {
    for (const auto& r : rows) {
        if (r.def.questIndex == questIndex) return &r;
    }
    return nullptr;
}

const QuestState* findState(const std::vector<QuestState>& rows, uint32_t questIndex) {
    for (const auto& r : rows) {
        if (r.questIndex == questIndex) return &r;
    }
    return nullptr;
}

// step buttons carry no lock state so withholding rows is the only gate see the header for the widget proof
uint32_t unlockedThemes(const std::vector<QuestRow>& rows,
                        const std::vector<QuestState>& state) {
    (void)state;
    // every theme with rows is open the weekly and event tabs sat empty waiting on the previous theme
    uint32_t unlocked = 0;
    for (uint32_t theme = 0; theme < QuestPackets::QUEST_THEME_MAX; ++theme) {
        if (findRow(rows, QuestPackets::questIndexFor(theme, 0)) == nullptr) break;
        ++unlocked;
    }
    return unlocked == 0 ? 1 : unlocked;
}

/// the contiguous rows of every unlocked theme in publish order
std::vector<const QuestRow*> publishableRows(const std::vector<QuestRow>& rows,
                                             uint32_t unlocked) {
    std::vector<const QuestRow*> out;
    for (uint32_t theme = 0; theme < unlocked && theme < QuestPackets::QUEST_THEME_MAX;
         ++theme) {
        for (uint32_t row = 0; row < QuestPackets::QUEST_ROWS_PER_THEME; ++row) {
            const QuestRow* r = findRow(rows, QuestPackets::questIndexFor(theme, row));
            if (r == nullptr) break;
            out.push_back(r);
        }
    }
    return out;
}

std::vector<QuestDef> defsOf(const std::vector<const QuestRow*>& rows) {
    std::vector<QuestDef> out;
    out.reserve(rows.size());
    for (const QuestRow* r : rows) out.push_back(r->def);
    return out;
}

bool isPublished(const std::vector<const QuestRow*>& rows, uint32_t questIndex) {
    for (const QuestRow* r : rows) {
        if (r->def.questIndex == questIndex) return true;
    }
    return false;
}

} // namespace

bool QuestHandler::reloadCatalog() {
    std::lock_guard<std::mutex> lock(g_catalogMutex);
    g_rows = loadRows();
    g_catalogLoaded = true;

    LOG_INFO("QUEST", "catalog holds " + std::to_string(g_rows.size()) + " quests");
    return !g_rows.empty();
}

std::vector<QuestPackets::QuestDefWire> QuestHandler::catalog() {
    const std::vector<QuestRow> rows = cachedRows();
    std::vector<QuestDef> out;
    out.reserve(rows.size());
    for (const auto& r : rows) out.push_back(r.def);
    return out;
}

size_t QuestHandler::publishTheme(Session::Ptr session, uint32_t themeId) {
    if (!session) return 0;
    if (themeId >= QuestPackets::QUEST_THEME_MAX) {
        LOG_ERROR("QUEST", "theme " + std::to_string(themeId) + " past 3 not published");
        return 0;
    }

    const std::vector<QuestRow> rows = cachedRows();
    size_t sent = 0;
    for (uint32_t row = 0; row < QuestPackets::QUEST_ROWS_PER_THEME; ++row) {
        const uint32_t idx = QuestPackets::questIndexFor(themeId, row);
        const QuestRow* r = findRow(rows, idx);
        // a gap would shift every later row of the theme so stop at the first miss
        if (r == nullptr) break;
        // single row so build it straight instead of walking the cached set
        sendChecked(session, QuestPackets::questDefinition(r->def), "questDefinition");
        ++sent;
    }

    LOG_INFO("QUEST", "theme " + std::to_string(themeId) + " published " +
                      std::to_string(sent) + " definitions to char " +
                      std::to_string(session->characterId));
    return sent;
}

void QuestHandler::onCharacterEnter(Session::Ptr session, GameServer* server) {
    (void)server;

    if (session->characterId == 0) {
        LOG_WARN("QUEST", "quest push with no character from " + session->remoteAddress());
        return;
    }

    std::vector<QuestRow> rows = cachedRows();
    if (rows.empty()) {
        // db may have been unseeded when the process booted so try once more
        reloadCatalog();
        rows = cachedRows();
    }
    if (rows.empty()) {
        LOG_WARN("QUEST", "no publishable catalog so stage 26 stays empty for char " +
                          std::to_string(session->characterId));
        return;
    }

    const std::vector<QuestState> raw =
        QuestPackets::loadQuestState(static_cast<int32_t>(session->characterId));

    const uint32_t unlocked = unlockedThemes(rows, raw);
    const std::vector<const QuestRow*> pub = publishableRows(rows, unlocked);
    if (pub.empty()) {
        LOG_ERROR("QUEST", "theme zero has no rows so nothing can be published to char " +
                           std::to_string(session->characterId));
        return;
    }

    // catalog only ever grows so one burst per session and never a second
    for (const QuestRow* r : pub) {
        sendChecked(session, QuestPackets::questDefinitionRaw(r->record.data()),
                    "questDefinition");
    }

    const std::vector<QuestDef> pubDefs = defsOf(pub);
    const std::vector<QuestState> safe = QuestPackets::sanitizeStateList(raw, pubDefs);
    if (safe.size() != raw.size()) {
        LOG_WARN("QUEST", "state list dropped " + std::to_string(raw.size() - safe.size()) +
                          " rows for char " + std::to_string(session->characterId));
    }

    sendChecked(session, QuestPackets::questStateList(safe), "questStateList");

    LOG_INFO("QUEST", "published " + std::to_string(pub.size()) + " defs over " +
                      std::to_string(unlocked) + " themes and " +
                      std::to_string(safe.size()) + " state rows to char " +
                      std::to_string(session->characterId));
}

void QuestHandler::handleAccept(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    QuestPackets::QuestIndexReq req;
    if (!QuestPackets::parseAcceptRequest(packet, req)) {
        LOG_WARN("QUEST", "accept short payload from " + session->remoteAddress());
        return;
    }
    if (session->characterId == 0) {
        LOG_WARN("QUEST", "accept with no character from " + session->remoteAddress());
        return;
    }

    const std::vector<QuestRow> rows = cachedRows();
    const std::vector<QuestState> state =
        QuestPackets::loadQuestState(static_cast<int32_t>(session->characterId));
    const std::vector<const QuestRow*> pub =
        publishableRows(rows, unlockedThemes(rows, state));

    // a state row the catalog does not hold is a deref of plus 44 in the race hud
    if (!isPublished(pub, req.questIndex)) {
        LOG_WARN("QUEST", "accept index " + std::to_string(req.questIndex) +
                          " is not published to char " +
                          std::to_string(session->characterId));
        return;
    }

    // sub 47F270 appends with no dupe check a second ack poisons the list
    if (findState(state, req.questIndex) != nullptr) {
        LOG_WARN("QUEST", "accept index " + std::to_string(req.questIndex) +
                          " already in the state list of char " +
                          std::to_string(session->characterId));
        return;
    }
    for (const auto& r : state) {
        if (r.state == QuestPackets::STATE_IN_PROGRESS) {
            LOG_WARN("QUEST", "accept index " + std::to_string(req.questIndex) +
                              " refused, index " + std::to_string(r.questIndex) +
                              " is already in progress");
            session->send(ShopPackets::rejectAscii("MSG_ALREADY_RUNNING", 1));
            return;
        }
    }

    const bool ok = Database::instance().executePrepared(
        "INSERT INTO char_quest_state (char_id, quest_index, state, progress_count) "
        "VALUES (?, ?, 1, 0)",
        { static_cast<int32_t>(session->characterId), req.questIndex });

    if (!ok) {
        LOG_ERROR("QUEST", "accept index " + std::to_string(req.questIndex) +
                           " could not be stored for char " +
                           std::to_string(session->characterId));
        session->send(ShopPackets::rejectAscii("MSG_DB_ACCESS_FAIL", 1));
        return;
    }

    sendChecked(session, QuestPackets::questAcceptAck(req.questIndex), "questAcceptAck");
    LOG_INFO("QUEST", "char " + std::to_string(session->characterId) + " accepted index " +
                      std::to_string(req.questIndex));
}

void QuestHandler::handleDiscard(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    QuestPackets::QuestIndexReq req;
    if (!QuestPackets::parseDiscardRequest(packet, req)) {
        LOG_WARN("QUEST", "discard short payload from " + session->remoteAddress());
        return;
    }
    if (session->characterId == 0) {
        LOG_WARN("QUEST", "discard with no character from " + session->remoteAddress());
        return;
    }

    const std::vector<QuestState> rows =
        QuestPackets::loadQuestState(static_cast<int32_t>(session->characterId));
    const QuestState* row = findState(rows, req.questIndex);

    if (row == nullptr) {
        LOG_WARN("QUEST", "discard index " + std::to_string(req.questIndex) +
                          " absent from the state list of char " +
                          std::to_string(session->characterId));
        return;
    }
    if (row->state != QuestPackets::STATE_IN_PROGRESS) {
        LOG_WARN("QUEST", "discard index " + std::to_string(req.questIndex) + " is in state " +
                          std::to_string(row->state) + " and only state 1 can be dropped");
        return;
    }

    const bool ok = Database::instance().executePrepared(
        "DELETE FROM char_quest_state WHERE char_id = ? AND quest_index = ?",
        { static_cast<int32_t>(session->characterId), req.questIndex });

    if (!ok) {
        LOG_ERROR("QUEST", "discard index " + std::to_string(req.questIndex) +
                           " could not be stored for char " +
                           std::to_string(session->characterId));
        session->send(ShopPackets::rejectAscii("MSG_DB_ACCESS_FAIL", 1));
        return;
    }

    sendChecked(session, QuestPackets::questDiscardAck(req.questIndex), "questDiscardAck");
    LOG_INFO("QUEST", "char " + std::to_string(session->characterId) + " discarded index " +
                      std::to_string(req.questIndex));
}

void QuestHandler::handleProgressReport(Session::Ptr session, Packet& packet,
                                        GameServer* server) {
    (void)server;

    QuestPackets::QuestIndexReq req;
    if (!QuestPackets::parseProgressReport(packet, req)) {
        LOG_WARN("QUEST", "progress short payload from " + session->remoteAddress());
        return;
    }
    if (session->characterId == 0) {
        LOG_WARN("QUEST", "progress with no character from " + session->remoteAddress());
        return;
    }
    if (req.questIndex > kReportedIndexMax) {
        LOG_WARN("QUEST", "progress index " + std::to_string(req.questIndex) +
                          " is never reported by the stock client so it is forged");
    }

    const std::vector<QuestRow> rows = cachedRows();
    const std::vector<QuestState> state =
        QuestPackets::loadQuestState(static_cast<int32_t>(session->characterId));

    const uint32_t unlockedBefore = unlockedThemes(rows, state);
    const std::vector<const QuestRow*> pub = publishableRows(rows, unlockedBefore);

    if (!isPublished(pub, req.questIndex)) {
        LOG_WARN("QUEST", "progress index " + std::to_string(req.questIndex) +
                          " is not published to char " +
                          std::to_string(session->characterId));
        return;
    }

    const QuestState* row = findState(state, req.questIndex);

    // sub 47F2F0 and sub 47F340 deref the lookup with no null check so the row must exist
    if (row == nullptr) {
        LOG_WARN("QUEST", "progress index " + std::to_string(req.questIndex) +
                          " absent from the state list of char " +
                          std::to_string(session->characterId));
        return;
    }
    if (row->state != QuestPackets::STATE_IN_PROGRESS) {
        LOG_WARN("QUEST", "progress index " + std::to_string(req.questIndex) + " is in state " +
                          std::to_string(row->state) + " so nothing is credited");
        return;
    }

    const uint32_t progress = row->progressCount + 1;
    const uint32_t goal = QuestPackets::loadQuestGoal(req.questIndex);
    const bool done = goal > 0 && progress >= goal;
    const uint32_t newState = done ? QuestPackets::STATE_COMPLETE
                                   : QuestPackets::STATE_IN_PROGRESS;

    if (goal == 0) {
        LOG_WARN("QUEST", "index " + std::to_string(req.questIndex) +
                          " has no goal_count so it can never complete");
    }

    const bool ok = Database::instance().executePrepared(
        "UPDATE char_quest_state SET progress_count = ?, state = ? "
        "WHERE char_id = ? AND quest_index = ?",
        { progress, newState, static_cast<int32_t>(session->characterId), req.questIndex });

    if (!ok) {
        LOG_ERROR("QUEST", "progress index " + std::to_string(req.questIndex) +
                           " could not be stored for char " +
                           std::to_string(session->characterId));
        return;
    }

    if (done) {
        sendChecked(session, QuestPackets::questCompleted(req.questIndex, progress),
                    "questCompleted");

        const std::vector<QuestState> after =
            QuestPackets::loadQuestState(static_cast<int32_t>(session->characterId));
        const uint32_t unlockedAfter = unlockedThemes(rows, after);
        // append only container so the new step goes out row by row right now
        for (uint32_t theme = unlockedBefore; theme < unlockedAfter; ++theme) {
            publishTheme(session, theme);
        }
    } else {
        sendChecked(session, QuestPackets::questProgress(req.questIndex, progress),
                    "questProgress");
    }

    LOG_INFO("QUEST", "char " + std::to_string(session->characterId) + " index " +
                      std::to_string(req.questIndex) + " progress " +
                      std::to_string(progress) + " of " + std::to_string(goal) +
                      " state " + std::to_string(newState));
}

} // namespace knc
