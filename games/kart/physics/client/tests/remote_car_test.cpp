// Test remote car module 100 ms apart step tick ease position yaw anchor coast settled point

#include "../remote_car.h"

#include <cmath>
#include <cstdio>

using namespace KnC::Kart::Client;

int main() {
    int failures = 0;

    RemoteCarState car;
    car.frameDt = 0.02f;

    MotionSample s1;
    s1.pos[0] = 0.0f; s1.pos[1] = 0.0f; s1.pos[2] = 0.0f;
    s1.predPos[0] = 0.0f; s1.predPos[1] = 0.0f; s1.predPos[2] = 0.0f;
    s1.yawByte = 0;
    s1.statusLo = 0;
    s1.statusHi = 0;
    s1.hint = 1000;
    net_motion_sample_push(car.mailbox, s1);
    car_remote_update_tick(car, 0);
    std::printf("after sample one pos x %f y %f yaw %f\n", static_cast<double>(car.posX),
                static_cast<double>(car.posY), static_cast<double>(car.yawDeg));
    if (!car.everReceivedSample) { std::printf("first sample did not mark ever received\n"); ++failures; }

    MotionSample s2;
    s2.pos[0] = 10.0f; s2.pos[1] = 4.0f; s2.pos[2] = 0.0f;
    s2.predPos[0] = 10.0f; s2.predPos[1] = 4.0f; s2.predPos[2] = 0.0f;
    s2.yawByte = static_cast<uint8_t>(std::lround(90.0f / (360.0f / 255.0f)));
    s2.statusLo = 0;
    s2.statusHi = 0;
    s2.hint = 1000;
    net_motion_sample_push(car.mailbox, s2);
    car_remote_update_tick(car, 100);

    float targetX = car.anchorTarget[0];
    float targetY = car.anchorTarget[1];
    float targetYaw = 90.0f;
    std::printf("anchor target x %f y %f\n", static_cast<double>(targetX), static_cast<double>(targetY));

    for (int tick = 0; tick < 60; ++tick) {
        car_remote_update_tick(car, 100 + tick * 16);
        std::printf("tick %d pos x %f y %f yaw %f\n", tick, static_cast<double>(car.posX),
                    static_cast<double>(car.posY), static_cast<double>(car.yawDeg));
    }

    if (std::fabs(car.posX - targetX) > 0.5f) { std::printf("position ease did not reach the anchor x\n"); ++failures; }
    if (std::fabs(car.posY - targetY) > 0.5f) { std::printf("position ease did not reach the anchor y\n"); ++failures; }
    if (std::fabs(car.yawDeg - targetYaw) > 1.0f) { std::printf("yaw ease did not reach the sent yaw byte\n"); ++failures; }

    car.watchdogState = 1;
    car.watchdogThresholdMs = 500;
    car.watchdogSavedMs = 100 + 59 * 16;
    int64_t nowMs = car.watchdogSavedMs + 600;
    net_remote_watchdog(car, nowMs);
    if (car.watchdogState != 2) { std::printf("watchdog did not move to state two after the coast fire\n"); ++failures; }

    MotionSample settled;
    bool popped = net_motion_sample_pop(car.mailbox, settled);
    if (!popped) { std::printf("the coast left no settled sample pending\n"); ++failures; }
    bool poppedAgain = net_motion_sample_pop(car.mailbox, settled);
    if (poppedAgain) { std::printf("more than one settled sample was pending\n"); ++failures; }
    std::printf("settled sample pred pos x %f y %f\n", static_cast<double>(settled.predPos[0]),
                static_cast<double>(settled.predPos[1]));

    if (failures == 0) {
        std::printf("remote_car_test PASS\n");
        return 0;
    }
    std::printf("remote_car_test FAIL %d\n", failures);
    return 1;
}
