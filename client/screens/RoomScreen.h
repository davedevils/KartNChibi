// the waiting room on the stock overlay the karts in 3D the player list the track box the chat
#pragma once

#include "net/Session.h"
#include "race/RaceSim.h"
#include "race/RaceView.h"
#include "race/TrackData.h"
#include "ui/WidgetScreen.h"

#include <map>
#include <string>
#include <vector>

namespace KnC::Client {

class RoomScreen : public WidgetScreen {
public:
    explicit RoomScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "room"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void onKey(int key, int action, int mods) override;
    void onSession(SessionEvent event) override;

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    void refreshButtons();
    void refreshCars();
    // the local kart drives the room field the stock runs cars frame update in stage 9 too
    void startRoomDrive();
    void driveRoom(float dt);
    // the room reads 0x0021 0x0064 and 0x0040 itself so the team side stays in one wire space
    void onRoomFrame(uint16_t opcode, Packet& pkt);
    // 0 red 1 blue as the wire says it minus one when the member never carried a team
    int wireTeam(uint32_t playerId) const;
    // the room craft world of the 0x0013 decor the floor loads like a track the rest as props
    bool loadRoomWorld(std::string& error);
    // the inviting box of the random invite stays this long after the press below zero when closed
    float m_inviteBoxLeft = -1.f;
    void sendChat();
    void openTrackPick();
    void exitRoom();
    std::string trackName(int trackId) const;

    // one member kart on the room world at its start row
    struct Car {
        uint32_t playerId = 0;
        std::string model;
        std::string driver;
        // the kart the driver and the parts the member blobs name a new 0x0021 with another look rebuilds the car
        std::string look;
        int handle = -1;
        int carIndex = -1;
        float x = 0.f, y = 0.f, z = 0.f;
    };

    // the camera of the room scene mode 14 until the own seat stands then mode 13 on it
    void roomCamera(float eye[3], float look[3]) const;
    // FUN 0048DB30 sets camera mode 13 on the own room car camera update 0x43F040 follows it each frame
    void followCamera(float dt);
    // a world point to canvas units through the room camera false when it lies behind the eye
    bool project(const float world[3], float& sx, float& sy) const;
    void drawNamePlates(DrawContext& ctx);

    RaceView m_view;
    RaceWorld m_world;
    std::vector<Car> m_cars;
    bool m_sceneReady = false;
    bool m_worldLoaded = false;
    // the 30 second auto start of the stock shown once a second seat fills below zero when off
    float m_autoStartLeft = -1.f;
    InputWidget* m_chatInput = nullptr;
    InputWidget* m_whisperInput = nullptr;
    ButtonWidget* m_readyButton = nullptr;
    ButtonWidget* m_startButton = nullptr;
    std::string m_status;
    float m_time = 0.f;
    // the auto walk picks the track presses ready or start and captures the room once
    bool m_autoTrackSent = false;
    bool m_autoReadySent = false;
    bool m_autoStartSent = false;
    bool m_captured = false;
    bool m_capturedMore = false;
    size_t m_capturedMembers = 0;
    float m_trackAckAt = -1.f;
    bool m_ready = false;
    bool m_autoTeamSent = false;
    bool m_readySoundPlayed = false;
    // the room physics the local car on the floor col the others on the room 0x0040
    RaceSim m_sim;
    bool m_simReady = false;
    bool m_localSpawned = false;
    double m_driveClock = 0.0;
    double m_motionAt = 0.0;
    // the mode 13 eye and look of the last frame valid once the own car stands on the floor
    float m_followEye[3] = {0.f, 0.f, 0.f};
    float m_followLook[3] = {0.f, 0.f, 0.f};
    bool m_followValid = false;
    // the team of every member in the wire space of 0x0021 and 0x0064 0 red 1 blue
    std::map<uint32_t, int> m_teams;
};

// the track choice over the room arrows walk the 0x00C3 rows OK sends 0x0035
class TrackPickPopup : public Screen {
public:
    explicit TrackPickPopup(App& app);
    const char* name() const override { return "trackpick"; }
    bool opaque() const override { return false; }
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;

private:
    // the tracks of the picked theme in id order
    std::vector<const TrackRow*> themeTracks() const;
    void stepTheme(int delta);
    void step(int delta);
    void confirm();
    // the weather radio of the track box 0 sun 1 night 2 rain the pick rides the 0x0035 tail
    int m_weather = 0;
    void close();
    App& m_app;
    // the theme ids with a pickable track the picked one and the first of the four shown
    std::vector<uint32_t> m_themes;
    int m_theme = 0;
    int m_first = 0;
    int m_index = 0;
    bool m_closed = false;
};

}
