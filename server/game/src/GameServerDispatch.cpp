/// the opcode switch one call per packet no logic beyond routing and small inline replies

#include "GameServer.h"
#include "GameServerInternal.h"
#include "packets/gen/GhostPackets.h"
#include "packets/gen/PartStatPackets.h"
#include "packets/gen/GachaPetPackets.h"
#include "packets/PacketBuilder.h"
#include "packets/gen/ShopPackets.h"
#include "packets/gen/MissionPackets.h"
#include "packets/gen/SocialPackets.h"
#include "packets/gen/CustomCarPackets.h"
#include "packets/gen/SpawnPackets.h"
#include "packets/gen/ItemPackets.h"
#include "handlers/ProgressionHandler.h"
#include "handlers/QuestHandler.h"
#include "logging/Logger.h"
#include "util/InviteOption.h"
#include <unordered_set>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <memory>
#include <functional>
#include <array>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <cctype>
#include "security/BanManager.h"
#include "security/PacketValidator.h"
#include "db/Database.h"
#include "handlers/LicenseHandler.h"
#include "handlers/MissionHandler.h"
#include "handlers/AntiCheatHandler.h"
#include "handlers/GarageHandler.h"
#include "handlers/LobbyHandler.h"
#include "handlers/GhostHandler.h"
#include "handlers/ScenarioHandler.h"
#include "handlers/CharCreateHandler.h"
#include "handlers/ItemHandler.h"
#include "handlers/GachaHandler.h"
#include "handlers/KeepaliveAnticheatHandler.h"
#include "crypto/PasswordHash.h"
#include "util/PendantRules.h"
#include "util/RetiredRoutes.h"

namespace knc {

// room passwords are ascii on the row and utf16 on the wire
static std::u16string widenAscii(const std::string& in) {
    std::u16string out;
    out.reserve(in.size());
    for (unsigned char c : in) out.push_back(static_cast<char16_t>(c));
    return out;
}

void GameServer::handlePacket(Session::Ptr session, Packet& packet) {
    uint8_t cmd = packet.cmd();
    // dispatch on the full u16 or opcodes above 0xFF truncate to their low byte and collide
    const uint16_t op = packet.opcode();

    std::string hexDump;
    const auto& payload = packet.payload();
    // 0x07 and 0xA7 carry a password or the redirect ticket
    const bool secret = PacketValidator::payloadIsSecret(op);
    for (size_t i = 0; !secret && i < std::min<size_t>(32, payload.size()); ++i) {
        if (i > 0) hexDump += " ";
        hexDump += toHex(payload[i]);
    }
    if (secret) hexDump = "redacted";
    else if (payload.size() > 32) hexDump += "...";

    LOG_DEBUG("GAME", "RECV from " + session->remoteAddress() + ": Opcode=0x" + toHex16(packet.opcode()) +
             " Size=" + std::to_string(packet.payloadSize()) + " Data=[" + hexDump + "]");

    // security gate then flood throttle then dispatch a failing packet is dropped never dispatched
    ValidationResult vr = PacketValidator::validate(packet);
    if (vr != ValidationResult::OK) {
        LOG_WARN("GAME", "drop invalid packet opcode 0x" + toHex16(packet.opcode()) +
                 " from " + session->remoteAddress() + " reason " + PacketValidator::resultToString(vr));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        auto rit = m_rateLimiters.find(session->id());
        if (rit != m_rateLimiters.end() && !rit->second.check(op)) {
            LOG_WARN("GAME", "rate limit drop opcode 0x" + toHex16(packet.opcode()) +
                     " from " + session->remoteAddress() +
                     " total dropped " + std::to_string(rit->second.getDroppedCount()));
            return;
        }
    }
    // before 0x07 or 0xA7 binds an account a socket reaches only the login frames
    if (session->accountId == 0 && !PacketValidator::allowedBeforeAuth(op)) {
        LOG_WARN("GAME", "drop opcode 0x" + toHex16(op) + " before login from " + session->remoteAddress());
        return;
    }
    if (isRetiredC2S(op, packet.payloadSize())) {
        LOG_WARN("GAME", "drop retired opcode 0x" + toHex16(op) + " size " +
                 std::to_string(packet.payloadSize()) + " from char " + std::to_string(session->characterId));
        return;
    }

    // a throwing handler used to unwind through the loop only this client may pay for it
    try {
    switch (op) {
        case CMD::C_HEARTBEAT:      handleHeartbeat(session, packet); break;
        case CMD::C_CLIENT_AUTH:    handleClientAuth(session, packet); break;
        case CMD::C_FULL_STATE:     handleFullState(session, packet); break;
        case CMD::C_CLIENT_INFO:
            ProgressionHandler::handleAstroPoll(session, packet, this);    handleClientInfo(session, packet); break;
        case CMD::S_SESSION_CONFIRM: handleSessionConfirm(session, packet); break;

        case CMD::C_SERVER_QUERY:   handleServerQuery(session, packet); break;

        // 0x2D is CREATE ROOM request not chat see OPCODE AUDIT md
        case CMD::C_CREATE_ROOM_REQ: handleCreateRoom(session, packet); break;
        // real client joins with 0x2F body roomId u32 then u16 0 our C JOIN ROOM 0x3F is never sent
        case 0x2F:
        case CMD::C_JOIN_ROOM:       handleJoinRoom(session, packet); break;
        case CMD::C_LEAVE_ROOM:      handleLeaveRoom(session, packet); break;
        // the ack closes the Loading box and runs sub 408020 which wipes the grid so rows must follow it
        case CMD::C_LOBBY_ENTER:
            leaveCurrentRoom(session);
            session->send(Packet(CMD::S_SHOW_LOBBY));
            sendLobbyRoomList(session);
            break;
        case CMD::C_PLAYER_READY:    handlePlayerReady(session, packet); break;
        // C2S 0x40 overloaded racing means motion sub 4818A0 else waiting room host start
        case CMD::C_MOTION: {
            // 0x40 carries both in race motion and waiting room start size is the discriminator a start request has no position
            auto room = getRoom(session->roomId);
            const bool looksLikeMotion = packet.payloadSize() >= 8;
            if (room && room->state() == RoomState::Racing) {
                m_raceHandler.handleMotion(session, packet, room.get());
            } else if (looksLikeMotion) {
                // waiting room the stock drives its kart on the room field the seat is relayed as is
                if (room) m_raceHandler.handleRoomMotion(session, packet, room.get());
            } else {
                handleGameStart(session, packet);
            }
            break;
        }

        // 0x35 both directions FUN 00479C20 track select was blind echoed with zero bytes client dropped the socket
        case CMD::S_ROOM_TRACK_SELECT: {
            const int32_t wantTrack = packet.remaining() >= 4 ? packet.readInt32() : 0;
            const int32_t subType   = packet.remaining() >= 4 ? packet.readInt32() : 0;
            auto room = getRoom(session->roomId);
            auto& db = Database::instance();
            auto hit = db.queryPrepared(
                "SELECT track_id, COALESCE(map_id, 0) AS map_id FROM track_catalog "
                "WHERE track_id = ? LIMIT 1", {wantTrack});
            if (hit.empty()) {
                LOG_WARN("ROOM", "track select " + std::to_string(wantTrack) +
                         " is in no track_catalog row, keeping the current one");
                if (room) {
                    const RoomTrackChoice keep = defaultRoomTrack(room->settings().mapId);
                    session->send(PacketBuilder::roomTrackSelect(keep.trackId, subType));
                }
                break;
            }
            const int32_t mapId = std::stoi(hit[0].at("map_id"));
            if (room && mapId > 0) {
                room->settings().mapId = static_cast<uint8_t>(mapId & 0xFF);
            }
            LOG_INFO("ROOM", "track select " + std::to_string(wantTrack) +
                     " map " + std::to_string(mapId) + " char " +
                     std::to_string(session->characterId));
            Packet ack = PacketBuilder::roomTrackSelect(wantTrack, subType);
            if (room) room->broadcast(ack); else session->send(ack);
            break;
        }

        // Quick Garage swap sub 47D540 C2S is 68 bytes character id kart id then the 0x3C custom car block
        case CMD::S_PLAYER_FULL_UPDATE: {
            const int32_t charInst = packet.remaining() >= 4 ? packet.readInt32() : 0;
            const int32_t kartInst = packet.remaining() >= 4 ? packet.readInt32() : 0;
            const int32_t me = static_cast<int32_t>(session->characterId);
            auto& db = Database::instance();

            auto own = db.queryPrepared(
                "SELECT base_key FROM owned_character WHERE id = ? AND character_id = ? LIMIT 1",
                {charInst, me});
            auto ok = db.queryPrepared(
                "SELECT id FROM owned_kart WHERE id = ? AND character_id = ? LIMIT 1",
                {kartInst, me});
            if (own.empty() || ok.empty()) {
                LOG_WARN("ROOM", "loadout swap char inst " + std::to_string(charInst) +
                         " kart inst " + std::to_string(kartInst) +
                         " not owned by char " + std::to_string(me));
                break;
            }

            db.executePrepared("UPDATE characters SET equipped_driver_id = ?, "
                               "selected_kart_instance_id = ? WHERE id = ?",
                               {std::stoi(own[0].at("base_key")), kartInst, me});

            RoomMemberWire m = buildRoomMember(me, session->displayName(), 0, 0, false);
            Packet upd = PacketBuilder::roomLoadoutUpdate(me, m.character, m.kart, m.customCar);
            auto room = getRoom(session->roomId);
            if (room) room->broadcast(upd); else session->send(upd);

            LOG_INFO("ROOM", "loadout swap char " + std::to_string(me) +
                     " driver inst " + std::to_string(charInst) +
                     " kart inst " + std::to_string(kartInst));
            break;
        }

        // Room Craft Save client sends 340 bytes a count then 48 byte records never routed before
        case CMD::C_ROOMCRAFT_SAVE: {
            std::vector<std::array<uint8_t, RoomCraftPackets::RECORD_SIZE>> raw;
            std::vector<RoomObjectInstance> recs;
            if (!RoomCraftPackets::parseSaveRequest(packet.payload(), raw, recs)) {
                LOG_WARN("ROOMCRAFT", "malformed save from char " +
                         std::to_string(session->characterId) + " size " +
                         std::to_string(packet.payloadSize()));
                break;
            }
            m_roomCraftHandler.handleSave(session, raw, recs, this);
            break;
        }

        // reversed from the client senders see docs PROTOCOL md

        // 0xA1 prop hit the hitter already ran its own knockdown so only fan out to others
        case CMD::C_PROP_HIT: {
            const int32_t propIndex = packet.remaining() >= 4 ? packet.readInt32() : -1;
            auto room = getRoom(session->roomId);
            if (room && propIndex >= 0) {
                Packet out(CMD::C_PROP_HIT);
                out.writeInt32(propIndex);
                room->broadcastExcept(out, session->id());
            }
            break;
        }

        // 0x121 waypoint reached no reply the client owns its own cursor
        case CMD::C_WAYPOINT_REACHED: {
            const int32_t idx = packet.remaining() >= 4 ? packet.readInt32() : -1;
            LOG_DEBUG("RACE", "waypoint " + std::to_string(idx) + " char " +
                      std::to_string(session->characterId));
            break;
        }

        // 0x125 overheat one byte edge triggered fan out with actor id skip the sender
        case CMD::C_OVERHEAT_STATE: {
            const uint8_t on = packet.remaining() >= 1 ? packet.readUInt8() : 0;
            auto room = getRoom(session->roomId);
            if (room) {
                Packet out = Packet::fromCmdFull(CMD::C_OVERHEAT_STATE);
                out.writeInt32(static_cast<int32_t>(session->characterId));
                out.writeUInt8(on ? 1 : 0);
                room->broadcastExcept(out, session->id());
            }
            break;
        }

        // 0x123 equip pendant one i32 key minus one takes it off the ack always carries the worn key
        case CMD::C_TITLE_EQUIP: {
            const int32_t want = packet.remaining() >= 4 ? packet.readInt32() : 0;
            const int32_t me = static_cast<int32_t>(session->characterId);
            auto& db = Database::instance();
            bool owned = false;
            if (want > 0) {
                auto own = db.queryPrepared(
                    "SELECT 1 AS ok FROM owned_pendant WHERE character_id = ? AND pendant_key = ? LIMIT 1",
                    {me, want});
                owned = !own.empty();
            }
            int32_t current = 0;
            {
                auto cur = db.queryPrepared(
                    "SELECT COALESCE(pendant_key, 0) AS p FROM characters WHERE id = ? LIMIT 1", {me});
                if (!cur.empty()) current = std::stoi(cur[0].at("p"));
            }
            const int32_t applied = pendantEquipApplied(want, owned, current);
            if (applied != current) {
                db.executePrepared("UPDATE characters SET pendant_key = ? WHERE id = ?", {applied, me});
            }
            if (want > 0 && !owned) {
                LOG_WARN("PENDANT", "char " + std::to_string(me) + " asked for pendant " +
                         std::to_string(want) + " it does not own");
            }
            // sub 47EAE0 clears the popup lock only on this answer so it goes out on every request
            Packet ack = Packet::fromCmdFull(CMD::C_TITLE_EQUIP);
            ack.writeInt32(applied);
            session->send(ack);
            break;
        }

        // 0x118 waiting room padlock kind 0 goes to the room kind 1 to the lobby
        case CMD::C_ROOM_PASSWORD: {
            const int32_t roomNo = packet.remaining() >= 4 ? packet.readInt32() : 0;
            const int32_t mode   = packet.remaining() >= 4 ? packet.readInt32() : 0;
            // the client edit buffer is wchar t 10 nine characters plus the NUL
            std::u16string pw    = packet.remaining() >= 2 ? packet.readWString(9) : u"";
            auto room = getRoom(session->roomId);
            if (!room || static_cast<int32_t>(room->id()) != roomNo) {
                LOG_WARN("ROOM", "password change for room " + std::to_string(roomNo) +
                         " from a char who is not in it");
                break;
            }
            if (room->hostSessionId() != session->id()) {
                LOG_WARN("ROOM", "password change refused, char " +
                         std::to_string(session->characterId) + " is not the master");
                break;
            }
            std::string ascii;
            for (char16_t c : pw) if (c > 0 && c < 128) ascii += static_cast<char>(c);
            room->settings().password = (mode == 1) ? ascii : std::string();
            room->settings().isPrivate = (mode == 1 && !ascii.empty());

            Packet inRoom = Packet::fromCmdFull(CMD::C_ROOM_PASSWORD);
            inRoom.writeInt32(0);                       // kind 0 my room padlock
            inRoom.writeInt32(roomNo);
            inRoom.writeInt32(room->settings().isPrivate ? 1 : 0);
            room->broadcast(inRoom);

            Packet inLobby = Packet::fromCmdFull(CMD::C_ROOM_PASSWORD);
            inLobby.writeInt32(1);                      // kind 1 the room list row
            inLobby.writeInt32(roomNo);
            inLobby.writeInt32(room->settings().isPrivate ? 1 : 0);
            for (const auto& s2 : getLobbySessions()) {
                if (s2->id() != session->id()) s2->send(inLobby);
            }

            LOG_INFO("ROOM", "room " + std::to_string(roomNo) + " lock " +
                     std::to_string(room->settings().isPrivate ? 1 : 0));
            break;
        }

        // 0x12F Random Invite exe gates on room not full FUN 00410420 no quota FUN 00410640 clears box after 5000 ms
        case CMD::C_RANDOM_INVITE: {
            auto room = getRoom(session->roomId);
            if (!room) break;
            if (room->hostSessionId() != session->id()) {
                LOG_WARN("ROOM", "random invite from a char who is not the master");
                break;
            }
            if (room->playerCount() >= room->settings().maxPlayers) {
                LOG_INFO("ROOM", "random invite ignored, room " +
                         std::to_string(room->id()) + " is full");
                break;
            }
            // ask a human first S2C 0x12F opens the accept box and its accept sends C2S 0x2F join
            Session::Ptr invitee = pickRandomInviteTarget(session, room.get());
            if (invitee) {
                invitee->send(PacketBuilder::invitePopupShort(
                    1, session->characterName, static_cast<int32_t>(room->id()),
                    widenAscii(room->settings().password)));
                m_randomInviteDue[room->id()] =
                    static_cast<int64_t>(RaceHandler::nowMs()) + kRandomInviteWaitMs;
                LOG_INFO("ROOM", "random invite asked char " +
                         std::to_string(invitee->characterId) + " into room " +
                         std::to_string(room->id()));
                break;
            }
            // nobody to ask so the waiting box gets a CPU car right away
            seatOneBot(room.get(), "random invite");
            break;
        }

        // 0x33 START and READY the client already validated alone and caps so a 1 from master is a real start
        case CMD::C_ROOM_READY_TOGGLE: {
            const int32_t pressed = packet.remaining() >= 4 ? packet.readInt32() : 0;
            auto room = getRoom(session->roomId);
            if (!room) break;
            const bool isHost = room->hostSessionId() == session->id();
            if (isHost) {
                if (pressed == 0) break;   // master un pressed nothing to do
                if (const char* why = room->startRefusal()) {
                    LOG_WARN("ROOM", std::string("start refused ") + why + " in room " +
                             std::to_string(room->id()));
                    session->send(ShopPackets::rejectAscii(why, 1));
                    break;
                }
                LOG_INFO("ROOM", "master " + std::to_string(session->characterId) +
                         " starts room " + std::to_string(room->id()));
                m_raceHandler.handleStartRace(session, room.get(), this);
            } else {
                RoomPlayer* rp = room->getPlayer(session->id());
                if (rp) rp->ready = (pressed != 0);
                Packet echo(CMD::C_ROOM_READY_TOGGLE);
                echo.writeInt32(static_cast<int32_t>(session->characterId));
                echo.writeInt32(pressed);
                room->broadcast(echo);
                LOG_INFO("ROOM", "char " + std::to_string(session->characterId) +
                         " ready " + std::to_string(pressed));
            }
            break;
        }

        // 0x6E is a join carrying the same payload as 0x2F but arrives from an invite
        case CMD::C_INVITE_JOIN:     handleJoinRoom(session, packet); break;

        // 0x2F is not the whisper C WHISPER SEND 0xB5 is the real one this dead case was swallowing it
        case CMD::C_LOBBY_CHAT:      m_socialHandler.handleChatSend(session, packet, this); break;
        case CMD::C_WHISPER_SEND:    m_socialHandler.handleWhisperSend(session, packet, this); break;

        case CMD::C_STATE_CHANGE:
            // 0x2C kind 1 sub 0 is the ghost record board not a state change
            if (GhostHandler::isBoardRequest(packet)) GhostHandler::handleMenuSelect(session, packet, this);
            else handleStateChange(session, packet);
            break;
        // C POSITION 0x31 dead route removed client never sends it S2C only minimap
        case CMD::C_RACE_FINISH:    handleRoomKick(session, packet); break;   // 0x39 host kick 0x37 0x38 0x45 dead routes removed no client sender exists 0x45 old grant reply scrambled rank table

        // in race item wire see ItemPackets h for the proven C2S senders
        case 0x47: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleItemSpawn(session, packet, r.get()); break; }
        case 0x49: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleItemGrantReport(session, packet, r.get()); break; }
        case 0x4B: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleHomingLaunch(session, packet, r.get()); break; }
        case 0x57: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleLockState(session, packet, r.get()); break; }
        case 0x5C: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleTurtleLaunch(session, packet, r.get()); break; }
        case 0x5F: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handlePetReached(session, packet, r.get()); break; }
        case 0x69: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleHitReport(session, packet, r.get()); break; }
        case 0x6A: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleRaceValue(session, packet, r.get()); break; }
        case 0xCF: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleSlotSync(session, packet, r.get()); break; }

        // ten client senders that used to fall through unhandled see docs packets UNROUTED TEN md
        case CMD::C_NICK_QUERY: {
            // echoed by sub 479AB0 into a 14 wchar stack buffer with no bound so a long name overruns the frame
            const std::u16string nick = packet.readWString().substr(0, 13);
            Packet r(CMD::S_ROOM_STRING);
            r.writeWString(nick).writeString("").writeInt32(0);
            session->send(r);
            break;
        }
        case CMD::C_EXT_DATA_132: {
            // 0x84 note mark read u32 id the echo greys the row in sub 47B8D0
            const uint32_t id = packet.remaining() >= 4 ? packet.readUInt32() : 0;
            m_socialHandler.handleNoteMarkRead(session, id);
            break;
        }
        case CMD::C_EXT_DATA_133: {
            // 0x85 note delete u32 id the echo compacts the inbox in sub 47B900
            const uint32_t id = packet.remaining() >= 4 ? packet.readUInt32() : 0;
            m_socialHandler.handleNoteDelete(session, id);
            break;
        }
        case CMD::C_GIFT_ACTION: {
            const int32_t id = packet.remaining() >= 4 ? packet.readInt32() : 0;
            Packet r(static_cast<uint16_t>(op));
            r.writeInt32(id);
            session->send(r);
            break;
        }
        case CMD::C_ENTITY_LIST_276: {
            // 0x114 carcraft preset rename i32 preset id then an ascii name was an unread stub that acked with zero
            CarRenameRequest req = CustomCarPackets::parseRenameRequest(packet);
            m_carCraftHandler.handleRename(session, req, this);
            break;
        }
        case CMD::C_QUEST_CLAIM:
            // 0xF8 scenario result report sub 42E330 eight bytes client waits in state 2013 and never times out so always reply
            ScenarioHandler::handleScenarioResultReport(session, packet, this);
            break;
        // no client handler for these two so a reply would be dropped anyway
        case CMD::C_NICK_SELECT:
        case CMD::C_NICK_ACTION:
            break;
        case CMD::C_ENTITY_REQ_242: {
            // 0xF2 ability class 8 9 10 11 parsed by ItemHandler class 10 and 11 also broadcast S2C 0xCD shield absorb
            auto r = getRoom(session->roomId);
            if (r) ItemHandler::handleAbilityClass(session, packet, r.get());
            break;
        }

        // shop enter client sends 0x10 sub 4806B0 push all catalogs
        case CMD::C_SHOP_ENTER:     m_shopHandler.handleEnterShop(session, this); break;

        // shop and garage MSG WAIT is up every path must answer
        case CMD::C_BUY: {
            ShopPackets::BuyRequest r;
            if (ShopPackets::parseBuy(packet, r)) m_shopHandler.handleBuy(session, r, this);
            break;
        }
        case CMD::C_SELL: {
            // inventory claims category 4 pets  shop runs for everything else
            if (m_inventoryHandler.handleDelete(session, packet, this)) break;
            ShopPackets::SellRequest r;
            if (ShopPackets::parseSell(packet, r)) m_shopHandler.handleSell(session, r, this);
            break;
        }
        case CMD::C_EXTEND: {
            ShopPackets::ExtendRequest r;
            if (ShopPackets::parseExtend(packet, r)) m_shopHandler.handleExtend(session, r, this);
            break;
        }

        // mission and licence screen acks need their data first
        case CMD::C_MISSION_MENU_OPEN:
            if (MissionPackets::parseMissionMenuOpen(packet))
                MissionHandler::handleOpenMissionMenu(session, this);
            break;
        case CMD::C_LICENSE_SCREEN_OPEN:
            if (MissionPackets::parseLicenseScreenOpen(packet))
                LicenseHandler::handleOpenLicenseScreen(session, this);
            break;
        case CMD::C_LICENSE_PANEL_CLOSE: {
            MissionPackets::PanelCloseReq r;
            MissionPackets::parseLicensePanelClose(packet, r);
            break;
        }

        // 16 bit stage opens with no data builder yet echo keeps the ui alive
        case CMD::C_STAGE22_OPEN:
            // quest mode the 0x00F4 rows then the 0x011C ack that pushes stage 22 the ScenarioMenu
            ScenarioHandler::handleMenuOpen(session, this);
            break;
        case CMD::C_STAGE23_OPEN:
            // the record board must be sent before the screen opens or every rank row draws dashes
            GhostHandler::sendRecordBoard(session, false, this);
            session->send(Packet::fromCmdFull(0x011D));
            break;

        // messenger verified see ARBITRATION 2026-08-17 json
        case CMD::C_ROOM_INVITE:
        case CMD::C_FRIEND_ADD:
        case CMD::C_BLOCK_ADD:
        case CMD::C_SMALLTALK_REQ: {
            std::u16string name;
            if (SocialPackets::parseNameRequest(packet, name))
                m_socialHandler.handleNameRequest(session, op, name, this);
            break;
        }
        case CMD::C_FRIEND_REQ_ACCEPT:
        case CMD::C_FRIEND_REQ_REJECT:
        case CMD::C_FRIEND_DEL:
        case CMD::C_BLOCK_DEL:
        case CMD::C_SMALLTALK_ACCEPT:
        case CMD::C_SMALLTALK_DECLINE:
        case CMD::C_SMALLTALK_CLOSE: {
            uint32_t playerId = 0;
            if (SocialPackets::parseIdRequest(packet, playerId))
                m_socialHandler.handleIdRequest(session, op, playerId, this);
            break;
        }
        case CMD::C_ROOM_INVITE_ANSWER: {
            SocialPackets::RoomInviteAnswer a;
            if (SocialPackets::parseRoomInviteAnswer(packet, a))
                m_socialHandler.handleRoomInviteAnswer(session, a, this);
            break;
        }
        case CMD::C_USERINFO_BY_NAME: {
            std::u16string name;
            if (SocialPackets::parseUserInfoReqByName(packet, name))
                m_socialHandler.handleUserInfoByName(session, name, this);
            break;
        }
        case CMD::C_FRIEND_STATUS_POLL:
            if (SocialPackets::parseFriendStatusPoll(packet))
                m_socialHandler.handleStatusPoll(session, this);
            break;
        case CMD::C_NOTE_SEND: {
            SocialPackets::NoteSend n;
            if (SocialPackets::parseNoteSend(packet, n))
                m_socialHandler.handleNoteSend(session, n, this);
            break;
        }
        case CMD::C_USERLIST_PAGE: {
            uint32_t page = 0;
            if (SocialPackets::parseUserListPageReq(packet, page))
                m_socialHandler.handleUserListPage(session, page, this);
            break;
        }
        case CMD::C_USERINFO_BY_ID: {
            uint32_t playerId = 0;
            if (SocialPackets::parseUserInfoReqById(packet, playerId))
                m_socialHandler.handleUserInfoById(session, playerId, this);
            break;
        }

        // 16 bit reachable only since the switch widened to u16
        case CMD::C_CARCRAFT_OPEN:
            m_carCraftHandler.handleOpen(session, this);
            break;
        case CMD::C_ROOMCRAFT_OPEN:
            m_roomCraftHandler.handleOpen(session, this);
            break;
        case CMD::C_CARCRAFT_SAVE: {
            CarSaveRequest r = CustomCarPackets::parseSaveRequest(packet);
            m_carCraftHandler.handleSave(session, r, this);
            break;
        }
        // 0x10F roomcraft save is routed above as C ROOMCRAFT SAVE through parseSaveRequest

        // not shop opcodes dword 1A5CA48 dword 1A5BC30 and sub 4820F0 prove a messenger list see SocialPackets
        case CMD::C_SHOP_GIFT: {
            ShopPackets::GiftRequest r;
            if (ShopPackets::parseGift(packet, r)) m_shopHandler.handleGift(session, r, this);
            break;
        }

        // equip moved to garage install 0xB9 see EQUIP VS MISSION 0x8C md
        case CMD::C_USE_ITEM:         handleUseItem(session, packet); break;

        case CMD::C_LOBBY_TELEMETRY: {
            // C2S 0x0130 is client option 11 as a float zero keeps invites anything else drops them
            const float opt = packet.remaining() >= 4 ? packet.readFloat() : 0.0f;
            const bool optOut = inviteOptOutFromOption11(opt);
            if (session->inviteOptOut.exchange(optOut) != optOut && session->accountId != 0) {
                Database::instance().executePrepared(
                    "UPDATE accounts SET invite_opt_out = ? WHERE id = ?",
                    {optOut ? 1 : 0, static_cast<int32_t>(session->accountId)});
                LOG_INFO("ROOM", "invite option " + std::to_string(optOut ? 1 : 0) +
                         " stored for account " + std::to_string(session->accountId));
            }
            break;
        }
        case CMD::C_RACE_GAUGE: {
            // pure relay sub 47AD60 wants the player id then the float silence here kills the connection mid race
            const float v = packet.remaining() >= 4 ? packet.readFloat() : 0.0f;
            Packet echo(CMD::S_RACE_GAUGE);
            echo.writeInt32(static_cast<int32_t>(session->characterId));
            echo.writeFloat(v);
            auto room = getRoom(session->roomId);
            if (room) room->broadcast(echo); else session->send(echo);
            break;
        }
        case CMD::C_PRACTICE_START: {
            // license START empty both ways sub 479580 closes the modal so the reply carries no payload
            LOG_INFO("LICENSE", "practice start from char " +
                     std::to_string(session->characterId) + " entering tutorial stage");
            session->send(Packet(CMD::S_ENTER_TUTORIAL));
            // the tutorial stage puts a car on a track so it needs the camera too
            session->send(SpawnPackets::cameraMode(0));
            break;
        }
        case CMD::C_GACHA_ROLL:       GachaHandler::handleRoll(session, packet, this); break;
        // C2S 0xAA is only GhostEnter the 12 byte tutorial result paid gold on every send and is retired
        case CMD::C_TUTORIAL_COMPLETE: GhostHandler::handleGhostEnter(session, packet, this); break;

        // real mission C2S opcodes see MISSION PROTOCOL VERIFIED md 0x8E menu enter is 0 bytes
        case CMD::C_MISSION_COMPLETE: {
            MissionPackets::MissionIdReq r;
            if (MissionPackets::parseMissionComplete(packet, r))
                MissionHandler::handleMissionComplete(session, r, this);
            break;
        }
        // real start is 0x90 sub 4835C0 0x8D was wrong and is now a panel close
        case CMD::C_MISSION_START: {
            MissionPackets::MissionIdReq r;
            if (MissionPackets::parseMissionStart(packet, r))
                MissionHandler::handleStartMission(session, r, this);
            break;
        }
        case CMD::C_MISSION_MENU:
            // 0x8E is the tail of every lobby scenario and mission stage init chibikart answers nothing so nothing goes back
            break;
        // 0xA3 is license completion not mission claim mission claim C2S opcode unreversed stages 24 25
        case CMD::C_LICENSE_COMPLETE: LicenseHandler::handleLicenseComplete(session, packet, this); break;

        case CMD::C_OPEN_GARAGE:      GarageHandler::handleOpenGarage(session, packet, this); break;
        case CMD::C_UPGRADE_VEHICLE: {
            // 0x9C gift mark as read i32 gift id sub 482B00 must echo back so sub 47C2A0 flags the row read
            const int32_t giftId = packet.remaining() >= 4 ? packet.readInt32() : 0;
            if (giftId > 0 && session->characterId != 0) {
                Database::instance().executePrepared(
                    "UPDATE gift_log SET read_flag = 1 WHERE id = ? AND target_id = ?",
                    {std::to_string(giftId), std::to_string(session->characterId)});
                Packet r(CMD::S_GIFT_MARK_READ);
                r.writeInt32(giftId);
                session->send(r);
            }
            break;
        }
        // real client garage opcodes see GARAGE ACTIONS VERIFIED md repair install action 2 not its own opcode
        case CMD::C_GARAGE_INSTALL:
            if (!m_inventoryHandler.handleInstall(session, packet, this))
                GarageHandler::handleGarageInstall(session, packet, this);
            break;
        case CMD::C_GARAGE_REMOVE:
            if (!m_inventoryHandler.handleRemove(session, packet, this))
                GarageHandler::handleGarageRemove(session, packet, this);
            break;
        // 0xB8 is C SELL now routed in the shop block cat 5 needs an instance uid not a base key

        // 0x64 overloaded in room means team change else lobby quick match
        case CMD::C_QUICK_MATCH: {
            if (session->roomId != 0) {
                handleTeamChange(session, packet);
            } else {
                LobbyHandler::handleQuickMatch(session, packet, this);
            }
            break;
        }

        case CMD::C_ADD_FRIEND:
            if (GhostHandler::isSubmit(session, packet)) GhostHandler::handleSubmit(session, packet, this);
            else LobbyHandler::handleAddFriend(session, packet, this);
            break;
        case CMD::C_REMOVE_FRIEND:
            // empty payload is stage begin  the friend opcode always carries an int32
            if (GhostHandler::isStageEvent(packet)) GhostHandler::handleStageBegin(session, packet, this);
            else LobbyHandler::handleRemoveFriend(session, packet, this);
            break;
        case CMD::C_BLOCK_PLAYER:
            if (GhostHandler::isStageEvent(packet)) GhostHandler::handleFinalLap(session, packet, this);
            else LobbyHandler::handleBlockPlayer(session, packet, this);
            break;
        case CMD::C_PLAYER_PROFILE:   LobbyHandler::handlePlayerProfile(session, packet, this); break;



        // real ghost upload opcodes 0xC0 to 0xC6 were fabricated and never sent
        case 0x00F5:
            // quest mode sub 437F30 is the only sender the handler sends the rival ghost then S2C 0xF5
            ScenarioHandler::handleScenarioStageSelect(session, packet, this);
            break;
        case GhostPackets::kOpReplayCount: GhostHandler::handleUploadCount(session, packet, this); break;
        case GhostPackets::kOpReplayChunk: GhostHandler::handleUploadChunk(session, packet, this); break;
        case 0x00FE: QuestHandler::handleAccept(session, packet, this); break;
        case 0x0100: QuestHandler::handleDiscard(session, packet, this); break;
        case 0x0102: QuestHandler::handleProgressReport(session, packet, this); break;

        // raw hex on purpose CMD S names here are S2C and misleading 0x0D scene loaded empty GO waits on it
        case 0x04: CharCreateHandler::handleCreateCharacter(session, packet, this); break;
        case 0x0D: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleSceneLoaded(session, r.get(), this); break; }
        case 0x41: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleCheckpoint(session, packet, r.get(), this); break; }
        case 0x67: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleProgress(session, packet, r.get()); break; }
        case 0x68: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleRespawn(session, packet, r.get()); break; }
        case 0x58: { auto r = getRoom(session->roomId); if (r) m_raceHandler.handleAnimState(session, packet, r.get()); break; }
        case 0x3B: {
            // C2S LeaveRace empty body sub 4810C0 race bookkeeping must run before the player leaves the room
            auto r = getRoom(session->roomId);
            const bool midRace = r && r->state() != RoomState::Waiting;
            if (r) m_raceHandler.handlePlayerLeave(r.get(), session->characterId, this);
            leaveCurrentRoom(session);
            // FUN 004810C0 only writes and waits so sub 4795A0 the lobby ack is what moves the client past exit
            if (midRace) {
                session->send(Packet(CMD::S_SHOW_LOBBY));
                sendLobbyRoomList(session);
            }
            break;
        }

        case CMD::C_SCENARIO_COMPLETE: {
            // 0xCB shared three ways split by size
            auto swapRoom = getRoom(session->roomId);
            // the item 1000 row comes from the lobby and room inits too so no room is needed
            const auto& swapBytes = packet.payload();
            if (packet.payloadSize() == ItemPackets::kSwapTicketC2SSize && swapBytes.size() >= 8 &&
                (swapBytes[4] | (swapBytes[5] << 8) | (swapBytes[6] << 16) | (swapBytes[7] << 24)) ==
                    ItemPackets::kSwapTicketTemplate) {
                m_raceHandler.handleSwapTicket(session, packet, swapRoom.get());
                break;
            }
            // a frame under 20 bytes never gets here isRetiredC2S drops it
            m_inventoryHandler.handleItemStateNotify(session, packet, this);
            break;
        }
        case CMD::C_SCENARIO_PROGRESS:
            // 0xCC is KartPartUseNotify at lobby and room entry the part wear is client local and wants no answer
            LOG_DEBUG("GAME", "kart part use notify size " + std::to_string(packet.payloadSize()) +
                      " char " + std::to_string(session->characterId));
            break;
        case CMD::C_SCENARIO_CHAPTERS: {
            // sub 483180 0xCD is the ability ack it means nothing outside a race
            auto r = getRoom(session->roomId);
            if (r && r->state() == RoomState::Racing) ItemHandler::handleAbilityFire(session, packet, r.get());
            break;
        }

        case CMD::C_REQUEST_DATA:
            handleRequestData(session, packet);
            break;
        case CMD::C_UNKNOWN_32:
            handleUnknown32(session, packet);
            break;

        case CMD::C_CHANNEL_SELECT:
            handleChannelSelect(session, packet);
            break;

        // 0x8E now mission menu handled above old C ACK guess was wrong
        case 0x0B:
            break;
        case 0x4E:
            // the client answers an armed S2C 0x4E fifteen seconds later via FUN 00485290
            KeepaliveAnticheatHandler::handleDelayedAckFire(session, packet);
            break;

        // 0x73 was wrong C DISCONNECT now C SHOP POLL real disconnect is a TCP close

        default: {
            // classification tables generated by ida gen cpp lists py regenerate when the IDB updates see docs OPCODE AUDIT md
            static const std::unordered_set<uint8_t> MIRROR_UI = {
                // 0x35 0x33 0x04 removed routed above their zero payload echoes read past sub 479C20 and sub 479230
                0x11, 0x12, 0x16, 0x25,
                0x39, // 0x58 echo reads 5 bytes off the end
                0x65, 0x6C,
                // 0x47 0x69 0x6A 0x6F 0x98 removed a zero payload echo makes the client read past the frame and crash
                0x7D, 0x81, 0x84, 0x85, 0x90, 0x9B,
                // opcodes removed zero payload echo made sub 47D540 sub 47D5E0 read past frame routed above plus 0xF5 quest ghost crash
                0xF8,
                // 0x49 0x4B 0x57 0x5C 0x5F 0xCF removed their handlers read bytes a zero payload echo breaks the parse
            };
            static const std::unordered_set<uint8_t> DATA_REQ = {
                0x26, 0x29, 0x2C, 0x6D, 0x7E,   // 0x2F is the real join routed above
                0x7F, 0x80, 0xF2,
            };
            // nothing is ever silent an unhandled opcode is logged with its full payload so the wire stays recoverable
            std::string full;
            for (size_t i = 0; i < payload.size(); ++i) {
                if (i) full += " ";
                full += toHex(payload[i]);
            }

            if (op >= 0x100) {
                // never blind echo a 16 bit opcode several handlers read fields and a zero payload reads past the frame end
                LOG_WARN("UNHANDLED", "NOT HANDLED C2S 0x" + toHex16(op) +
                         " size=" + std::to_string(packet.payloadSize()) +
                         " payload=[" + full + "] reason=no16bitRoute");
            } else if (MIRROR_UI.count(cmd)) {
                Packet reply(cmd);
                session->send(reply);
                LOG_WARN("UNHANDLED", "NOT HANDLED C2S 0x" + toHex16(op) +
                         " size=" + std::to_string(packet.payloadSize()) +
                         " payload=[" + full + "] reason=blindEchoNoSystem");
            } else if (DATA_REQ.count(cmd)) {
                LOG_WARN("UNHANDLED", "NOT HANDLED C2S 0x" + toHex16(op) +
                         " size=" + std::to_string(packet.payloadSize()) +
                         " payload=[" + full + "] reason=swallowedNoSystem");
            } else {
                LOG_ERROR("UNHANDLED", "NOT HANDLED C2S 0x" + toHex16(op) +
                          " size=" + std::to_string(packet.payloadSize()) +
                          " payload=[" + full + "] reason=unknownOpcode");
            }
            break;
        }
    }
    } catch (const std::exception& e) {
        LOG_ERROR("GAME", "handler threw on opcode 0x" + toHex16(op) +
                  " account " + std::to_string(session->accountId) +
                  " char " + std::to_string(session->characterId) + ": " + e.what());
        session->stop();
    } catch (...) {
        LOG_ERROR("GAME", "handler threw an unknown error on opcode 0x" + toHex16(op) +
                  " account " + std::to_string(session->accountId) +
                  " char " + std::to_string(session->characterId));
        session->stop();
    }
}

}  // namespace knc
