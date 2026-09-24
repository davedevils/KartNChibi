/// client option 11 of C2S 0x0130 the random invite never reaches a player who turned invites off

#include <gtest/gtest.h>
#include <asio.hpp>
#include <memory>
#include <vector>

#include "net/Session.h"
#include "util/InviteOption.h"

using namespace knc;
using asio::ip::tcp;

namespace {

Session::Ptr makeSession(asio::io_context& io, uint32_t characterId, uint32_t roomId,
                         bool optOut) {
    auto s = std::make_shared<Session>(tcp::socket(io));
    s->characterId = characterId;
    s->roomId = roomId;
    s->inviteOptOut.store(optOut);
    return s;
}

}  // namespace

// the wire carries a float and sub 483C10 treats anything but zero as invites off
TEST(InviteOption, ZeroKeepsInvitesAnythingElseDropsThem) {
    EXPECT_FALSE(inviteOptOutFromOption11(0.0f));
    EXPECT_TRUE(inviteOptOutFromOption11(1.0f));
    EXPECT_TRUE(inviteOptOutFromOption11(-1.0f));
    EXPECT_TRUE(inviteOptOutFromOption11(0.5f));
}

// a logged in player in the lobby is the candidate the picker wants
TEST(InviteOption, ALobbyPlayerIsACandidate) {
    asio::io_context io;
    auto asker = makeSession(io, 7, 4, false);
    auto other = makeSession(io, 9, 0, false);
    EXPECT_TRUE(inviteCandidate(*other, asker->id(), 4));
}

// the whole point a player who reported option 11 must never see the popup
TEST(InviteOption, AnOptedOutPlayerIsNeverAsked) {
    asio::io_context io;
    auto asker = makeSession(io, 7, 4, false);
    auto other = makeSession(io, 9, 0, true);
    EXPECT_FALSE(inviteCandidate(*other, asker->id(), 4));
}

// the asker its own room mates and a session with no character stay out
TEST(InviteOption, TheAskerItsRoomAndAHalfLoggedSessionStayOut) {
    asio::io_context io;
    auto asker = makeSession(io, 7, 4, false);
    auto sameRoom = makeSession(io, 8, 4, false);
    auto noChar = makeSession(io, 0, 0, false);
    EXPECT_FALSE(inviteCandidate(*asker, asker->id(), 4));
    EXPECT_FALSE(inviteCandidate(*sameRoom, asker->id(), 4));
    EXPECT_FALSE(inviteCandidate(*noChar, asker->id(), 4));
}
