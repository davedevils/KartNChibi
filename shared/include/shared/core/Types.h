
#pragma once

#include <cstdint>
#include <cstddef>

namespace KnC {

using int8   = std::int8_t;
using int16  = std::int16_t;
using int32  = std::int32_t;
using int64  = std::int64_t;

using uint8  = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using byte = uint8;

using float32 = float;
using float64 = double;

using size_t = std::size_t;
using usize = std::size_t;
using isize = std::ptrdiff_t;

/// player id unique identifier
using PlayerId = uint32;

using RoomId = uint32;
using ItemId = uint32;
using VehicleId = uint32;
using TrackId = uint32;
using SessionId = uint64;

/// timestamp in milliseconds
using Timestamp = uint64;

constexpr PlayerId INVALID_PLAYER_ID = 0;
constexpr RoomId INVALID_ROOM_ID = 0;
constexpr ItemId INVALID_ITEM_ID = 0;
constexpr VehicleId INVALID_VEHICLE_ID = 0;
constexpr TrackId INVALID_TRACK_ID = 0;
constexpr SessionId INVALID_SESSION_ID = 0;

namespace Limits {
    constexpr uint32 MAX_PLAYERS_PER_ROOM = 12;
    constexpr uint32 MAX_PLAYER_NAME_LENGTH = 32;
    constexpr uint32 MAX_ROOM_NAME_LENGTH = 64;
    constexpr uint32 MAX_CHAT_MESSAGE_LENGTH = 256;
    constexpr uint32 MAX_PACKET_SIZE = 65535;
}

} // namespace KnC

