#pragma once

#include <common.h>
#include <spm/evtmgr.h>

/*
    Tools for changing the game's scripts while it runs.

    Most of the story (cutscenes, NPCs, what a room does when you enter it) is written as scripts:
    lists of numbered commands such as "IF the story value is SMALLER than 25, run this block".
    Each command is stored as several 32-bit words in memory:
      - first word: the command number (its "opcode") and how many arguments follow
      - then the arguments: plain numbers, or encoded references to variables (a save byte, a save
        flag, or one of the script's own local variables)
    The mod changes the story's rules by rewriting single words, for example making an IF read one
    of the mod's own save bytes instead of the story value.

    Every patch first checks that the words are exactly what the mod expects (matches()). If they
    aren't (a different version of the game, say), the patch is skipped and a message is logged,
    so a mismatch can't corrupt a script.

    Patch locations are given as byte offsets: from the start of relF's script section for map
    scripts (relfScript), or from a known script in the main program.
*/

namespace mod::script_patching {

using spm::evtmgr::EvtScriptCode;

// ---- Command numbers (opcodes), from evt_cmd.h ----
#define OP_IF_EQUAL 24
#define OP_IF_NOT_EQUAL 25
#define OP_IF_SMALL 26
#define OP_IF_LARGE 27
#define OP_IF_SMALL_EQUAL 28
#define OP_IF_LARGE_EQUAL 29
#define OP_SWITCH 34
#define OP_CASE_SMALL 38
#define OP_SET 50
#define OP_USER_FUNC 92
#define OP_RUN_EVT 93

// The first word of a command: its argument count and command number
#define EVT_CMD(argc, op) (((argc) << 16) | (op))

// How scripts refer to variables: a fixed large negative number plus the variable's number
#define GSW_VAR(id) (-170000000 + (id))   // save byte id (GSW 0 is the story value)
#define GSWF_VAR(id) (-130000000 + (id))  // save flag id
#define LW_VAR(id) (-30000000 + (id))     // the script's own local variable id

// The script command at `offset` bytes into relF's script section.
EvtScriptCode * relfScript(u32 offset);

// Checks that the `count` words at `at` are exactly `expected`. If not, logs that the patch named
// `what` was skipped, and returns false.
bool matches(const EvtScriptCode * at, const s32 * expected, s32 count, const char * what);

/*
    Shared patch: CASE_SMALL(value) -> CASE_SMALL(GSW(saveByte)).
    A SWITCH case "smaller than value" then compares with a save byte the mod keeps up to date, so
    the mod decides when that case applies.
*/
void caseComparesWithSaveByte(EvtScriptCode * at, s32 value, s32 saveByte, const char * what);

// The game script functions skipCall can switch off
enum SkippableCall
{
    CALL_DOOR_ENABLE_DISABLE,  // evt_door_enable_disable_map_door_desc: bar or unbar a door
    CALL_SOUND_3D,             // evt_snd_sfxon_3d: play a sound effect at a spot
    CALL_MAP_ANIMATION,        // evt_map_playanim: play a map animation
    CALL_ADD_ITEM,             // evt_pouch_add_item: give Mario an item
};

/*
    Shared patch: a call to one of the functions above (with argCount arguments) is replaced with
    a call that does nothing.
*/
void skipCall(EvtScriptCode * at, s32 argCount, SkippableCall call, const char * what);

}
