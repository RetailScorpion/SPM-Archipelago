#pragma once

/*
    Game functions and data the mod uses that aren't in spm-headers (SeekyCt's list of the game's
    functions).

    These were found while researching the randomizer. Each name here gets its memory address from
    mod/symbols/spm.us0.lst, which also says how each one was found. Addresses are for the US 1.0
    game (R8PE01 revision 0) only.

    "Script functions" (EVT_DECLARE_USER_FUNC) are game functions that the game's scripts call;
    the number is how many arguments they take.
*/

#include <common.h>
#include <evt_cmd.h>
#include <spm/evtmgr.h>
#include <spm/spmario_snd.h>

namespace mod::game_addresses {

extern "C" {

// ---- main.dol (the game's main program) ----

// Tippi's state; its first 2 bytes are flags, and 0x80 among them means Tippi is switched off
void * guideGetWork();

// Script functions that set up a dimension door with a section-select menu:
// evt_door_select_entry(5, 1, hitobjA, hitobjB, script, slot); evt_door_select_set(slot, field, value)
EVT_DECLARE_USER_FUNC(evt_door_select_entry, 6)
EVT_DECLARE_USER_FUNC(evt_door_select_set, 3)

// Script that plays Mario going through a door; LW(0) = the door's name
extern const spm::evtmgr::EvtScriptCode door_enter_evt[];

// The instruction in the "can Mario use his ability" check that reads the story value, which
// blocks flipping before Bestovius (story value 1-15)
extern u32 mario_flip_story_check[];

// The file select's "does this save show Pure Heart N" test: 4 instructions that compare the save's
// story value (r17 = the save) with heart r23's threshold, then skip the heart's picture if lower
extern u32 filesel_heart_check[];

// Gives Mario a status (like a Cursya curse) at a level. spm-headers lists this function at a
// wrong address, so the mod uses its own entry
void mario_status_apply(s32 status, s32 lv);

// Key item menu: the "is it the Floro Sprout" test when a key item is used
extern u32 keyitem_menu_use_check[];
// Key item menu drawing: the "is it the Return Pipe" tests before greying out its picture / name
extern u32 keyitem_icon_pipe_check[];
extern u32 keyitem_name_pipe_check[];

// Pause menu: the two calls that count maps found, which decide whether the (unused) Maps button
// appears and whether the cursor can stop on it, and the function that opens the Maps window
extern u32 pluswin_open_maps_count[];
extern u32 pluswin_check_maps_count[];
void pluswinMapsWinOpen();

// The music track list (name, sound archive number, volume...), filled at startup, ended by an
// entry with no name
extern spm::spmario_snd::BgmEntry * spsbgmlist;

// The game's 3 music players (spm-headers describes them but has no address)
extern spm::spmario_snd::BgmPlayer spsndBgmPlayers[3];

// Turns a music player's volume back up (fade in milliseconds) after it was turned down for a jingle
s32 bgm_volume_restore(s32 player, s32 fadeMsec);

// One entry of the sound effect list
struct SfxEntry
{
    const char * name;
    u16 sound;          // the effect's number in the sound archive
    u8 unknown_0x6[0x14 - 0x6];
};
// The sound effect list (sorted by name, filled at startup) and how many entries it has
extern SfxEntry * spssfxlist;
extern u16 spsnd_sfx_count;

// Is an effect with this name playing? / The id of the one playing (-1 if none)
s32 sfx_check_name(const char * name);
s32 sfx_id_playing_name(const char * name);

// Only used in Fleep's join scene, right after the join. They put something Tippi's pointer uses
// into a Fleep mode (the names are guesses); pointing crashes afterwards if Mario has no Fleep
EVT_DECLARE_USER_FUNC(evt_fleep_mode_on, 1)
EVT_DECLARE_USER_FUNC(evt_fleep_mode_set, 1)

// ---- relF (the file of map scripts, loaded once and kept) ----

// Flipside Tower's dimension doors (door = chapter - 1)

// evt_mac_set_door_dest(doorName, map, entrance): where a door leads
EVT_DECLARE_USER_FUNC(evt_mac_set_door_dest, 3)
// evt_mac_door_section_dest(doorName, section, &map, &entrance): first map of a section (0-3)
EVT_DECLARE_USER_FUNC(evt_mac_door_section_dest, 4)
// evt_mac_door_info(door, &doorName, &hitobjA, &hitobjB, &mapobj, &anim): a door's parts
EVT_DECLARE_USER_FUNC(evt_mac_door_info, 6)
// evt_mac_door_objs(door, &mapobjOff, &mapobjOn): the door's open/closed models
EVT_DECLARE_USER_FUNC(evt_mac_door_objs, 3)
// evt_mac_door_msgs(door, &msg, &selectMsg): "This door leads to Chapter N" and the section list
EVT_DECLARE_USER_FUNC(evt_mac_door_msgs, 3)

// Script a section-select door runs when used
extern const spm::evtmgr::EvtScriptCode mac_door_select_evt[];
// The tower's door setup script, run when the tower loads
extern const spm::evtmgr::EvtScriptCode mac_door_setup_evt[];
// The instruction in the tower's start-up script that runs mac_door_setup_evt (2 words)
extern spm::evtmgr::EvtScriptCode mac_tower_run_door_setup[];

// The start of relF's section 5 (map scripts and data). Script patches are written as offsets from
// here: an offset into relF.rel minus 0xAF300 (where section 5 starts in the file)
extern spm::evtmgr::EvtScriptCode relF_section5[];

}

}
