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
#include <spm/itemdrv.h>
#include <spm/npcdrv.h>
#include <spm/seq_title.h>
#include <spm/spmario_snd.h>
#include <spm/seq_load_sub.h>
#include <spm/wpadmgr.h>
#include <wii/gx.h>
#include <wii/mtx.h>

namespace mod::game_addresses {

extern "C" {

// ---- main.dol (the game's main program) ----

// Tippi's state; its first 2 bytes are flags, and 0x80 among them means Tippi is switched off
void * guideGetWork();

// Script functions that set up a dimension door with a section-select menu:
// evt_door_select_entry(5, 1, hitobjA, hitobjB, script, slot); evt_door_select_set(slot, field, value)
EVT_DECLARE_USER_FUNC(evt_door_select_entry, 6)
EVT_DECLARE_USER_FUNC(evt_door_select_set, 3)

// evt_map_set_anim_speed(anim name, f32 speed): a map animation's playback speed (1 = normal)
EVT_DECLARE_USER_FUNC(evt_map_set_anim_speed, 2)

// evt_cam_reset_to_mario(): what every vanilla elevator ride calls right after moving Mario. The camera
// catches up at once, road cameras included (Fort Francis's; tested by the user, 2026-11-05)
EVT_DECLARE_USER_FUNC(evt_cam_reset_to_mario, 0)

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

// The title screen's work, while the title screen is up: its animation (animNum 0 = the opening,
// 1 = the loop) and the frame it's at
extern spm::seq_title::SeqTitleWork * seq_title_wp;

// The file select's bottom row (Erase Data, Copy Data, Back): each button's picture and position
struct FileselBottomButton
{
    u32 picture;
    f32 x;
    f32 y;
};
extern FileselBottomButton fileselect_bottom_buttons[3];
// Its buttons' cursor table: 0-3 the files, 4-6 the bottom row; copy mode's table follows
extern spm::seq_load_sub::SeqLoadButtonDef fileselect_button_defs[14];
// Draws one of the current TPL's pictures (the file select's buttons) at a position, tinted
void fileselDrawPicture(s32 picture, wii::mtx::Mtx34 * mtx, wii::gx::GXColor * colour);

// The Wii Remotes' state; each controller's buttons this frame are statuses[controller][0]
extern spm::wpadmgr::WpadWork * wpadmgr_wp;
// In the title's drawing: the instruction that animates its layout (a call through the layout's
// list of functions, with r3 = the layout and r4 = 0), just before the panes' positions are worked out
extern u32 title_layout_animate_call[];

// The Pixl ability table: what pressing 1 does for each Pixl id (0-terminated). Each function gets the
// Pixl id and a mode and returns whether the ability started
struct PixlAbility
{
    s32 pixl;
    s32 (*use)(s32 pixl, s32 mode);
};
extern PixlAbility pixl_ability_table[];
// Tippi's tattle text for an NPC (her pointer): its catch card's text with its HP and attack, or her own
const char * searchGetNpcMsg(spm::npcdrv::NPCEntry * npc);
// Tippi's tattle text for an item lying in a level (her pointer's kind 5): by item number, "that's an
// item", "that's important"...
const char * search_item_msg(spm::itemdrv::ItemEntry * item);
// Fleep's ability (what pressing 1 does with him): starts his pointer motion (0x26)
s32 fleep_ability(s32 pixl, s32 mode);
// In Fleep's motion and its end: "li r3, 0xe2", his Pixl id, before looking up his Pixl
extern u32 fleep_motion_pixl_id_load[];
extern u32 fleep_end_pixl_id_load[];
// Changes a Pixl's animation (by name; the Pixl's display sets it)
void fairy_change_anim(void * fairy, const char * anim);
// Fleep's ability (what pressing 1 does with him): starts his pointer motion (0x26)
s32 fleep_ability(s32 pixl, s32 mode);
// In Fleep's motion and its end: "li r3, 0xe2", his Pixl id, before looking up his Pixl
extern u32 fleep_motion_pixl_id_load[];
extern u32 fleep_end_pixl_id_load[];
// Changes a Pixl's animation (by name; the Pixl's display sets it)
void fairy_change_anim(void * fairy, const char * anim);

// A Pixl's work by its id, or nullptr if there's none
void * fairy_work_by_id(s32 id);
// In Mario's flip: the call that looks up his Pixl, whose result is used without checking for none
extern u32 flip_pixl_lookup_call[];
// In Tippi's pointer code: the story value compare that stops pointing below 12
extern u32 tippi_pointer_story_check[];
// In Mario's climbing: the adds of the stick's direction to his climbing move (2D and 3D)
extern u32 climb_2d_vertical_add_1[];
extern u32 climb_2d_vertical_add_2[];
extern u32 climb_2d_sideways_add[];
extern u32 climb_3d_vertical_add_1[];
extern u32 climb_3d_vertical_add_2[];

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
