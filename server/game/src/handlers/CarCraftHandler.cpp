/// car craft screen stage 18 s2c 0107 0108 0109 010A 010B

#include "handlers/CarCraftHandler.h"
#include "packets/gen/CharCreatePackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <exception>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

namespace knc {

namespace {

// room and grid tails ask for the same block many times per join so keep it
std::mutex g_blockMutex;
std::unordered_map<uint64_t, std::array<uint8_t, 0x3C>> g_blocks;

uint64_t blockKey(int32_t characterId, uint32_t kartInstanceId) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(characterId)) << 32) | kartInstanceId;
}

void storeBlock(uint64_t key, const std::array<uint8_t, 0x3C>& block) {
    std::lock_guard<std::mutex> lock(g_blockMutex);
    g_blocks[key] = block;
}

uint32_t rowU32(const std::map<std::string, std::string>& row, const char* key) {
    return static_cast<uint32_t>(rowUInt64Throwing(row, key, 0));
}

bool samePreset(const CarPreset& a, const CarPreset& b) {
    return a.presetId == b.presetId &&
           a.slotState == b.slotState &&
           a.kartInstanceId == b.kartInstanceId &&
           a.partCover == b.partCover &&
           a.partTire == b.partTire &&
           a.partBooster == b.partBooster &&
           a.partBumper == b.partBumper &&
           a.partFFender == b.partFFender &&
           a.partRFender == b.partRFender &&
           a.partWing == b.partWing;
}

void slotPointers(CarPreset& p, uint32_t* out[7]) {
    out[0] = &p.partCover;
    out[1] = &p.partTire;
    out[2] = &p.partBooster;
    out[3] = &p.partBumper;
    out[4] = &p.partFFender;
    out[5] = &p.partRFender;
    out[6] = &p.partWing;
}

bool statBlockIsZero(const CarPartDef& def) {
    for (int i = 0; i < 17; ++i) {
        if (def.stats[i] != 0.0f) return false;
    }
    return true;
}

// def trans index names car craft parts COVER to WING then the key and sub 4E1B70 wants an exact hit
std::string shippedTitleKey(uint32_t partKey, uint32_t category) {
    static const char* const kFamily[7] = {"COVER", "BOOSTER", "TIRES", "F_FENDER", "R_FENDER", "BUMPER", "WING"};
    if (category > 6) return "PART_" + std::to_string(partKey) + "_TITLE";
    return std::string(kFamily[category]) + "_" + std::to_string(partKey) + "_TITLE";
}

// no ack here else stage init runs on a preset it cannot resolve
void refuseScreen(const Session::Ptr& session, const std::string& why) {
    LOG_WARN("CARCRAFT", why);
    session->send(CharCreatePackets::messageKeyBox("MSG_UNSUPPORT", 1));
}

} // namespace

CarCraftHandler::CarCraftView CarCraftHandler::buildView(int32_t characterId) {
    CarCraftView view;
    auto& db = Database::instance();

    // one built slot per owned chassis with its basic set the top row of sub 42EFE0 lists them
    if (CustomCarPackets::syncFactoryLoadouts(characterId)) invalidateCustomCar(characterId);

    // 0x430420 derefs the 0xC0 row of the preset kart with no null test so only published karts qualify
    for (const auto& r : db.queryPrepared(
             "SELECT k.id, COALESCE(v.is_factory_car, 0) AS f FROM owned_kart k "
             "JOIN vehicle_templates v ON v.id = k.base_key AND COALESCE(v.is_enabled, 1) = 1 "
             "WHERE k.character_id = ? ORDER BY k.id ASC",
             {characterId})) {
        const uint32_t id = rowU32(r, "id");
        if (id == 0) continue;
        view.ownedKarts.insert(id);
        if (rowU32(r, "f") != 0) view.factoryKarts.push_back(id);
    }

    // same order and cap the login push uses so both sides agree
    std::unordered_set<uint32_t> defKeys;
    auto defs = db.queryPrepared(
        "SELECT part_key FROM carcraft_part_def ORDER BY part_key LIMIT 256", {});
    for (const auto& r : defs) {
        defKeys.insert(rowU32(r, "part_key"));
    }

    const auto allParts = CustomCarPackets::loadPartInstances(characterId);
    for (const auto& p : allParts) {
        if (view.parts.size() >= CustomCarPackets::MAX_PARTS_PER_CHAR) {
            LOG_WARN("CARCRAFT", "character " + std::to_string(characterId) +
                                 " part list capped client drops the rest");
            break;
        }
        // missing def is an unguarded deref on every mouse move in the stage
        if (defKeys.find(p.partKey) == defKeys.end()) {
            LOG_ERROR("CARCRAFT", "part instance " + std::to_string(p.instanceId) +
                                  " key " + std::to_string(p.partKey) +
                                  " has no definition dropping it");
            continue;
        }
        view.parts.push_back(p);
    }

    CustomCarPackets::ensureDefaultPreset(characterId);
    view.presets = CustomCarPackets::loadPresets(characterId);

    std::unordered_set<uint32_t> validInstances;
    for (const auto& p : view.parts) validInstances.insert(p.instanceId);
    bool repaired = false;

    for (auto& preset : view.presets) {
        const CarPreset before = preset;
        const std::string name = CustomCarPackets::clampPresetName(preset.name);

        if (preset.slotState != CustomCarPackets::SLOT_BUILT) {
            preset = CustomCarPackets::emptyPreset(preset.presetId);
        } else {
            uint32_t* slots[7];
            slotPointers(preset, slots);
            for (uint32_t* slot : slots) {
                if (*slot == 0) continue;
                if (validInstances.find(*slot) != validInstances.end()) continue;
                LOG_WARN("CARCRAFT", "preset " + std::to_string(preset.presetId) +
                                     " slot points at unknown part " + std::to_string(*slot));
                *slot = 0;
            }
            // a built car lost its chassis or its tire so the slot is empty again like a fresh one
            if (!CustomCarPackets::presetIsBuildable(preset, view.factoryKarts, view.parts)) {
                LOG_WARN("CARCRAFT", "preset " + std::to_string(preset.presetId) + " kart " +
                                     std::to_string(preset.kartInstanceId) +
                                     " is no owned factory chassis with a tire back to the empty slot");
                preset = CustomCarPackets::emptyPreset(preset.presetId);
            } else {
                preset.name = name;
            }
        }

        if (!samePreset(before, preset)) {
            // persist the repair else every open logs the same damage
            if (!CustomCarPackets::savePreset(characterId, preset)) {
                LOG_WARN("CARCRAFT", "preset " + std::to_string(preset.presetId) +
                                     " repair could not be persisted");
            } else {
                CustomCarPackets::syncRefcounts(characterId);
            }
            invalidateCustomCar(characterId);
            repaired = true;
        }
    }

    if (repaired) {
        // a repair moved the equip counts so the 0x0109 rows carry the stored ones
        const auto stored = CustomCarPackets::loadPartInstances(characterId);
        for (auto& p : view.parts) {
            for (const auto& s : stored) {
                if (s.instanceId == p.instanceId) p.equipRefcount = s.equipRefcount;
            }
        }
    }

    view.ok = true;
    return view;
}

std::array<uint8_t, 0x3C> CarCraftHandler::customCarFor(int32_t characterId,
                                                        uint32_t kartInstanceId) {
    if (characterId <= 0 || kartInstanceId == 0) {
        return CustomCarPackets::customCarBlockEmpty();
    }

    const uint64_t key = blockKey(characterId, kartInstanceId);
    {
        std::lock_guard<std::mutex> lock(g_blockMutex);
        auto it = g_blocks.find(key);
        if (it != g_blocks.end()) return it->second;
    }

    // sub 490A70 only walks the block when kart catalog dword five is one
    if (!CustomCarPackets::isFactoryKart(kartInstanceId)) {
        const std::array<uint8_t, 0x3C> empty = CustomCarPackets::customCarBlockEmpty();
        storeBlock(key, empty);
        return empty;
    }

    const std::array<uint8_t, 0x3C> block =
        CustomCarPackets::customCarBlockFor(characterId, kartInstanceId);
    storeBlock(key, block);
    return block;
}

void CarCraftHandler::invalidateCustomCar(int32_t characterId) {
    const uint64_t hi = static_cast<uint64_t>(static_cast<uint32_t>(characterId)) << 32;
    std::lock_guard<std::mutex> lock(g_blockMutex);
    for (auto it = g_blocks.begin(); it != g_blocks.end();) {
        if ((it->first & 0xFFFFFFFF00000000ULL) == hi) it = g_blocks.erase(it);
        else ++it;
    }
}

void CarCraftHandler::pruneClosedSessions() {
    for (auto it = m_sent.begin(); it != m_sent.end();) {
        if (it->second.owner.expired()) it = m_sent.erase(it);
        else ++it;
    }
}

void CarCraftHandler::sendPartDefs(Session::Ptr session) {
    {
        std::lock_guard<std::mutex> lock(m_sentMutex);
        pruneClosedSessions();

        SentParts& entry = m_sent[session->id()];
        entry.owner = session;
        if (entry.characterId != session->characterId) {
            entry.characterId = session->characterId;
            entry.instanceIds.clear();
        }
        // sub 44F9F0 appends with no key compare so a second burst duplicates
        if (entry.defsSent) return;
        entry.defsSent = true;
    }

    const std::vector<CarPartDef> defs = CustomCarPackets::loadPartDefs();
    if (defs.empty()) {
        LOG_ERROR("CARCRAFT", "carcraft_part_def is empty so every owned part is dropped "
                              "and stage eighteen has nothing to install");
        return;
    }

    size_t sent = 0;
    size_t zeroStats = 0;
    size_t oddKeys = 0;
    for (const auto& d : defs) {
        if (sent >= CustomCarPackets::MAX_PART_DEFS) {
            LOG_ERROR("CARCRAFT", "carcraft_part_def over client cap " +
                                  std::to_string(CustomCarPackets::MAX_PART_DEFS) +
                                  " extra keys resolve to null in stage eighteen");
            break;
        }
        if (statBlockIsZero(d)) ++zeroStats;
        if (d.displayNameKey != shippedTitleKey(d.partKey, d.category)) ++oddKeys;
        session->send(CustomCarPackets::partDef(d));
        ++sent;
    }

    if (oddKeys != 0) {
        // sub 42FE20 hands def rec plus 0x35 straight to sub 4E1B70 with no fallback
        LOG_WARN("CARCRAFT", std::to_string(oddKeys) + " part defs carry a display key that is not "
                             "the shipped COVER to WING _<key>_TITLE form so sub_4E1B70 finds no row in "
                             "Define/Eng/def_trans_index.txt and the part name draws raw");
    }

    if (sent != 0 && zeroStats == sent) {
        // whole shipped tree has no numeric part table only prose in def trans message
        LOG_INFO("CARCRAFT", "every part def carries an all zero stat block so sub_48F710 adds "
                             "nothing and parts stay cosmetic, the 0x44 block has no shipped "
                             "numeric source, author carcraft_part_def stat_block_hex from the "
                             "PART_<key>_INFO prose in Define/Eng/def_trans_message.txt");
    }

    LOG_INFO("CARCRAFT", "pushed " + std::to_string(sent) + " part definitions to session " +
                         std::to_string(session->id()));
}

void CarCraftHandler::sendPartInstances(Session::Ptr session,
                                        const std::vector<CarPartInstance>& parts) {
    std::vector<CarPartInstance> fresh;
    {
        std::lock_guard<std::mutex> lock(m_sentMutex);
        pruneClosedSessions();

        SentParts& entry = m_sent[session->id()];
        entry.owner = session;
        if (entry.characterId != session->characterId) {
            entry.characterId = session->characterId;
            entry.instanceIds.clear();
        }
        for (const auto& p : parts) {
            // container append has no key compare so a resend duplicates the row
            if (entry.instanceIds.find(p.instanceId) != entry.instanceIds.end()) continue;
            entry.instanceIds.insert(p.instanceId);
            fresh.push_back(p);
        }
    }

    for (const auto& p : fresh) {
        session->send(CustomCarPackets::partInstance(p));
    }
    if (!fresh.empty()) {
        LOG_DEBUG("CARCRAFT", "pushed " + std::to_string(fresh.size()) +
                              " new part instances to session " +
                              std::to_string(session->id()));
    }
}

void CarCraftHandler::forgetSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_sentMutex);
    m_sent.erase(sessionId);
}

void CarCraftHandler::sendLoginSnapshot(Session::Ptr session) {
    if (!session) return;
    const int32_t charId = static_cast<int32_t>(session->characterId);
    if (charId == 0) return;

    // defs first a 0x0109 whose key has no def is an unguarded deref later
    sendPartDefs(session);

    CarCraftView view = buildView(charId);
    if (!view.ok) {
        LOG_WARN("CARCRAFT", "login snapshot skipped for character " +
                             std::to_string(charId));
        return;
    }

    sendPartInstances(session, view.parts);
    session->send(CustomCarPackets::presetList(view.presets));

    // room join batches 0x0021 in one write so resolve the block of every built car before it
    for (const auto& p : view.presets) {
        if (p.slotState != CustomCarPackets::SLOT_BUILT) continue;
        const std::array<uint8_t, 0x3C> block = customCarFor(charId, p.kartInstanceId);
        LOG_DEBUG("CARCRAFT", "custom car block for char " + std::to_string(charId) +
                              " kart " + std::to_string(p.kartInstanceId) +
                              (block[0] != 0 ? " carries its chassis" : " is empty"));
    }
}

void CarCraftHandler::pushFactoryLoadout(Session::Ptr session) {
    if (!session) return;
    const int32_t charId = static_cast<int32_t>(session->characterId);
    if (charId == 0) return;
    if (!CustomCarPackets::syncFactoryLoadouts(charId)) return;
    invalidateCustomCar(charId);

    sendPartDefs(session);
    CarCraftView view = buildView(charId);
    if (!view.ok) return;
    // the new basic set lands before the full replace 0x0107 that names it
    sendPartInstances(session, view.parts);
    session->send(CustomCarPackets::presetList(view.presets));
    LOG_INFO("CARCRAFT", "chassis loadout pushed to char " + std::to_string(charId) + " " +
                         std::to_string(view.presets.size()) + " slots");
}

void CarCraftHandler::handleOpen(Session::Ptr session, GameServer* server) {
    (void)server;
    if (!session) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    if (charId == 0) {
        refuseScreen(session, "car craft open with no character");
        return;
    }

    LOG_INFO("CARCRAFT", "open stage eighteen char " + std::to_string(charId));

    // no op when the login push already sent them this session
    sendPartDefs(session);

    CarCraftView view = buildView(charId);
    if (!view.ok) {
        refuseScreen(session, "car craft open refused for character " +
                              std::to_string(charId));
        return;
    }

    // every record has to land before the ack closes the wait modal
    sendPartInstances(session, view.parts);
    session->send(CustomCarPackets::presetList(view.presets));
    session->send(CustomCarPackets::openCarCraftAck());
}

void CarCraftHandler::handleSave(Session::Ptr session, const CarSaveRequest& req,
                                 GameServer* server) {
    (void)server;
    if (!session) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    if (charId == 0 || !req.valid) {
        refuseScreen(session, "car craft save with no character or bad body");
        return;
    }

    CarCraftView view = buildView(charId);
    if (!view.ok || view.presets.empty()) {
        refuseScreen(session, "car craft save refused for character " +
                              std::to_string(charId));
        return;
    }

    bool rejected = false;

    const CarPreset* match = nullptr;
    for (const auto& p : view.presets) {
        if (p.presetId == req.presetId) { match = &p; break; }
    }
    if (!match) {
        LOG_WARN("CARCRAFT", "save names preset " + std::to_string(req.presetId) +
                             " not owned by character " + std::to_string(charId));
        match = &view.presets[0];
        rejected = true;
    }

    const CarPreset current = *match;
    CarPreset saved = rejected ? current : CustomCarPackets::validateSave(req, current, view.parts);

    // the craft needs an owned factory chassis and a tire a refusal answers with the stored slot
    bool kartTaken = false;
    for (const auto& other : view.presets) {
        if (other.presetId != saved.presetId && other.slotState == CustomCarPackets::SLOT_BUILT &&
            other.kartInstanceId == saved.kartInstanceId) {
            kartTaken = true;
        }
    }
    if (!rejected && (!CustomCarPackets::presetIsBuildable(saved, view.factoryKarts, view.parts) || kartTaken)) {
        LOG_WARN("CARCRAFT", "save wants kart " + std::to_string(saved.kartInstanceId) + " tire " +
                             std::to_string(saved.partTire) +
                             " which is no owned free factory chassis with a tire for character " +
                             std::to_string(charId));
        saved = current;
        rejected = true;
    }

    // the 0x20 car config is the whole identity of a preset nothing else can move
    const bool sameConfig = CustomCarPackets::carConfigBlob(saved) ==
                            CustomCarPackets::carConfigBlob(current) &&
                            saved.slotState == current.slotState;

    if (sameConfig) {
        LOG_DEBUG("CARCRAFT", "preset " + std::to_string(saved.presetId) +
                              " unchanged skipping the write");
    } else if (!CustomCarPackets::savePreset(charId, saved)) {
        LOG_ERROR("CARCRAFT", "preset " + std::to_string(saved.presetId) +
                              " write failed replying with the stored row");
        saved = current;
        rejected = true;
    } else {
        CustomCarPackets::syncRefcounts(charId);
    }

    // reply carries db truth so the client stops trusting its own refcounts
    std::unordered_set<uint32_t> allowed;
    for (const auto& p : view.parts) allowed.insert(p.instanceId);

    std::vector<CarPartInstance> fresh;
    for (const auto& p : CustomCarPackets::loadPartInstances(charId)) {
        if (allowed.find(p.instanceId) != allowed.end()) fresh.push_back(p);
    }

    if (!sameConfig) {
        // preset just moved so replace the room copy instead of only dropping it
        invalidateCustomCar(charId);
        const uint32_t chassis = CustomCarPackets::chassisKartKey(saved.kartInstanceId);
        if (chassis == 0) {
            LOG_WARN("CARCRAFT", "kart instance " + std::to_string(saved.kartInstanceId) +
                                 " has no catalog key so the room block stays empty");
        } else if (CustomCarPackets::isFactoryKart(saved.kartInstanceId)) {
            storeBlock(blockKey(charId, saved.kartInstanceId),
                       CustomCarPackets::customCarBlock(chassis, saved, fresh));
        }
    }

    std::unordered_map<uint32_t, int32_t> clientSaid;
    const size_t pairs = req.clientInstanceIds.size() < req.clientRefcounts.size()
                       ? req.clientInstanceIds.size() : req.clientRefcounts.size();
    for (size_t i = 0; i < pairs; ++i) {
        clientSaid[req.clientInstanceIds[i]] = req.clientRefcounts[i];
    }

    // frame cap means not every part fits so correct the wrong ones first
    std::vector<CarPartInstance> reply;
    std::unordered_set<uint32_t> picked;
    for (const auto& p : fresh) {
        if (reply.size() >= MAX_PARTS_IN_SAVE_RESULT) break;
        auto it = clientSaid.find(p.instanceId);
        const bool clientWrong = (it != clientSaid.end() && it->second != p.equipRefcount);
        if (p.equipRefcount == 0 && !clientWrong) continue;
        reply.push_back(p);
        picked.insert(p.instanceId);
    }
    for (const auto& p : fresh) {
        if (reply.size() >= MAX_PARTS_IN_SAVE_RESULT) break;
        if (picked.find(p.instanceId) != picked.end()) continue;
        reply.push_back(p);
    }
    if (reply.size() < fresh.size()) {
        LOG_WARN("CARCRAFT", "save result truncated to " + std::to_string(reply.size()) +
                             " of " + std::to_string(fresh.size()) + " parts frame cap");
    }

    session->send(CustomCarPackets::saveResult(saved, saved.kartInstanceId, reply));

    // sub 47E530 selects the kart of the answer with no test so the stored selection follows
    if (saved.kartInstanceId != 0 && view.ownedKarts.count(saved.kartInstanceId) != 0) {
        Database::instance().executePrepared(
            "UPDATE characters SET selected_kart_instance_id = ? WHERE id = ?",
            {saved.kartInstanceId, charId});
    }

    if (rejected) {
        // save result always draws MSG SAVE DONE so refuse after it
        session->send(CharCreatePackets::messageKeyBox("MSG_UNSUPPORT", 1));
    }

    LOG_INFO("CARCRAFT", "saved preset " + std::to_string(saved.presetId) +
                         " kart " + std::to_string(saved.kartInstanceId) +
                         " char " + std::to_string(charId));
}

void CarCraftHandler::handleRename(Session::Ptr session, const CarRenameRequest& req,
                                   GameServer* server) {
    (void)server;
    if (!session) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    if (charId == 0 || !req.valid) {
        LOG_WARN("CARCRAFT", "preset rename with no character or bad body");
        return;
    }

    const std::vector<CarPreset> presets = CustomCarPackets::loadPresets(charId);
    const CarPreset* match = nullptr;
    for (const auto& p : presets) {
        if (p.presetId == req.presetId) { match = &p; break; }
    }
    if (!match) {
        // no owned row to safely echo back refuse instead of acking a guess
        LOG_WARN("CARCRAFT", "rename names preset " + std::to_string(req.presetId) +
                             " not owned by character " + std::to_string(charId));
        session->send(CharCreatePackets::messageKeyBox("MSG_UNSUPPORT", 1));
        return;
    }

    const bool renamed = CustomCarPackets::renamePreset(charId, req.presetId, req.name);

    CarPreset result = *match;
    if (renamed) {
        result.name = CustomCarPackets::clampPresetName(req.name);
    } else {
        LOG_ERROR("CARCRAFT", "preset " + std::to_string(req.presetId) +
                              " rename write failed for character " + std::to_string(charId));
    }

    // ack unlocks the dialog either way row update keeps the slot label in sync
    session->send(CustomCarPackets::presetRenameAck(result.presetId, result.name));
    session->send(CustomCarPackets::presetRowUpdate(result));

    if (!renamed) {
        session->send(CharCreatePackets::messageKeyBox("MSG_UNSUPPORT", 1));
    }

    LOG_INFO("CARCRAFT", "renamed preset " + std::to_string(req.presetId) + " to '" +
                         result.name + "' char " + std::to_string(charId));
}

} // namespace knc
