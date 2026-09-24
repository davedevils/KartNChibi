// sub 4A9EA0 the attack view and sub 4AD3C0 the event view two cameras on the race world
#pragma once

#include "RaceSim.h"

#include <cstdint>

namespace KnC::Render { class SceneRenderer; }

namespace KnC::Client {

class RaceItems;

class ItemViews {
public:
    void reset();
    // sub 4A9AC0 a camera in front of the car for 3000 ms its heading and pitch are taken once
    void startAttack(int carIndex, const CarPose& pose);
    // sub 4AB570 mode 3 a car mode 4 an item kind and its slot refused while one runs
    void startEvent(int mode, int a, int b);
    // sub 4A9BA0 and sub 4AB880 the eyes follow their subject the look points ease a quarter a frame
    void update(float dt, const RaceSim& sim, const RaceItems& items);
    // sub 4A9DA0 and sub 4AD310 the world through the two cameras into their rects of the canvas
    void draw(KnC::Render::SceneRenderer& renderer, uint16_t fbW, uint16_t fbH, float canvasW, float canvasH) const;
    bool attackShown() const { return m_attack.active; }
    int attackCar() const { return m_attack.car; }
    // the frame sprite swaps between its two pngs every second 60 Hz frame
    int attackFrame() const { return m_attack.frame; }
    bool eventShown() const { return m_event.active; }
    int eventFrame() const { return m_event.frame; }

private:
    struct View {
        bool active = false;
        int mode = 0;
        int a = -1;
        int b = -1;
        int car = -1;
        double startedAt = 0.0;
        bool first = true;
        float heading = 0.f;
        float pitch = 0.f;
        float eye[3] = {0.f, 0.f, 0.f};
        float look[3] = {0.f, 0.f, 0.f};
        int frame = 0;
        float frameClock = 0.f;
    };
    void ease(View& v, const float look[3], float dt) const;
    void tick(View& v, float dt) const;

    View m_attack;
    View m_event;
    double m_now = 0.0;
};

}
