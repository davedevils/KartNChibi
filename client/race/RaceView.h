// track scene on scene renderer one ghost car and driver per racer plus chase camera
#pragma once

#include "RaceSim.h"
#include "TrackData.h"

#include "tools/track_scene/ghost_car.h"

#include "games/kart/physics/client/world_collision.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace KnC::Render { class SceneRenderer; }

namespace KnC::Client {

// weather word from 0x0014 launch dword 0xB2319C manager sub 4D1C70 picks class by it
enum class RaceWeather { Clear = 0, Night = 1, Rain = 2, Snow = 3 };

// one question mark box of track session hides it on pickup shows again later
struct ItemBoxSpot {
    float position[3] = {0.f, 0.f, 0.f};
    // true while a car holds it box draws again when respawn clock passes
    bool hidden = false;
    // scene clock second box comes back below clock means it is back
    float respawnAt = 0.f;
};

// race camera from camera update 0x43F040 mode 1 behind car on row tail floats
struct ChaseCamera {
    // camera 0x74 degrees eased toward car yaw plus 90 minus 0 8 times drift gauge
    float headingDeg = 0.f;
    // camera 0x98 eye camera 0xBC look point countdown eases them
    float eye[3] = {0.f, 0.f, 0.f};
    float look[3] = {0.f, 0.f, 0.f};
    // camera 0x8 field of view radians 0x43E850 widens with speed
    float fovRadians = 1.05f;
    // camera 0x1E8 shake kick raises it 0x84 per frame decay jitters look point
    float shake = 0.f;
    bool valid = false;
    // camera mode 0xD of licence test field stays 1 05 distance at least 8 4
    bool licenceMode = false;

    // one frame of stock rule dt in seconds stock ran it per rendered frame at 60 a second
    void update(const CarPose& car, float dt);
    // camera mode 9 finish 0x3C sets it 6 units off nose 3 up looking above car
    void finish(const CarPose& car, float dt);
    // fixed eye look and field radians podium mode 8 stands here
    void place(const float eyeIn[3], const float lookIn[3], float fov, float dt);
    // camera shake kick 0x43EAD0 countdown digits and podium thumps call it
    void kick(float amount) { if (amount > shake) shake = amount; }
    // view matrix and eye for scene renderer shake jitters look point
    void view(float out[16], float outEye[3]) const;
    void reset() { valid = false; shake = 0.f; }

private:
    void decayShake(float dt);
};

class RaceView {
public:
    // uploads track scene sets far plane from its bounds
    bool load(KnC::Render::SceneRenderer& renderer, RaceWorld& world);
    // empty scene so preview can show car with no track loaded
    bool loadEmpty(KnC::Render::SceneRenderer& renderer);
    // one car visual on kart model and driver body loaded once per name and paint returns its handle
    int addCar(KnC::Render::SceneRenderer& renderer, const std::string& gameDir, const std::string& kartModel,
               const std::string& driverAsset, const std::string& paint = std::string());
    // swaps body of a car for copy painted with this BodyColor folder loaded once per pair
    void setCarPaint(KnC::Render::SceneRenderer& renderer, int handle, const std::string& paint);
    // driver manager load driver 0x48CE97 pet body nif in Pet Body facial folder in Pet Facial empty nif removes pet
    void setCarPet(KnC::Render::SceneRenderer& renderer, int handle, const std::string& petNif,
                   const std::string& facialDir);
    // kart model name a car was added with empty for bad handle
    const std::string& kartModelOf(int handle) const;
    // moves car visual to pose body wheels and clip follow pose feed
    void setPose(int handle, const CarPose& pose, float dt, float clockSeconds);
    void removeCar(int handle);
    // chase camera behind pose then scene draw
    void draw(KnC::Render::SceneRenderer& renderer, const CarPose& chase, float dt);
    // finish camera of mode 9 in front of own kart then scene draw
    void drawFinish(KnC::Render::SceneRenderer& renderer, const CarPose& own, float dt);
    // fixed camera eye look and vertical field radians then scene draw podium uses it
    void drawFixed(KnC::Render::SceneRenderer& renderer, const float eye[3], const float look[3], float fovRadians,
                   float dt);
    // preview camera turns around target at fixed distance and height
    void drawOrbit(KnC::Render::SceneRenderer& renderer, const CarPose& target, float orbitDeg, float distance,
                   float height, float dt);
    // question mark boxes of track at their itembox ini spots session picks from this list
    const std::vector<ItemBoxSpot>& itemBoxes() const { return m_itemBoxes; }
    std::vector<ItemBoxSpot>& itemBoxes() { return m_itemBoxes; }
    // hides one box until that scene clock second race session calls it on pickup
    void hideItemBox(size_t index, float respawnAt);
    // boxes that came back show again call it once a frame with scene clock
    void refreshItemBoxes(float clockSeconds);
    // sky phase of track true takes night nif from 0x0014 launch flag
    void setNightSky(KnC::Render::SceneRenderer& renderer, bool night);
    // sub 4D1C70 and sub 4D3570 load weather class of race rain sheet splash thunder
    bool setWeather(KnC::Render::SceneRenderer& renderer, const std::string& gameDir, RaceWeather weather);
    // sub 4D2F10 sheet rides camera splash pops near car thunder strikes ahead
    void updateWeather(KnC::Render::SceneRenderer& renderer, float dt, const float carPos[3]);
    // sub 4D34D0 veil over frame 0x60 in rain 0x30 in snow fresh thunder lifts it
    uint8_t weatherVeil() const;
    // true once when thunder fired so screen plays its cue
    bool takeThunder();
    RaceWeather weather() const { return m_weather; }
    // one more prop drawn with cars returns its handle podium stands on one
    int addProp(KnC::Render::SceneRenderer& renderer, const KnC::Render::PropModel& model);
    void placeProp(int handle, const float world[16], bool shown);
    // restarts controllers and emitters of a prop on scene clock gacha machine plays once
    void restartProp(KnC::Render::SceneRenderer& renderer, int handle);
    // driver clip forced on car by KFM sequence id minus one gives rule back
    void setDriverClip(int handle, int sequenceId);
    // view and eye of last draw whatever camera hud projects name tags with them
    void lastView(float out[16], float outEye[3]) const;
    bool loaded() const { return m_loaded; }
    const ChaseCamera& camera() const { return m_camera; }
    ChaseCamera& camera() { return m_camera; }

private:
    struct CarModel {
        KnC::Tools::GhostCar car;
        size_t firstModelIndex = 0;
        bool valid = false;
    };
    struct DriverModel {
        KnC::Tools::GhostDriver driver;
        size_t modelIndex = 0;
        bool valid = false;
    };
    // pet body with its kfm clips loaded once per Pet Body folder
    struct PetModel {
        KnC::Tools::GhostDriver driver;
        size_t modelIndex = 0;
        bool valid = false;
    };
    // driver place on car 0x48B800 three hover state machines from pet manager 0xE8A0 0xE8A4 0xE8A8
    struct PetHover {
        int trailState = 0;
        float trail = 0.f;
        double trailAt = 0.0;
        int sideState = 0;
        float side = 0.f;
        double sideAt = 0.0;
        int bobState = 0;
        float bob = 0.f;
    };
    struct CarVisual {
        // car model key is kart model at paint
        std::string kartModel;
        std::string kartName;
        std::string paint;
        std::string driverAsset;
        bool alive = false;
        KnC::Tools::GhostWheelState wheels;
        float prevPos[3] = {0.f, 0.f, 0.f};
        bool prevValid = false;
        float world[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
        int clip = -2;
        float clipStart = 0.f;
        float clipSeconds = 0.f;
        // KFM sequence forced on driver minus one lets pose rule pick
        int forcedClip = -1;
        // pet model key empty for none plus its hover
        std::string petModel;
        PetHover hover;
        // car 0x3224 and 0x3228 of remote car ground under its wheels eased by an eighth
        float pitchDeg = 0.f;
        float rollDeg = 0.f;
        KnC::Kart::Client::BspQuery probe;
    };
    // prop beside cars podium nif and its confetti
    struct ExtraProp {
        size_t modelIndex = 0;
        float world[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
        bool shown = false;
    };

    // fills prop and character instance lists of every live car
    void submit(KnC::Render::SceneRenderer& renderer);
    // car visual update 0x48E6A0 ground under four wheel points pitches and rolls remote car
    void remoteLean(CarVisual& v, const CarPose& pose, float dt);
    // lens of camera then scene draw last view kept for hud
    void present(KnC::Render::SceneRenderer& renderer, float fovRadians);
    // rolls for weather placements zero to span and minus one to one
    float weatherRoll(float span);
    float weatherJitter();
    CarModel& carModel(KnC::Render::SceneRenderer& renderer, const std::string& gameDir, const std::string& model,
                       const std::string& paint);
    DriverModel& driverModel(KnC::Render::SceneRenderer& renderer, const std::string& gameDir, const std::string& asset,
                             const std::string& chassis);
    PetModel& petModel(KnC::Render::SceneRenderer& renderer, const std::string& petNif, const std::string& facialDir);
    // hover of pet per frame off car speed and clock
    void hoverPet(CarVisual& v, const CarPose& pose, float dt, double clockSeconds);

    std::map<std::string, CarModel> m_carModels;
    std::string m_gameDir;
    std::map<std::string, DriverModel> m_driverModels;
    std::map<std::string, PetModel> m_petModels;
    std::vector<CarVisual> m_cars;
    std::vector<ExtraProp> m_props;
    // appended item box model plus one spot per itembox ini row of track
    std::size_t m_itemBoxModel = static_cast<std::size_t>(-1);
    std::vector<ItemBoxSpot> m_itemBoxes;
    ChaseCamera m_camera;
    // weather class from sub 4D3570 one prop per nif minus one when mode does not load it
    RaceWeather m_weather = RaceWeather::Clear;
    int m_rainSheet = -1;
    int m_rainSplash = -1;
    int m_thunderProp = -1;
    int m_snowSheet = -1;
    // 0x27C splash state and its clock 0x28C thunder state and its clock
    int m_splashState = 0;
    float m_splashAge = 0.f;
    int m_thunderState = 0;
    float m_thunderAge = 0.f;
    float m_thunderPos[3] = {0.f, 0.f, 0.f};
    bool m_thunderFired = false;
    // rain sheet emitter window is 3 33 s long place restarts it when it runs out
    float m_rainAge = 0.f;
    uint32_t m_weatherRng = 2463534242u;
    KnC::Kart::Client::BspQuery m_weatherProbe;
    // track collision of loaded world remote lean probes it null for a preview
    const KnC::Kart::Client::ColTrack* m_collision = nullptr;
    float m_lastView[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    float m_lastEye[3] = {0.f, 0.f, 0.f};
    size_t m_characterCount = 0;
    bool m_loaded = false;
};

}
