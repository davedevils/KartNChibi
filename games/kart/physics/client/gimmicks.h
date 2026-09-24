#pragma once
// Gimmick csv loaders and hit tests ported from EFFECTS AND GEAR

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Kart::Client {

// boost ini one line kind then four floats gimmick load boost 0x48AB10
struct GimmickBoostRow {
    int32_t kind = 0;  // csv field 0 boost pad kind see BOOST KIND constants below csv field 1 meaning not resolved
    float field_1 = 0.0f;
    float field_2 = 0.0f;  // csv field 2 meaning not resolved csv field 3 meaning not resolved
    float field_3 = 0.0f;
    float field_4 = 0.0f;  // csv field 4 meaning not resolved
};

// itembox ini one line position gimmick load itembox 0x48AC20
struct GimmickItemboxRow {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// itembite ini one line position gimmick load itembite 0x48AD20
struct GimmickItembiteRow {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// itemdrum ini one line position plus one float gimmick load itemdrum 0x48AF20
struct GimmickItemdrumRow {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float field_3 = 0.0f; // csv field 3 meaning not resolved
};

// follow csv one line four floats gimmick load follow 0x489730
struct GimmickFollowRow {
    float field_0 = 0.0f;  // csv field 0 guess position meaning not resolved csv field 1 guess position meaning not resolved
    float field_1 = 0.0f;
    float field_2 = 0.0f;  // csv field 2 guess position meaning not resolved csv field 3 meaning not resolved
    float field_3 = 0.0f;
};

// start ini one line position plus heading gimmick load start 0x48A800
struct GimmickStartRow {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float heading = 0.0f; // csv field 3 heading in degrees zero drives toward minus x
};

// itemdrum hit test 0x4bed40 what the barrel does to the car that touched it
enum class GimmickDrumHit : int32_t {
    None = 0,  // no barrel under the swept point under 20 km per hour the x and y velocity halve
    Slow = 1,
    Bounce = 2,  // over 20 kmh impact then push away from barrel boosting car loses boost keeps 0 7 velocity
    BoostCancel = 3
};

// itemdrum hit test 0x4bed40 result the row it touched and what to do with the car
struct GimmickDrumResult {
    GimmickDrumHit kind = GimmickDrumHit::None;
    int32_t row = -1;  // the itemdrum row index minus one when nothing was touched the bearing of the push away from the barrel
    float pushHeadingDeg = 0.0f;
    float pushStrength = 0.0f;   // km per hour times 0 2 plus 10
};

// itemdrum hit test 0x4bed40 the coarse reach then a sweep of the heading point every 0 2 units 0x59f404
constexpr float GIMMICK_DRUM_COARSE_REACH = 10.0f;
constexpr float GIMMICK_DRUM_HIT_REACH = 5.0f;  // 0x5a3238 0x5a15ec
constexpr float GIMMICK_DRUM_SWEEP_STEP = 0.2f;
constexpr float GIMMICK_DRUM_SWEEP_FROM = -1.0f;
constexpr float GIMMICK_DRUM_SWEEP_TO = 1.0f;  // 0x59f480 0x5a3298
constexpr float GIMMICK_DRUM_SLOW_KMH = 20.0f;
constexpr float GIMMICK_DRUM_SLOW_SCALE = 0.5f;  // 0x59f414 0x5a32b4
constexpr float GIMMICK_DRUM_BOOST_SCALE = 0.7f;
constexpr float GIMMICK_DRUM_PUSH_TURN = 270.0f;  // 0x5a6a50 0x5a15ec
constexpr float GIMMICK_DRUM_PUSH_PER_KMH = 0.2f;
constexpr float GIMMICK_DRUM_PUSH_FLOOR = 10.0f;  // 0x59f404 0x5a32b0
constexpr float GIMMICK_DRUM_GAUGE_TURN = 1.2f;
constexpr float GIMMICK_DRUM_HEADING_TURN = 90.0f;  // 0x5a323c

// itemdrum hit test 0x4bed40 a broken row is skipped the caller keeps the broken flags
GimmickDrumResult itemdrum_hit_test(const std::vector<GimmickItemdrumRow>& rows,
                                     const std::vector<uint8_t>& broken, float carX, float carY,
                                     float carYawDeg, float driftGaugeSmoothed, float speedKmh,
                                     bool boosting);

// item box hit test 0x4BC830 the same sweep as the drum with a 4 unit reach on the ready boxes
constexpr float GIMMICK_BOX_COARSE_REACH = 10.0f;
constexpr float GIMMICK_BOX_HIT_REACH = 4.0f;
// 0x4BCA90 a taken box hides 2000 ms then waits 500 ms before it can be taken again
constexpr int32_t GIMMICK_BOX_HIDDEN_MS = 2000;
constexpr int32_t GIMMICK_BOX_ARM_MS = 500;
// the first ready row the car sweep touches minus one when none a row with ready zero is skipped
int32_t itembox_hit_test(const std::vector<GimmickItemboxRow>& rows, const std::vector<uint8_t>& ready, float carX,
                         float carY, float carYawDeg, float driftGaugeSmoothed);

constexpr size_t GIMMICK_BOOST_MAX_LINES = 100;  // gimmick load boost 0x48AB10 gimmick load itembox 0x48AC20
constexpr size_t GIMMICK_ITEMBOX_MAX_LINES = 100;
constexpr size_t GIMMICK_ITEMBITE_MAX_LINES = 100;  // gimmick load itembite 0x48AD20 gimmick load itemdrum 0x48AF20
constexpr size_t GIMMICK_ITEMDRUM_MAX_LINES = 80;
constexpr size_t GIMMICK_FOLLOW_MAX_LINES = 400;  // gimmick load follow 0x489730 per file gimmick load start 0x48A800
constexpr size_t GIMMICK_START_MAX_LINES = 100;
constexpr int32_t GIMMICK_FOLLOW_FILE_MIN = 1;  // follow 01 ini follow 04 ini
constexpr int32_t GIMMICK_FOLLOW_FILE_MAX = 4;

// loaders read track ini files from disk this port skips exe staging
bool gimmick_load_boost(const std::string& track_dir, std::vector<GimmickBoostRow>& out,
                         std::string& error);
bool gimmick_load_itembox(const std::string& track_dir, std::vector<GimmickItemboxRow>& out,
                           std::string& error);
bool gimmick_load_itembite(const std::string& track_dir, std::vector<GimmickItembiteRow>& out,
                            std::string& error);
bool gimmick_load_itemdrum(const std::string& track_dir, std::vector<GimmickItemdrumRow>& out,
                            std::string& error);
// index 1 to 4 picks follow 01 ini to follow 04 ini a missing file is not a parse error
bool gimmick_load_follow(const std::string& track_dir, int32_t index,
                          std::vector<GimmickFollowRow>& out, std::string& error);
bool gimmick_load_start(const std::string& track_dir, std::vector<GimmickStartRow>& out,
                         std::string& error);

// car boost start 0x496BE0 kind argument named values seen in the doc kind zero the mini turbo boost
constexpr int32_t BOOST_KIND_MINI_TURBO = 0;
constexpr int32_t BOOST_KIND_CANCEL = 6;  // kind six cancels the active boost boost ini kind 3 is a launch pad car launch pad kick 0x497190
constexpr int32_t BOOST_KIND_LAUNCH_PAD = 3;

// world cell boost pad index 0x486CF0 a cell named BOOST NNN gives row NNN minus 1 else minus one
int32_t world_cell_boost_pad_index(const char* cell_name);

// world cell warp index 0x486CA0 a cell named WARP NNN gives NNN minus 1 else minus one
int32_t world_cell_warp_index(const char* cell_name);

// world wheel bump slot 0x4C37D0 one entry of the itembite and itemdrum 16 slot tables
struct GimmickBumpSlot {
    uint8_t active = 0;  // entry minus 8 one means the slot is live entry minus 4 the car occupying the slot
    int32_t car_index = -1;
    int32_t payload = 0;    // entry plus 0 must be zero for a hit match
};

constexpr size_t GIMMICK_BUMP_TABLE_SIZE = 16; // world wheel bump slot 0x4C37D0 table size

// world wheel bump slot 0x4C37D0 returns the matching slot index or negative one
int32_t world_wheel_bump_slot(const std::array<GimmickBumpSlot, GIMMICK_BUMP_TABLE_SIZE>& table,
                               int32_t car_index);

// effect hazard hit lookup 0x4CFCB0 one entry of the effect 500 16 slot table
struct GimmickHazardSlot {
    uint8_t active = 0;  // entry minus 0x38 one means the slot is live entry minus 0x30 the car occupying the slot
    int32_t car_index = -1;
    int32_t countdown = 0;  // entry plus 0 unit not determined
};

constexpr size_t GIMMICK_HAZARD_TABLE_SIZE = 16;  // effect hazard hit lookup 0x4CFCB0 table size effect hazard hit lookup lower bound 102
constexpr int32_t GIMMICK_HAZARD_COUNTDOWN_MIN = 0x66;
constexpr int32_t GIMMICK_HAZARD_COUNTDOWN_MAX = 299;  // effect hazard hit lookup upper bound

// effect hazard hit lookup 0x4CFCB0 returns the matching slot index or negative one
int32_t effect_hazard_hit_lookup(const std::array<GimmickHazardSlot, GIMMICK_HAZARD_TABLE_SIZE>& table,
                                  int32_t car_index);

constexpr size_t GIMMICK_POOL_LIVE_SLOTS = 8; // gimmick pool update 0x4C7ED0 live slot cap

// pool update 0x4C7ED0 state ints 0 1 100 200 201 read as denormal floats in the decompile
enum class GimmickPoolState : int32_t {
    Idle = 0,
    GrabLock = 1,  // effect 600 applied car held 600 ms slot drags car along follow polyline for carry window
    Carry = 100,
    Drop = 200,  // car placed on follow row kicked 40 along its yaw release boost window then slot frees after 1300 ms
    Released = 201
};

// carry pool slot 0x250 bytes pool 0x2EFC818 holds 8 effect 900 pool 0x2ECDA38 same offsets are bytes from slot base

struct GimmickPoolSlot {
    bool active = false;  // base minus 0x30 nonzero base minus 0x2C the assigned car
    int32_t car_index = -1;
    float x = 0.0f, y = 0.0f, z = 0.0f;  // base minus 0x28 minus 0x24 minus 0x20 the slot position base minus 0x10 minus 0xC minus 8 the eased target
    float target_x = 0.0f, target_y = 0.0f, target_z = 0.0f;
    float yaw_deg = 0.0f;  // base minus 4 the slot heading base 0x00 respawn follow advance point last distance
    float last_dist = 0.0f;
    GimmickPoolState state = GimmickPoolState::Idle;  // base 0x04 base 0x210 the follow list of the carry
    int32_t follow_list = 0;
    int32_t follow_point = 0;  // base 0x214 the follow point of the carry base 0x1F0 timestamp of the state start
    int64_t state_ms = 0;
    int64_t land_ms = 0;  // base 0x1F8 timestamp the land effect and the drop read base 0x21C the release boost arms on the drop
    int32_t boost_armed = 0;
    int32_t land_played = 0;                   // base 0x94 one after the land effect
};

constexpr int32_t GIMMICK_POOL_SCRIPTED_FLOOR = 200; // gimmick pool slot lookup 0x4B9FE0 states under this are live

// gimmick pool slot lookup 0x4B9FE0 the slot of the car with a state under 200 or minus one
int32_t gimmick_pool_slot_lookup(const std::array<GimmickPoolSlot, GIMMICK_POOL_LIVE_SLOTS>& pool,
                                  int32_t car_index);

// effect hive hit lookup 0x4CF020 one entry of the effect 400 16 slot table at 0x2F077F0 stride 0x160
struct GimmickHiveSlot {
    uint8_t active = 0;  // entry minus 8 one means the slot is live entry plus 0 the car occupying the slot
    int32_t car_index = -1;
    int32_t state = 0;      // entry plus 4 must be 0x69 for a hit match
};

constexpr size_t GIMMICK_HIVE_TABLE_SIZE = 16;  // effect hive hit lookup 0x4CF020 table size and the live state value
constexpr int32_t GIMMICK_HIVE_STATE_HELD = 0x69;

// effect hive hit lookup 0x4CF020 returns the matching slot index or negative one
int32_t effect_hive_hit_lookup(const std::array<GimmickHiveSlot, GIMMICK_HIVE_TABLE_SIZE>& table,
                                int32_t car_index);

// world gimmick hit dispatch 0x4D3950 one class per themed world gimmick kind of the track
enum class GimmickClass : int32_t {
    Ant = 0,  // world gimmick Ant %02d hit test 0x4D49B0 hit test 0x4D7EB0 is a stub returning minus one never fires
    LavaMan = 1,
    MushMan = 2,  // mush png hit test 0x4DB3F0 the chase creature of gimmick mushman update 0x4DB500 hit test 0x4DC680
    Pierrot = 3,
    Scorpion = 4,  // hit test 0x4DCD80 hit test 0x4DF000 it also needs an animation window of 1700 to 2700 ms
    ToyBox = 5,
    CookieMan = 6,  // hit test 0x4D5C60 hit test 0x4D5000
    Chef = 7,
    TreeFairy = 8,  // hit test 0x4DFC80 hit test 0x4DF6E0
    TreeDoor = 9,
    Mole = 10,  // hit test 0x4DB100 it only bites a car under its node hit test 0x4DD3C0
    Sheep = 11,
    Twister = 12,  // hit test 0x4E0D70 spider01 png hit test 0x4DE3B0
    Spider = 13,
    Cobra = 14,  // hit test 0x4D55E0 mission gimmick hit test 0x4D9650 loaded with no world
    MissionEffect = 15,
    MissionMark = 16,  // mission gimmick hit test 0x4DAA20 loaded with no world hit test 0x4D7870
    Glass = 17,
    Turnstile = 18,  // hit test 0x4E0440 and 0x4D62B0 both sweep the four wheel points over their faces
    Door = 19,
    Fountain = 20,  // hit test 0x4D6B10 planar and only for a car under its node hit test 0x4D7200
    Frame = 21
};

constexpr int32_t GIMMICK_CLASS_COUNT = 22; // world gimmick hit dispatch 0x4D3950 classes 0 to 0x15

// car gimmick hit 0x4982D0 two effect codes 0x495C30 classes 0 3 6 7 8 0xB 0xC 0xE 0x15
constexpr int32_t GIMMICK_EFFECT_STUMBLE = 100;
constexpr int32_t GIMMICK_EFFECT_SPINOUT = 300; // classes 1 4 5 0xA 0x14

// car gimmick hit 0x4982D0 case 9 tree door shove two bearings 0x5A6998 yaw at or under takes low bearing
constexpr float GIMMICK_JUMP_YAW_LOW = 70.0f;
constexpr float GIMMICK_JUMP_YAW_HIGH = 250.0f;  // 0x5A6A34 yaw at or over this takes the low bearing 0x49838B the bearing outside the yaw window
constexpr float GIMMICK_JUMP_HEADING_LOW = 160.2f;
constexpr float GIMMICK_JUMP_HEADING_HIGH = 340.20001f;  // 0x49836D the bearing inside the yaw window 0x498368 the push strength of both bearings
constexpr float GIMMICK_JUMP_STRENGTH = 120.0f;

// car gimmick hit 0x4982D0 cases 0x12 and 0x13 boost cancel then push off touched face 0x498450 push strength of both
constexpr float GIMMICK_BLOCK_STRENGTH = 100.0f;
constexpr float GIMMICK_BLOCK_VELOCITY_XY = 0.5f;  // 0x59F414 the x and y velocity kept after the push 0x5A2494 the z velocity kept after the push
constexpr float GIMMICK_BLOCK_VELOCITY_Z = 0.1f;

// world gimmick hit dispatch 0x4D3950 the order the sweep tries the classes first match wins
extern const int32_t GIMMICK_DISPATCH_ORDER[GIMMICK_CLASS_COUNT];

// the hit reach of each class read on its own test function indexed by the class
extern const float GIMMICK_CLASS_REACH[GIMMICK_CLASS_COUNT];

// a plain world point the gimmick rows and the per car reference slot use it
struct GimmickPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// one gimmick instance the track loader filled row shape read on the manager records
struct GimmickInstance {
    float x = 0.0f, y = 0.0f, z = 0.0f;  // the node position every test measures against zero takes the class reach of GIMMICK CLASS REACH
    float radius = 0.0f;
    int32_t gimmickClass = 0;  // class the dispatch hands to car gimmick hit row plus 0 fifth argument of car gimmick hit ignores it
    int32_t id = 0;
    int32_t state = 0;  // row plus 0xAC nonzero rows are skipped by every test row plus 0xB8 the time the row was last touched
    int64_t hitMs = 0;
};

constexpr size_t GIMMICK_REF_POINT_SLOTS = 30; // 0x5F1958 float 90 three per car slot

// world gimmick hit dispatch 0x4D3950 the manager at 0x5F1950 its loaded byte rows and car points
struct GimmickWorld {
    uint8_t loaded = 0;  // 0x5F1954 one once world gimmick load by track 0x4D4180 ran every loaded instance of the track in one list
    std::vector<GimmickInstance> rows;
    std::array<GimmickPoint, GIMMICK_REF_POINT_SLOTS> refPoint{}; // 0x5F1958 the push anchor per car
};

// gimmick mushman update 0x4DB500 the state of one mush row
enum class GimmickMushState : int32_t {
    Idle = 0,  // looking for a racing car inside the find reach closing a quarter of the gap each frame
    Chase = 1,
    Caught = 2,  // held once the chase stopped closing the three shake counters grow until the first passes the cap
    Grow = 3,
    Cooldown = 4  // held then back to the spawn point facing 180
};

// gimmick mushman update 0x4DB500 0xF8 byte row mush manager 0x2F16D50 row base is manager 0xAC plus 0xF8 times index

struct GimmickMushman {
    GimmickMushState state = GimmickMushState::Idle;  // row 0x00 row 0x08 the chased car the hit test at 0x4DB3F0 never writes it
    int32_t carIndex = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;  // row 0x0C 0x10 0x14 the live position row 0x18 the render heading
    float yawDeg = 180.0f;
    float lastDist = 0.0f;  // row 0x1C the planar gap of the frame before row 0x24 the time the current state started
    int64_t stateMs = 0;
    int64_t animMs = 0;  // row 0x2C time idle animation last restarted row 0x34 grows by 5 in grow state and ends it
    int32_t shakeA = 0;
    int32_t shakeB = 0;  // row 0x38 grows by 4 row 0x3C grows by 4
    int32_t shakeC = 0;
    // the six random numbers of the catch in the exe draw order rows 0x40 0x4C 0x44 0x50 0x48 0x54
    int32_t jitter[6] = {};
    float homeX = 0.0f, homeY = 0.0f, homeZ = 0.0f; // 0x5F1AC0 three floats a row the spawn point
};

constexpr float GIMMICK_MUSH_FIND_REACH = 18.0f;  // 0x4DB56D car nearest index 0x499B50 reach 0x5A32D4 a quarter of the gap each frame
constexpr float GIMMICK_MUSH_CHASE_LERP = 0.25f;
constexpr float GIMMICK_MUSH_DIST_SEED = 10000.0f;  // 0x4DB5A8 the first distance of a chase 0x4DB761 and 0x4DB7D1 the two waits
constexpr int64_t GIMMICK_MUSH_HOLD_MS = 3000;
constexpr int64_t GIMMICK_MUSH_ANIM_MS = 2800;  // 0x4DB822 the idle animation restart period 0x4DB78D the first counter step
constexpr int32_t GIMMICK_MUSH_GROW_A = 5;
constexpr int32_t GIMMICK_MUSH_GROW_B = 4;  // 0x4DB786 the other two counter steps 0x4DB797 the first counter ends the grow
constexpr int32_t GIMMICK_MUSH_GROW_CAP = 800;
constexpr int32_t GIMMICK_MUSH_SHAKE_SEED = -50;  // 0x4DB6D4 the three counters at the catch 0x4DB801 the heading of the reset
constexpr float GIMMICK_MUSH_HOME_YAW = 180.0f;
constexpr float GIMMICK_MUSH_CAMERA_SHAKE = 20000.0f;  // 0x4DB5B7 the local car screen shake 0x4DB6E4 onward
constexpr int32_t GIMMICK_MUSH_JITTER_MOD[6] = {400, 300, 100, 100, 100, 100};
constexpr int32_t GIMMICK_MUSH_JITTER_BIAS[6] = {-200, -150, 0, 0, 0, 0};      // 0x4DB6EB and 0x4DB6FE

struct GameState; // car state h owns it this header only takes it by reference

// world gimmick hit dispatch 0x4D3950 the reach rule of one class four wheel points for 0x12 and 0x13
bool gimmick_row_touched(const GimmickInstance& row, float carX, float carY, float carZ,
                          const GimmickPoint* wheelPoints);

// car gimmick hit 0x4982D0 response of one class false when car already runs effect refPoint slot 0x5F1958 pushes 0x12 0x13

bool car_gimmick_hit(GameState& game, int carIndex, int32_t gimmickClass,
                      const GimmickPoint& refPoint, int64_t nowMs);

// world gimmick hit dispatch 0x4D3950 the row index the local car touched this frame or minus one
int32_t world_gimmick_hit_dispatch(GameState& game, int carIndex, GimmickWorld& world,
                                    int64_t nowMs);

// car nearest index 0x499B50 the closest racing car inside the reach or minus one
int32_t car_nearest_index(const GameState& game, float x, float y, float maxDistance,
                           int32_t excludeCarIndex);

// gimmick mushman update 0x4DB500 one mush row per frame the chase the catch and the reset
void gimmick_mushman_update(GameState& game, GimmickMushman& mush, int64_t nowMs);

} // namespace KnC Kart Client
