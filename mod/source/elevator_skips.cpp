#include <common.h>
#include <evt_cmd.h>
#include <spm/evt_map.h>
#include <spm/camdrv.h>
#include <spm/evt_mario.h>
#include <spm/evtmgr_cmd.h>
#include <spm/mario.h>
#include <wii/os.h>

#include "elevator_skips.h"
#include "game_addresses.h"
#include "options_menu.h"
#include "script_patching.h"

namespace mod::elevator_skips {

using namespace mod::script_patching;
using namespace spm::evt_map;
using namespace spm::evt_mario;

// ---- Teleporting Mario within a room ----

/*
    ap_cam_follow(x, y, z): Mario has just been moved from (x, y, z); the camera moves by as much, so it
    starts where it would end up instead of panning over (user, 2026-11-05). Each frame it eases toward
    a spot worked out from Mario, from wherever it is, so it has nowhere left to go.
*/
// The 3D camera's copies of where it is now (CamEntry offsets, unnamed in spm-headers; each a Vec3 whose
// y climbed with the camera after a teleport while its goal, +0x10c, was already at the top)
#define CAM_NOW_1 0x100
#define CAM_NOW_2 0x140
#define CAM_NOW_3 0x14c
#define CAM_NOW_4 0x19c
#define CAM_NOW_5 0x1a8

static EVT_DEFINE_USER_FUNC(ap_cam_follow)
{
    (void) isFirstCall;
    spm::evtmgr::EvtScriptCode * args = evt->pCurData;
    wii::mtx::Vec3 & now = spm::mario::marioGetPtr()->position;
    f32 dx = now.x - spm::evtmgr_cmd::evtGetFloat(evt, args[0]);
    f32 dy = now.y - spm::evtmgr_cmd::evtGetFloat(evt, args[1]);
    f32 dz = now.z - spm::evtmgr_cmd::evtGetFloat(evt, args[2]);
    // The camera that follows Mario is the 3D one (camGetCurPtr gives whichever camera is being drawn).
    // Besides its position and target, it keeps where it is now in five more places, which it moves
    // toward where it wants to be at a capped speed (vertically about 300 units a second; measured
    // 2026-11-05, Flipside Tower): those move too, so there's nothing left to catch up
    spm::camdrv::CamEntry * cam = spm::camdrv::camGetPtr(spm::camdrv::CAM_ID_3D);
    u8 * raw = reinterpret_cast<u8 *>(cam);
    wii::mtx::Vec3 * points[] = {
        &cam->pos, &cam->target,
        reinterpret_cast<wii::mtx::Vec3 *>(raw + CAM_NOW_1), reinterpret_cast<wii::mtx::Vec3 *>(raw + CAM_NOW_2),
        reinterpret_cast<wii::mtx::Vec3 *>(raw + CAM_NOW_3), reinterpret_cast<wii::mtx::Vec3 *>(raw + CAM_NOW_4),
        reinterpret_cast<wii::mtx::Vec3 *>(raw + CAM_NOW_5),
    };
    for (wii::mtx::Vec3 * p : points)
    {
        p->x += dx;
        p->y += dy;
        p->z += dz;
    }
    return 2;
}
static constexpr s32 ap_cam_follow_parameter_count = 3;

// How long Mario stays still after a teleport, so a button pressed twice doesn't send him straight back
#define TELEPORT_SETTLE_MSEC 500

// Puts Mario at (x, y, z), the camera with him. Uses LW(1)-LW(3)
#define TELEPORT_MARIO(x, y, z)                                                                        \
    USER_FUNC(evt_mario_get_pos, LW(1), LW(2), LW(3))                                                  \
    USER_FUNC(evt_mario_set_pos, x, y, z)                                                              \
    USER_FUNC(ap_cam_follow, LW(1), LW(2), LW(3))                                                      \
    USER_FUNC(game_addresses::evt_cam_reset_to_mario)

// evt_map_playanim's mode that jumps an animation to its end
#define ANIM_TO_END 2

// Fort Francis's elevator: where each ride leaves Mario (from the vanilla scripts' last evt_mario_set_pos)
#define FORT_ELEVATOR_X 25
#define FORT_ELEVATOR_TOP_Y 900
#define FORT_ELEVATOR_BOTTOM_Y 0
#define FORT_ELEVATOR_Z -75

EVT_BEGIN(ap_fort_elevator_up)
    USER_FUNC(evt_mario_key_off, 0)
    USER_FUNC(evt_map_playanim, PTR("anm_E_up"), ANIM_TO_END, 0)
    TELEPORT_MARIO(FORT_ELEVATOR_X, FORT_ELEVATOR_TOP_Y, FORT_ELEVATOR_Z)
    WAIT_MSEC(TELEPORT_SETTLE_MSEC)
    USER_FUNC(evt_mario_key_on)
    RETURN()
EVT_END()

EVT_BEGIN(ap_fort_elevator_down)
    USER_FUNC(evt_mario_key_off, 0)
    USER_FUNC(evt_map_playanim, PTR("anm_E_down"), ANIM_TO_END, 0)
    TELEPORT_MARIO(FORT_ELEVATOR_X, FORT_ELEVATOR_BOTTOM_Y, FORT_ELEVATOR_Z)
    WAIT_MSEC(TELEPORT_SETTLE_MSEC)
    USER_FUNC(evt_mario_key_on)
    RETURN()
EVT_END()

// The vanilla ride scripts (relF script offsets)
#define FORT_RIDE_UP 0x8cf58    // relF file 0x13c258
#define FORT_RIDE_DOWN 0x8d778  // relF file 0x13ca78

/*
    The door entries that start the rides: evt_door_select_entry(5, 1, hit 2D, hit 3D, script, 0),
    in ta4_11 (its init script and one other). Word 6 is the script.
*/
static const struct
{
    u32 offset;  // relF script offset of the USER_FUNC
    u32 ride;    // the vanilla ride it starts
    const spm::evtmgr::EvtScriptCode * instant;
    const char * what;
} sFortDoors[] = {
    {0x8e60c, FORT_RIDE_UP, ap_fort_elevator_up, "Fort Francis elevator up (first entry)"},
    {0x8e62c, FORT_RIDE_DOWN, ap_fort_elevator_down, "Fort Francis elevator down (first entry)"},
    {0x8e918, FORT_RIDE_UP, ap_fort_elevator_up, "Fort Francis elevator up (ta4_11 init)"},
    {0x8e938, FORT_RIDE_DOWN, ap_fort_elevator_down, "Fort Francis elevator down (ta4_11 init)"},
};
#define DOOR_SELECT_WORDS 8  // the command word, the function, then its 6 arguments
#define DOOR_SELECT_SCRIPT 6

// ---- Flipside and Flopside Tower's elevators ----

/*
    Each tower has an elevator between its ground floor (button tou_doa, x 0) and its top (button
    sui_03: x -600 in Flipside, 600 in Flopside), both in the same room. The vanilla rides (about 18
    seconds; user's rides, 2026-11-05) are hand-made scenes, so the mod's scripts don't replay them:
    Mario is simply put where a ride leaves him (measured in Flipside: (-600, 1500, -253) and
    (0, 0, -251.5); the towers mirror each other). Like the vanilla up ride, the instant one registers
    the top button again. During a new file's prologue (story value below 11, the hub's value after it)
    the rides carry its scenes, so they play as vanilla then.
*/
#define TOWER_PROLOGUE_END 11
#define TOWER_TOP_Y 1500
#define TOWER_EXIT_Z -250
#define FLIPSIDE_TOP_X -600
#define FLOPSIDE_TOP_X 600

// The vanilla rides (relF script offsets), for the prologue
enum TowerRide
{
    FLIPSIDE_UP,
    FLIPSIDE_DOWN,
    FLOPSIDE_UP,
    FLOPSIDE_DOWN,
};
static const u32 sTowerRides[] = {0x6000, 0x7388, 0x263f8, 0x27208};

// ap_tower_ride(ride, &script): script = that vanilla ride
static EVT_DEFINE_USER_FUNC(ap_tower_ride)
{
    (void) isFirstCall;
    s32 ride = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    spm::evtmgr_cmd::evtSetValue(evt, evt->pCurData[1], reinterpret_cast<s32>(relfScript(sTowerRides[ride])));
    return 2;
}
static constexpr s32 ap_tower_ride_parameter_count = 2;

// The towers' instant scripts. Down: to the ground floor. Up: to the top, then the top button again
// (with the top button's slot variable, as vanilla's up ride does)
EVT_BEGIN(ap_flipside_tower_down)
    IF_SMALL(GSW(0), TOWER_PROLOGUE_END)
        USER_FUNC(ap_tower_ride, FLIPSIDE_DOWN, LW(0))
        RUN_CHILD_EVT(LW(0))
        RETURN()
    END_IF()
    USER_FUNC(evt_mario_key_off, 0)
    TELEPORT_MARIO(0, 0, TOWER_EXIT_Z)
    WAIT_MSEC(TELEPORT_SETTLE_MSEC)
    USER_FUNC(evt_mario_key_on)
    RETURN()
EVT_END()

EVT_BEGIN(ap_flopside_tower_down)
    IF_SMALL(GSW(0), TOWER_PROLOGUE_END)
        USER_FUNC(ap_tower_ride, FLOPSIDE_DOWN, LW(0))
        RUN_CHILD_EVT(LW(0))
        RETURN()
    END_IF()
    USER_FUNC(evt_mario_key_off, 0)
    TELEPORT_MARIO(0, 0, TOWER_EXIT_Z)
    WAIT_MSEC(TELEPORT_SETTLE_MSEC)
    USER_FUNC(evt_mario_key_on)
    RETURN()
EVT_END()

EVT_BEGIN(ap_flipside_tower_up)
    IF_SMALL(GSW(0), TOWER_PROLOGUE_END)
        USER_FUNC(ap_tower_ride, FLIPSIDE_UP, LW(0))
        RUN_CHILD_EVT(LW(0))
        RETURN()
    END_IF()
    USER_FUNC(evt_mario_key_off, 0)
    TELEPORT_MARIO(FLIPSIDE_TOP_X, TOWER_TOP_Y, TOWER_EXIT_Z)
    USER_FUNC(game_addresses::evt_door_select_entry, 5, 1, PTR("A2_sui_03"), PTR("A3_sui_03"), PTR(ap_flipside_tower_down), GW(11))
    WAIT_MSEC(TELEPORT_SETTLE_MSEC)
    USER_FUNC(evt_mario_key_on)
    RETURN()
EVT_END()

EVT_BEGIN(ap_flopside_tower_up)
    IF_SMALL(GSW(0), TOWER_PROLOGUE_END)
        USER_FUNC(ap_tower_ride, FLOPSIDE_UP, LW(0))
        RUN_CHILD_EVT(LW(0))
        RETURN()
    END_IF()
    USER_FUNC(evt_mario_key_off, 0)
    TELEPORT_MARIO(FLOPSIDE_TOP_X, TOWER_TOP_Y, TOWER_EXIT_Z)
    USER_FUNC(game_addresses::evt_door_select_entry, 5, 1, PTR("A2_sui_03"), PTR("A3_sui_03"), PTR(ap_flopside_tower_down), GW(10))
    WAIT_MSEC(TELEPORT_SETTLE_MSEC)
    USER_FUNC(evt_mario_key_on)
    RETURN()
EVT_END()

// The towers' button entries in the rooms' init scripts (same form as Fort Francis's doors)
static const struct
{
    u32 offset;
    u32 ride;
    const spm::evtmgr::EvtScriptCode * instant;
    const char * what;
} sTowerButtons[] = {
    {0x1ba00, 0x6000, ap_flipside_tower_up, "Flipside Tower elevator up (mac_02)"},
    {0x1ba20, 0x7388, ap_flipside_tower_down, "Flipside Tower elevator down (mac_02)"},
    {0x2cc18, 0x263f8, ap_flopside_tower_up, "Flopside Tower elevator up (mac_12)"},
    {0x2cc38, 0x27208, ap_flopside_tower_down, "Flopside Tower elevator down (mac_12)"},
};

// ---- Flipside and Flopside's town elevators ----

// A float as scripts store it (evt_cmd.h FLOAT)
#define FLOAT_WORD(value) (static_cast<s32>((value) * 1024) - 240000000)

// How fast the arriving car's animation plays (each elevator's third table animation, ereveN_03)
#define CAR_ARRIVE_SPEED 8.0f
// The arriving script's evt_map_playanim(car, 10, 0): becomes ap_town_car_arrive
#define TOWN_CAR_ARRIVE 0x36c44

/*
    ap_town_car_arrive(car, mode, 0): the arriving script's evt_map_playanim, then the animation sped
    up to CAR_ARRIVE_SPEED (evt_map_set_anim_speed, given its own two arguments)
*/
static EVT_DEFINE_USER_FUNC(ap_town_car_arrive)
{
    s32 result = spm::evt_map::evt_map_playanim(evt, isFirstCall);
    spm::evtmgr::EvtScriptCode * args = evt->pCurData;
    spm::evtmgr::EvtScriptCode speedArgs[] = {args[0], FLOAT_WORD(CAR_ARRIVE_SPEED)};
    evt->pCurData = speedArgs;
    game_addresses::evt_map_set_anim_speed(evt, true);
    evt->pCurData = args;
    return result;
}
static constexpr s32 ap_town_car_arrive_parameter_count = 3;

/*
    Every town elevator plays the same two scripts: leaving (relF 0xe54f8: doors open, Mario walks in,
    the doors close and the car moves, then the room changes to the elevator table's destination) and
    arriving (0xe5c50: the car arrives, the doors open, Mario walks out). About 8 seconds each (user's
    ride, 2026-11-05). Their bookkeeping and the room change stay vanilla; only their waits become 0 and
    the walking in and out speeds up. The door and car animations still play, nothing waits for them.
*/
#define OP_WAIT_MSEC 10
#define WALK_SPEED_VANILLA FLOAT_WORD(120)
#define WALK_SPEED_FAST FLOAT_WORD(600)

static const struct
{
    u32 offset;     // relF script offset of the command
    s32 command;    // its first word
    s32 word;       // which word changes
    s32 vanilla;    // what it holds
    s32 instant;    // what it becomes
    const char * what;
} sTownWords[] = {
    // Leaving (0xe54f8)
    {0x363fc, EVT_CMD(1, OP_WAIT_MSEC), 1, LW_VAR(11), 0, "town elevator: doors opening"},
    {0x364dc, EVT_CMD(4, OP_USER_FUNC), 4, WALK_SPEED_VANILLA, WALK_SPEED_FAST, "town elevator: partner walks in"},
    {0x364fc, EVT_CMD(4, OP_USER_FUNC), 4, WALK_SPEED_VANILLA, WALK_SPEED_FAST, "town elevator: partner walks in (2)"},
    {0x365d8, EVT_CMD(5, OP_USER_FUNC), 5, WALK_SPEED_VANILLA, WALK_SPEED_FAST, "town elevator: Tippi flies in"},
    {0x365fc, EVT_CMD(5, OP_USER_FUNC), 5, WALK_SPEED_VANILLA, WALK_SPEED_FAST, "town elevator: Tippi flies in (2)"},
    {0x366dc, EVT_CMD(4, OP_USER_FUNC), 4, WALK_SPEED_VANILLA, WALK_SPEED_FAST, "town elevator: Mario walks in"},
    {0x3685c, EVT_CMD(1, OP_WAIT_MSEC), 1, 1000, 0, "town elevator: car starting"},
    {0x368a8, EVT_CMD(1, OP_WAIT_MSEC), 1, LW_VAR(11), 0, "town elevator: car moving"},
    // Arriving (0xe5c50)
    {0x36c58, EVT_CMD(1, OP_WAIT_MSEC), 1, 2000, 0, "town elevator: car arriving"},
    {0x36d10, EVT_CMD(1, OP_WAIT_MSEC), 1, LW_VAR(11), 0, "town elevator: car stopping"},
    {0x36d44, EVT_CMD(1, OP_WAIT_MSEC), 1, 700, 0, "town elevator: car stopped"},
    // (The 500 ms wait before Mario walks out, 0x36d78, stays: the car arrives fast, below, and that
    // half second lets it finish before the doors close; with no wait at all they stayed open, user,
    // 2026-11-05)
    {0x36e34, EVT_CMD(4, OP_USER_FUNC), 4, WALK_SPEED_VANILLA, WALK_SPEED_FAST, "town elevator: partner walks out"},
    {0x36e74, EVT_CMD(5, OP_USER_FUNC), 5, WALK_SPEED_VANILLA, WALK_SPEED_FAST, "town elevator: Tippi flies out"},
    {0x36ea8, EVT_CMD(4, OP_USER_FUNC), 4, WALK_SPEED_VANILLA, WALK_SPEED_FAST, "town elevator: Mario walks out"},
    {0x36fb0, EVT_CMD(1, OP_WAIT_MSEC), 1, 200, 0, "town elevator: camera"},
};

// ---- Switching them on and off (the "Fast elevators" setting) ----

// Every word the elevators change: where it is, and what it holds each way
static struct
{
    EvtScriptCode * at;
    s32 vanilla;
    s32 instant;
} sWords[32];
static s32 sWordCount;
static bool sFast;

// Notes a word to change (already checked against its vanilla value).
static void record(EvtScriptCode * at, s32 instant)
{
    if (sWordCount == static_cast<s32>(sizeof(sWords) / sizeof(sWords[0])))
    {
        wii::os::OSReport("[spmap] elevators: too many words to change\n");
        return;
    }
    sWords[sWordCount++] = {at, *at, instant};
}

// Writes every recorded word's fast or vanilla value. The map scripts are read from relF each time
// they start, so this takes effect from the next ride.
static void apply(bool fast)
{
    for (s32 i = 0; i < sWordCount; i++)
        *sWords[i].at = fast ? sWords[i].instant : sWords[i].vanilla;
    sFast = fast;
}

void onMapChange()
{
    bool fast = options_menu::get(options_menu::OPTION_FAST_ELEVATORS);
    if (fast != sFast)
    {
        apply(fast);
        wii::os::OSReport("[spmap] elevators: %s\n", fast ? "fast" : "vanilla");
    }
}

void init()
{
    s32 town = 0;
    for (auto & w : sTownWords)
    {
        EvtScriptCode * at = relfScript(w.offset);
        if (at[0] != w.command || at[w.word] != w.vanilla)
        {
            wii::os::OSReport("[spmap] script patch: %s isn't what was expected, not patched\n", w.what);
            continue;
        }
        record(&at[w.word], w.instant);
        town++;
    }
    EvtScriptCode * car = relfScript(TOWN_CAR_ARRIVE);
    const s32 carWords[] = {EVT_CMD(4, OP_USER_FUNC), reinterpret_cast<s32>(spm::evt_map::evt_map_playanim)};
    if (matches(car, carWords, 2, "town elevator: car arriving"))
        record(&car[1], reinterpret_cast<s32>(ap_town_car_arrive));
    wii::os::OSReport("[spmap] elevators: %d of %d town elevator waits shortened\n", town,
                      static_cast<s32>(sizeof(sTownWords) / sizeof(sTownWords[0])));

    for (auto & button : sTowerButtons)
    {
        EvtScriptCode * at = relfScript(button.offset);
        const s32 expected[] = {
            EVT_CMD(7, OP_USER_FUNC), reinterpret_cast<s32>(game_addresses::evt_door_select_entry), 5, 1, at[4], at[5],
            reinterpret_cast<s32>(relfScript(button.ride)), at[7],
        };
        if (matches(at, expected, DOOR_SELECT_WORDS, button.what))
            record(&at[DOOR_SELECT_SCRIPT], reinterpret_cast<s32>(button.instant));
    }

    for (auto & door : sFortDoors)
    {
        EvtScriptCode * at = relfScript(door.offset);
        const s32 expected[] = {
            EVT_CMD(7, OP_USER_FUNC), reinterpret_cast<s32>(game_addresses::evt_door_select_entry), 5, 1, at[4], at[5],
            reinterpret_cast<s32>(relfScript(door.ride)), 0,
        };
        if (!matches(at, expected, DOOR_SELECT_WORDS, door.what))
            continue;
        record(&at[DOOR_SELECT_SCRIPT], reinterpret_cast<s32>(door.instant));
    }
    apply(true);  // the setting's default, until a save is loaded
}

}
