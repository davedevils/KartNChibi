// Test for the gimmicks module loads every csv of one track folder and prints row counts

#include "../car_state.h"
#include "../gimmicks.h"
#include "../math_helpers.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace KnC::Kart::Client;

namespace {

std::string client_data_dir() {
    const char* env = std::getenv("KNC_CLIENT_DATA");
    if (env && env[0] != '\0') {
        return env;
    }
    return "Data";
}

int failures = 0;

void report_boost(const std::vector<GimmickBoostRow>& rows) {
    std::printf("boost.ini rows %zu\n", rows.size());
    if (!rows.empty()) {
        const GimmickBoostRow& r = rows.front();
        std::printf("  first kind %d %f %f %f %f\n", r.kind, static_cast<double>(r.field_1),
                    static_cast<double>(r.field_2), static_cast<double>(r.field_3),
                    static_cast<double>(r.field_4));
    }
}

void report_itembox(const std::vector<GimmickItemboxRow>& rows) {
    std::printf("itembox.ini rows %zu\n", rows.size());
    if (!rows.empty()) {
        const GimmickItemboxRow& r = rows.front();
        std::printf("  first %f %f %f\n", static_cast<double>(r.x), static_cast<double>(r.y),
                    static_cast<double>(r.z));
    }
}

void report_itembite(const std::vector<GimmickItembiteRow>& rows) {
    std::printf("itembite.ini rows %zu\n", rows.size());
    if (!rows.empty()) {
        const GimmickItembiteRow& r = rows.front();
        std::printf("  first %f %f %f\n", static_cast<double>(r.x), static_cast<double>(r.y),
                    static_cast<double>(r.z));
    }
}

void report_itemdrum(const std::vector<GimmickItemdrumRow>& rows) {
    std::printf("itemdrum.ini rows %zu\n", rows.size());
    if (!rows.empty()) {
        const GimmickItemdrumRow& r = rows.front();
        std::printf("  first %f %f %f %f\n", static_cast<double>(r.x), static_cast<double>(r.y),
                    static_cast<double>(r.z), static_cast<double>(r.field_3));
    }
}

void report_follow(int32_t index, const std::vector<GimmickFollowRow>& rows) {
    std::printf("follow_%02d.ini rows %zu\n", index, rows.size());
    if (!rows.empty()) {
        const GimmickFollowRow& r = rows.front();
        std::printf("  first %f %f %f %f\n", static_cast<double>(r.field_0),
                    static_cast<double>(r.field_1), static_cast<double>(r.field_2),
                    static_cast<double>(r.field_3));
    }
}

void report_start(const std::vector<GimmickStartRow>& rows) {
    std::printf("start.ini rows %zu\n", rows.size());
    if (!rows.empty()) {
        const GimmickStartRow& r = rows.front();
        std::printf("  first %f %f %f %f\n", static_cast<double>(r.x), static_cast<double>(r.y),
                    static_cast<double>(r.z), static_cast<double>(r.heading));
    }
}

} // namespace

// one car on the origin with no effect running the themed checks drive it
void seat_car(GameState& game) {
    game = GameState();
    game.localCarIndex = 0;
    // the push and the boost start both need a running session over valid ground
    game.sessionRunning = 1;
    CarState& car = game.cars[0];
    car.body.overValidGround = 1;
    car.slotOccupied = 1;
    car.slotActive = 1;
    car.finishRank = -1;
    car.posX = 0.0f;
    car.posY = 0.0f;
    car.posZ = 0.0f;
    car.yawDeg = 0.0f;
    for (int w = 0; w < 4; ++w) car.wheelProbePoint[w] = Vec3{0.0f, 0.0f, 0.0f};
}

// one row of a class right under the car so every reach test hits it
GimmickWorld one_row(int32_t gimmickClass, float z) {
    GimmickWorld world;
    world.loaded = 1;
    GimmickInstance row;
    row.gimmickClass = gimmickClass;
    row.x = 0.0f;
    row.y = 0.0f;
    row.z = z;
    world.rows.push_back(row);
    return world;
}

// item box hit test 0x4BC830 yaw 0 drives toward minus x the sweep reaches one unit each way
void itembox_checks() {
    const float carX = 100.0f, carY = 50.0f;
    std::vector<GimmickItemboxRow> rows(3);
    rows[0].x = carX - 3.0f; rows[0].y = carY;
    rows[1].x = carX; rows[1].y = carY + 4.5f;
    rows[2].x = carX - 2.0f; rows[2].y = carY + 1.0f;
    const std::vector<uint8_t> ready = {1, 1, 0};
    // the box three units ahead is touched the one 4 5 to the side and the unready one are not
    const int32_t hit = itembox_hit_test(rows, ready, carX, carY, 0.0f, 0.0f);
    if (hit != 0) {
        std::printf("itembox_hit_test wanted row 0 ahead of the car got %d\n", hit);
        ++failures;
    }
    const std::vector<uint8_t> sideOnly = {0, 1, 0};
    if (itembox_hit_test(rows, sideOnly, carX, carY, 0.0f, 0.0f) != -1) {
        std::printf("itembox_hit_test touched the box 4 5 to the side\n");
        ++failures;
    }
    // turned 90 the car drives toward plus y and the side box comes inside the reach
    if (itembox_hit_test(rows, sideOnly, carX, carY, 90.0f, 0.0f) != 1) {
        std::printf("itembox_hit_test at yaw 90 wanted the row 1 box\n");
        ++failures;
    }
    std::printf("itembox_hit_test the ready rows and the 4 unit reach read right\n");
}

// world gimmick hit dispatch 0x4D3950 and car gimmick hit 0x4982D0 one case per effect family
void themed_gimmick_checks() {
    GameState game;
    // the stumble family of the classes 0 3 6 7 8 0xB 0xC 0xE 0x15 hands the effect 100
    const int32_t stumble[9] = {0, 3, 6, 7, 8, 11, 12, 14, 21};
    for (int32_t cls : stumble) {
        seat_car(game);
        GimmickWorld world = one_row(cls, 0.0f);
        const int32_t row = world_gimmick_hit_dispatch(game, 0, world, 1000);
        if (row != 0 || game.cars[0].effect.activeCode != GIMMICK_EFFECT_STUMBLE) {
            std::printf("gimmick class %d wanted the effect 100 got row %d code %d\n", cls, row,
                        game.cars[0].effect.activeCode);
            ++failures;
        }
    }
    // spin out family classes 4 5 0xA 0x14 hands effect 300 mole and fountain need the lift
    const int32_t spinout[4] = {4, 5, 10, 20};
    for (int32_t cls : spinout) {
        seat_car(game);
        GimmickWorld world = one_row(cls, 2.0f);
        const int32_t row = world_gimmick_hit_dispatch(game, 0, world, 1000);
        if (row != 0 || game.cars[0].effect.activeCode != GIMMICK_EFFECT_SPINOUT) {
            std::printf("gimmick class %d wanted the effect 300 got row %d code %d\n", cls, row,
                        game.cars[0].effect.activeCode);
            ++failures;
        }
    }
    // the cosmetic classes 2 0xD 0xF 0x10 0x11 touch their row and change nothing on the car
    const int32_t cosmetic[5] = {2, 13, 15, 16, 17};
    for (int32_t cls : cosmetic) {
        seat_car(game);
        GimmickWorld world = one_row(cls, 0.0f);
        const int32_t row = world_gimmick_hit_dispatch(game, 0, world, 1000);
        if (row != 0 || game.cars[0].effect.activeCode != 0) {
            std::printf("gimmick class %d wanted no effect got row %d code %d\n", cls, row,
                        game.cars[0].effect.activeCode);
            ++failures;
        }
    }
    // the class 9 tree door pushes with no effect code the yaw picks the bearing
    {
        seat_car(game);
        GimmickWorld world = one_row(9, 0.0f);
        const int32_t row = world_gimmick_hit_dispatch(game, 0, world, 1000);
        const CarState& car = game.cars[0];
        // car boost push 0x496B40 adds its force straight into the velocity it starts no boost
        const float speed = math_hypot2d(car.body.wheels.velocity.x, car.body.wheels.velocity.y);
        if (row != 0 || car.effect.activeCode != 0 || speed < 1.0f) {
            std::printf("gimmick class 9 wanted a push got row %d code %d speed %f\n", row,
                        car.effect.activeCode, static_cast<double>(speed));
            ++failures;
        }
    }
    // the classes 0x12 and 0x13 cancel the boost and cut the velocity after the push
    const int32_t block[2] = {18, 19};
    for (int32_t cls : block) {
        seat_car(game);
        CarState& car = game.cars[0];
        car.body.wheels.velocity.x = 10.0f;
        car.body.wheels.velocity.y = 10.0f;
        car.body.wheels.velocity.z = 10.0f;
        GimmickWorld world = one_row(cls, 0.0f);
        world.rows[0].x = 3.0f;
        for (int w = 0; w < 4; ++w) car.wheelProbePoint[w] = Vec3{2.0f, 0.0f, 0.0f};
        const int32_t row = world_gimmick_hit_dispatch(game, 0, world, 1000);
        if (row != 0 || car.boostKind != BOOST_KIND_CANCEL) {
            std::printf("gimmick class %d wanted the boost cancel got row %d kind %d\n", cls, row,
                        car.boostKind);
            ++failures;
        }
        // the push lands before the cut so only the z term reads a clean tenth of its start
        if (std::fabs(car.body.wheels.velocity.z - (10.0f + GIMMICK_BLOCK_STRENGTH * 0.1f) * 0.1f) > 0.01f) {
            std::printf("gimmick class %d wanted the velocity cut got z %f\n", cls,
                        static_cast<double>(car.body.wheels.velocity.z));
            ++failures;
        }
    }
    // a car already under an effect takes no gimmick at all 0x4982E7
    {
        seat_car(game);
        game.cars[0].effect.activeCode = 700;
        GimmickWorld world = one_row(0, 0.0f);
        if (world_gimmick_hit_dispatch(game, 0, world, 1000) != -1) {
            std::printf("gimmick dispatch ran on a car that already carries an effect\n");
            ++failures;
        }
    }
    // a row that already stands live is skipped and a far car touches nothing
    {
        seat_car(game);
        GimmickWorld world = one_row(0, 0.0f);
        world.rows[0].state = 1;
        if (world_gimmick_hit_dispatch(game, 0, world, 1000) != -1) {
            std::printf("gimmick dispatch hit a row that already stands live\n");
            ++failures;
        }
        seat_car(game);
        GimmickWorld far = one_row(0, 0.0f);
        far.rows[0].x = 40.0f;
        if (world_gimmick_hit_dispatch(game, 0, far, 1000) != -1) {
            std::printf("gimmick dispatch hit a row 40 units away\n");
            ++failures;
        }
    }
    // gimmick mushman update 0x4DB500 idle finds the car chases catches grows then walks home
    {
        seat_car(game);
        game.cars[0].posX = 6.0f;
        GimmickMushman mush;
        mush.homeX = 0.0f;
        mush.homeY = 0.0f;
        mush.x = 0.0f;
        mush.y = 0.0f;
        gimmick_mushman_update(game, mush, 0);
        if (mush.state != GimmickMushState::Chase || mush.carIndex != 0) {
            std::printf("mushman wanted the chase got state %d car %d\n", static_cast<int>(mush.state),
                        mush.carIndex);
            ++failures;
        }
        int guard = 0;
        while (mush.state == GimmickMushState::Chase && guard < 200) {
            gimmick_mushman_update(game, mush, 20 * ++guard);
        }
        if (mush.state != GimmickMushState::Caught) {
            std::printf("mushman wanted the catch got state %d after %d frames\n",
                        static_cast<int>(mush.state), guard);
            ++failures;
        }
        gimmick_mushman_update(game, mush, mush.stateMs + GIMMICK_MUSH_HOLD_MS + 1);
        if (mush.state != GimmickMushState::Grow) {
            std::printf("mushman wanted the grow got state %d\n", static_cast<int>(mush.state));
            ++failures;
        }
        int64_t now = mush.stateMs;
        guard = 0;
        while (mush.state == GimmickMushState::Grow && guard < 1000) {
            gimmick_mushman_update(game, mush, now += 20);
            ++guard;
        }
        if (mush.state != GimmickMushState::Cooldown) {
            std::printf("mushman wanted the cooldown got state %d\n", static_cast<int>(mush.state));
            ++failures;
        }
        gimmick_mushman_update(game, mush, mush.stateMs + GIMMICK_MUSH_HOLD_MS + 1);
        if (mush.state != GimmickMushState::Idle || std::fabs(mush.x - mush.homeX) > 0.001f) {
            std::printf("mushman wanted the reset got state %d at %f\n", static_cast<int>(mush.state),
                        static_cast<double>(mush.x));
            ++failures;
        }
    }
    // car nearest index 0x499B50 skips a finished car and an empty slot
    {
        seat_car(game);
        game.cars[0].posX = 5.0f;
        if (car_nearest_index(game, 0.0f, 0.0f, 18.0f, -1) != 0) {
            std::printf("car nearest index missed the only racing car\n");
            ++failures;
        }
        game.cars[0].finishRank = 2;
        if (car_nearest_index(game, 0.0f, 0.0f, 18.0f, -1) != -1) {
            std::printf("car nearest index took a finished car\n");
            ++failures;
        }
    }
    std::printf("themed gimmicks the nine effect families read right\n");
}

int main() {
    themed_gimmick_checks();
    itembox_checks();
    const std::string data_dir = client_data_dir();
    const std::string track_dir = data_dir + "/Public/World/Cookie/Cookie_01";

    std::error_code ec;
    if (!std::filesystem::exists(track_dir, ec)) {
        std::printf("gimmicks_test skipped track folder missing %s\n", track_dir.c_str());
        return 0;
    }

    std::string error;

    std::vector<GimmickBoostRow> boost_rows;
    if (!gimmick_load_boost(track_dir, boost_rows, error)) {
        std::printf("gimmick_load_boost failed %s\n", error.c_str());
        ++failures;
    } else {
        report_boost(boost_rows);
    }

    std::vector<GimmickItemboxRow> itembox_rows;
    if (!gimmick_load_itembox(track_dir, itembox_rows, error)) {
        std::printf("gimmick_load_itembox failed %s\n", error.c_str());
        ++failures;
    } else {
        report_itembox(itembox_rows);
    }

    std::vector<GimmickItembiteRow> itembite_rows;
    if (!gimmick_load_itembite(track_dir, itembite_rows, error)) {
        std::printf("gimmick_load_itembite failed %s\n", error.c_str());
        ++failures;
    } else {
        report_itembite(itembite_rows);
    }

    std::vector<GimmickItemdrumRow> itemdrum_rows;
    if (!gimmick_load_itemdrum(track_dir, itemdrum_rows, error)) {
        std::printf("gimmick_load_itemdrum failed %s\n", error.c_str());
        ++failures;
    } else {
        report_itemdrum(itemdrum_rows);
    }

    for (int32_t index = GIMMICK_FOLLOW_FILE_MIN; index <= GIMMICK_FOLLOW_FILE_MAX; ++index) {
        std::vector<GimmickFollowRow> follow_rows;
        if (!gimmick_load_follow(track_dir, index, follow_rows, error)) {
            std::printf("follow_%02d.ini not present %s\n", index, error.c_str());
            continue; // only follow 01 and follow 02 ship with Cookie 01 not parse error
        }
        report_follow(index, follow_rows);
    }

    std::vector<GimmickStartRow> start_rows;
    if (!gimmick_load_start(track_dir, start_rows, error)) {
        std::printf("gimmick_load_start failed %s\n", error.c_str());
        ++failures;
    } else {
        report_start(start_rows);
    }

    // itemdrum hit test 0x4bed40 the barrel rows of the track drive the three responses
    if (!itemdrum_rows.empty()) {
        const GimmickItemdrumRow& row = itemdrum_rows.front();
        const std::vector<uint8_t> none(itemdrum_rows.size(), 0);
        std::vector<uint8_t> broken(itemdrum_rows.size(), 0);
        broken[0] = 1;
        // a car on the barrel at speed takes the bounce with the push away from it
        GimmickDrumResult hit = itemdrum_hit_test(itemdrum_rows, none, row.x, row.y, 0.0f, 0.0f, 90.0f, false);
        if (hit.kind != GimmickDrumHit::Bounce || hit.row != 0) {
            std::printf("itemdrum_hit_test fast wanted a bounce on row 0 got kind %d row %d\n",
                        static_cast<int>(hit.kind), hit.row);
            ++failures;
        } else if (hit.pushStrength < 27.9f || hit.pushStrength > 28.1f) {
            std::printf("itemdrum_hit_test push %f is not 90 times 0 2 plus 10\n", hit.pushStrength);
            ++failures;
        }
        // under the 20 km per hour gate the same touch only halves the velocity
        hit = itemdrum_hit_test(itemdrum_rows, none, row.x, row.y, 0.0f, 0.0f, 10.0f, false);
        if (hit.kind != GimmickDrumHit::Slow) {
            std::printf("itemdrum_hit_test slow wanted the slow kind got %d\n", static_cast<int>(hit.kind));
            ++failures;
        }
        // a boosting car loses the boost whatever its speed
        hit = itemdrum_hit_test(itemdrum_rows, none, row.x, row.y, 0.0f, 0.0f, 90.0f, true);
        if (hit.kind != GimmickDrumHit::BoostCancel) {
            std::printf("itemdrum_hit_test boost wanted the cancel got %d\n", static_cast<int>(hit.kind));
            ++failures;
        }
        // a broken barrel is skipped and a car 40 units away touches nothing
        hit = itemdrum_hit_test(itemdrum_rows, broken, row.x, row.y, 0.0f, 0.0f, 90.0f, false);
        if (hit.row == 0) {
            std::printf("itemdrum_hit_test hit the broken row 0\n");
            ++failures;
        }
        hit = itemdrum_hit_test(itemdrum_rows, none, row.x + 40.0f, row.y + 40.0f, 0.0f, 0.0f, 90.0f, false);
        if (hit.kind != GimmickDrumHit::None) {
            std::printf("itemdrum_hit_test far wanted nothing got %d\n", static_cast<int>(hit.kind));
            ++failures;
        }
        std::printf("itemdrum_hit_test %zu rows the three responses read right\n", itemdrum_rows.size());
    }

    if (failures == 0) {
        std::printf("gimmicks_test PASS\n");
        return 0;
    }
    std::printf("gimmicks_test FAIL %d\n", failures);
    return 1;
}
