/**
 * @file Types.h
 * @brief Core type definitions for Kart N'Chibi
 * 
 * Common types used across client, server, and tools.
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace KnC {

// ============================================================================
// Integer Types
// ============================================================================

using int8   = std::int8_t;
using int16  = std::int16_t;
using int32  = std::int32_t;
using int64  = std::int64_t;

using uint8  = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using byte = uint8;

// ============================================================================
// Floating Point Types
// ============================================================================

using float32 = float;
using float64 = double;

// ============================================================================
// Size Types
// ============================================================================

using size_t = std::size_t;
using usize = std::size_t;
using isize = std::ptrdiff_t;

// ============================================================================
// Game-Specific Types
// ============================================================================

/// Player ID (unique identifier)
using PlayerId = uint32;

/// Room ID
using RoomId = uint32;

/// Item ID
using ItemId = uint32;

/// Vehicle ID
using VehicleId = uint32;

/// Track ID
using TrackId = uint32;

/// Session ID
using SessionId = uint64;

/// Timestamp (milliseconds)
using Timestamp = uint64;

// ============================================================================
// Constants
// ============================================================================

constexpr PlayerId INVALID_PLAYER_ID = 0;
constexpr RoomId INVALID_ROOM_ID = 0;
constexpr ItemId INVALID_ITEM_ID = 0;
constexpr VehicleId INVALID_VEHICLE_ID = 0;
constexpr TrackId INVALID_TRACK_ID = 0;
constexpr SessionId INVALID_SESSION_ID = 0;

// ============================================================================
// Limits
// ============================================================================

namespace Limits {
    constexpr uint32 MAX_PLAYERS_PER_ROOM = 12;
    constexpr uint32 MAX_PLAYER_NAME_LENGTH = 32;
    constexpr uint32 MAX_ROOM_NAME_LENGTH = 64;
    constexpr uint32 MAX_CHAT_MESSAGE_LENGTH = 256;
    constexpr uint32 MAX_PACKET_SIZE = 65535;
}

} // namespace KnC

