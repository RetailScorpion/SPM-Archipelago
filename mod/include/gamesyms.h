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

// relF: Thoreau (1-2). Each points at a script instruction the mod patches (see joins.cpp).
extern spm::evtmgr::EvtScriptCode he2_07_thoreau_gate_spawn[];  // IF_SMALL(GSW(0), 25): Thoreau and his chest
extern spm::evtmgr::EvtScriptCode he2_07_thoreau_gate_lock[];   // IF_SMALL(GSW(0), 25): exit locked until Thoreau frees you
extern spm::evtmgr::EvtScriptCode he2_07_thoreau_gate_bars[];   // IF_SMALL(GSW(0), 25): iron bars close behind Mario
extern spm::evtmgr::EvtScriptCode he2_07_thoreau_join_gift[];   // USER_FUNC(evt_pouch_add_item, 221)
extern spm::evtmgr::EvtScriptCode he2_03_thoreau_safety_net[];  // IF_EQUAL(LW(0), 0): give Thoreau at the end of 1-2

}

}
