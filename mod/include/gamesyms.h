#pragma once

/*
    Game functions and data that aren't in spm-headers, found while researching the randomizer.
    Their addresses are in symbols/spm.<version>.lst (US 1.0 only so far).
*/

#include <common.h>
#include <evt_cmd.h>
#include <spm/evtmgr.h>

namespace mod::gamesyms {

extern "C" {

// main.dol

// Tippi's guide work; its first halfword is flag0 (0x80 = Tippi disabled)
void * guideGetWork();

// Sets up a dimension door with a section-select menu.
// evt_door_select_entry(5, 1, hitobjA, hitobjB, script, slot); evt_door_select_set(slot, field, value)
EVT_DECLARE_USER_FUNC(evt_door_select_entry, 6)
EVT_DECLARE_USER_FUNC(evt_door_select_set, 3)

// Plays going through a door; LW(0) = map door desc name
extern const spm::evtmgr::EvtScriptCode door_enter_evt[];

// The "bl evtGetValue(GSW(0))" instruction in the ability check that blocks Mario's flip while
// 0 < story value < 16 (before Bestovius)
extern u32 mario_flip_story_check[];

// The file select's "does this save show Pure Heart N" test: four instructions comparing the
// save's GSW 0 (r17 = save file) with heart r23's threshold, then blt past the heart's icon
extern u32 filesel_heart_check[];

// Only in Fleep's join scene, after the join: evt_fleep_mode_on(1) and evt_fleep_mode_set(1) put
// something Tippi's pointer uses into a Fleep mode (names are guesses); pointing crashes without Fleep
EVT_DECLARE_USER_FUNC(evt_fleep_mode_on, 1)
EVT_DECLARE_USER_FUNC(evt_fleep_mode_set, 1)

// relF: Flipside Tower dimension doors (door = chapter - 1)

// evt_mac_set_door_dest(mapDoorDesc, map, entrance)
EVT_DECLARE_USER_FUNC(evt_mac_set_door_dest, 3)
// evt_mac_door_section_dest(mapDoorDesc, section, &map, &entrance): first map of section (0-3)
EVT_DECLARE_USER_FUNC(evt_mac_door_section_dest, 4)

// evt_mac_door_info(door, &mapDoorDesc, &hitobjA, &hitobjB, &mapobj, &anim)
EVT_DECLARE_USER_FUNC(evt_mac_door_info, 6)
// evt_mac_door_objs(door, &mapobjOff, &mapobjOn)
EVT_DECLARE_USER_FUNC(evt_mac_door_objs, 3)
// evt_mac_door_msgs(door, &msg, &selectMsg): "This door leads to Chapter N" and the section list
EVT_DECLARE_USER_FUNC(evt_mac_door_msgs, 3)

// Script a section-select door runs when used
extern const spm::evtmgr::EvtScriptCode mac_door_select_evt[];
// Vanilla door setup script, run from the tower's init script
extern const spm::evtmgr::EvtScriptCode mac_door_setup_evt[];
// The RUN_CHILD_EVT(mac_door_setup_evt) instruction in the tower's init script (2 words)
extern spm::evtmgr::EvtScriptCode mac_tower_run_door_setup[];

// relF: start of section 5 (map scripts and data). Script patches are tables of offsets from here:
// relF.rel file offset - 0xAF300 (see joins.cpp)
extern spm::evtmgr::EvtScriptCode relF_section5[];

}

}
