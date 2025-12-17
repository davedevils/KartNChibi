/**
 * @file Player.cpp
 * @brief Player implementation
 */

#include "game/Player.h"
#include "net/Packet.h"

namespace knc {

Player::Player(uint32_t id, const std::string& name)
    : m_id(id), m_name(name)
{
}

void Player::serializeToPacket(Packet& pkt) const {
    pkt.writeUInt32(m_id);
    pkt.writeString(m_name);
    pkt.writeUInt32(m_stats.level);
    pkt.writeUInt32(m_stats.experience);
    pkt.writeUInt32(m_stats.wins);
    pkt.writeUInt32(m_stats.losses);
    pkt.writeUInt32(m_currency.gold);
    pkt.writeUInt32(m_currency.cash);
}

void Player::deserializeFromPacket(Packet& pkt) {
    m_id = pkt.readUInt32();
    m_name = pkt.readString();
    m_stats.level = pkt.readUInt32();
    m_stats.experience = pkt.readUInt32();
    m_stats.wins = pkt.readUInt32();
    m_stats.losses = pkt.readUInt32();
    m_currency.gold = pkt.readUInt32();
    m_currency.cash = pkt.readUInt32();
}

} // namespace knc

