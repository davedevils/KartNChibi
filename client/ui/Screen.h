// the screen base every state of the client derives from
#pragma once

#include "SpriteBatch.h"

namespace KnC::Client {

// what the session tells a screen the payloads sit on the session itself
enum class SessionEvent {
    ChannelList, Profile, MenuAck, LobbyAck, RoomsChanged, Chat, CharacterCreate, Disconnected,
    RoomEnter, RoomChanged, RoomTrack, RaceLaunch, GarageAck, ShopAck, InventoryChanged, BuyOk,
    CharacterCreated, LicenseAck, MissionMenu, MissionStart, MissionGo, MissionComplete, SocialChanged,
    RoomInvite, GachaResult, GiftOk, GhostMenuAck, GhostSession, GhostResult, LicenceTestAck, LicenceTestResult,
    RoomCraftAck, RoomCraftSaved, CarCraftAck, CarCraftSaved, PendantChanged
};

class Screen {
public:
    virtual ~Screen() = default;
    virtual const char* name() const = 0;
    virtual void enter() {}
    virtual void leave() {}
    virtual void update(float dt) { (void)dt; }
    // 3D pass before sprites top screen gets it first true when drawn false leaves it to the screen under
    virtual bool drawScene() { return false; }
    // another screen uploaded its own scene since this one drew so its upload is gone
    virtual void sceneLost() {}
    // the canvas rect a popup draws its scene in the sprites of the screens under it leave it open
    virtual bool sceneHole(float& x, float& y, float& w, float& h) const { (void)x; (void)y; (void)w; (void)h; return false; }
    virtual void draw(SpriteBatch& batch) { (void)batch; }
    virtual void onKey(int key, int action, int mods) { (void)key; (void)action; (void)mods; }
    virtual void onChar(unsigned codepoint) { (void)codepoint; }
    virtual void onMouseMove(float x, float y) { (void)x; (void)y; }
    virtual void onMouseButton(int button, int action, float x, float y) { (void)button; (void)action; (void)x; (void)y; }
    virtual void onSession(SessionEvent event) { (void)event; }
    // a popup returns false so the screen under it keeps drawing
    virtual bool opaque() const { return true; }
    // false while the screen still loads the script waitfor waits for it
    virtual bool ready() const { return true; }
    // true while the screens this one replaced must keep drawing in its place
    virtual bool holdsPrevious() const { return false; }
};

}
