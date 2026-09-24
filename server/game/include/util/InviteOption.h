/// client option 11 of C2S 0x0130 and the rule the random invite picker follows

#pragma once

#include <cstdint>

#include "net/Session.h"

namespace knc {

/// the wire carries a float sub 483C10 treats anything but zero as invites off
inline bool inviteOptOutFromOption11(float value) { return value != 0.0f; }

/// a session may be asked into a room only when it is another logged in player who takes invites
inline bool inviteCandidate(const Session& s, uint32_t askerSessionId, uint32_t roomId) {
    if (s.id() == askerSessionId) return false;
    if (s.characterId == 0) return false;
    if (s.roomId == roomId) return false;
    if (s.inviteOptOut.load()) return false;
    return true;
}

}  // namespace knc
