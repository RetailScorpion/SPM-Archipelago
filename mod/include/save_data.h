#pragma once

#include <common.h>

/*
    The randomizer's own data inside the save file.

    The game keeps its story state in two big tables that are written to the save file whole:
      - GSW: "global save words", 2048 numbers of one byte each (byte 0 is the story value)
      - GSWF: "global save flags", 8192 on/off flags
    The mod reserves the end of each table for itself. A scan of every game script, plus every place
    the game's code reads or writes these tables, found nothing above GSW 534 or GSWF 1327, so the
    reserved ranges below are never touched by the game.

    These numbers must never change: saves made with earlier versions depend on them, and the
    Archipelago client and the helper tools read some of them too.
*/

namespace mod::save_data {

// ---- Reserved GSW bytes: 1792-2047 ----

#define AP_GSW_BASE 1792

// Each chapter's own story value: 8 chapters x 2 bytes, 0 = chapter not started (story_progress)
#define AP_GSW_STORY_SLOTS (AP_GSW_BASE + 0)
// What the patched Pixl join scenes read instead of the story value (pixl_joins), 3 bytes
#define AP_GSW_JOIN_STAGES (AP_GSW_BASE + 16)
// What Flipside Tower's patched "Peach join scene" check compares with (character_joins)
#define AP_GSW_PEACH_SCENE (AP_GSW_BASE + 19)
// How many items from the Archipelago server this save has handled, 2 bytes (received_items)
#define AP_GSW_RECV_COUNT (AP_GSW_BASE + 20)
// What Bestovius's patched talk script compares with (npc_story_fixes)
#define AP_GSW_LESSON (AP_GSW_BASE + 22)
// What Watchitt's patched "go get help" check compares with (npc_story_fixes)
#define AP_GSW_WATCHITT (AP_GSW_BASE + 23)
// Peach's revival chain's own story value, 60-63 (character_joins)
#define AP_GSW_PEACH_CHAIN (AP_GSW_BASE + 24)
// The pause menu's settings: one bit per setting, set = changed from its default (options_menu)
#define AP_GSW_OPTIONS (AP_GSW_BASE + 25)
// The options menu's settings 9-16 (bit n - 8 = setting n), next to the first eight's
#define AP_GSW_OPTIONS_2 (AP_GSW_BASE + 28)
// Where Chapter 6's door leads, which Flipside Tower's patched door tests read (hub_story_fixes):
// 0 Sammer's Kingdom (the story's duels), 1 the World of Nothing, 2 post-game Sammer's Kingdom
#define AP_GSW_DOOR_6 (AP_GSW_BASE + 26)
// How many Progressive Wallets the save has received (wallet)
#define AP_GSW_WALLET (AP_GSW_BASE + 29)
// Archipelago or vanilla file, asked when a new file starts (game_mode): 0 not asked yet (a file from
// before the question counts as Archipelago), 1 Archipelago, 2 vanilla
#define AP_GSW_MODE (AP_GSW_BASE + 30)
#define FILE_MODE_UNSET 0
#define FILE_MODE_ARCHIPELAGO 1
#define FILE_MODE_VANILLA 2
// What the first Sammer duel room's patched "scripted scenes" case compares with (cutscene_skips):
// 228 plays King Sammer's scenes as in vanilla, 0 never does
#define AP_GSW_SAMMER_SCENES (AP_GSW_BASE + 27)
// Key items in storage: 32 slots x 2 bytes, 0 = empty slot (key_item_storage)
#define AP_GSW_KEY_STORAGE (AP_GSW_BASE + 32)
// Locations collected offline, before the client put the seed's items in the game: 32 bytes, bit
// (index & 7) of byte (index >> 3) for the location at that index (offline_checks)
#define AP_GSW_OFFLINE_CHECKS (AP_GSW_BASE + 96)
#define AP_GSW_OFFLINE_CHECKS_BYTES 32
// Mario's gravity when the game was saved (mario.h MarioGravityType, 0 = down): the game doesn't keep
// it, so a save made on a Whoa Zone wall would load with Mario the right way up, off his wall (save_block)
#define AP_GSW_GRAVITY (AP_GSW_BASE + 128)
// The step reached in checks whose scene spans several visits, 8 bytes, 0 = none (check_stages)
#define AP_GSW_CHECK_STEPS (AP_GSW_BASE + 129)
// The seed's starting doors, kept from the client's last connection so a save loaded before the client
// connects has them open (pure_hearts): bit N - 1 = door N, STARTING_DOORS_SAVED once written
#define AP_GSW_STARTING_DOORS (AP_GSW_BASE + 137)
#define STARTING_DOORS_SAVED 0x80

// The values of AP_GSW_PEACH_CHAIN, copied from vanilla's story values for the same steps:
// 60 Peach lies unconscious in Flipside, 61 Saffron was asked for soup, 62 the Spicy Soup is
// cooked, 63 Peach has joined
#define PEACH_CHAIN_START 60
#define PEACH_CHAIN_SOUP 62
#define PEACH_CHAIN_DONE 63

// ---- Reserved GSWF flags: 7168-8191 ----

#define AP_GSWF_BASE 7168

// Mario can flip (only used when the flip option is "random")
#define AP_GSWF_FLIP (AP_GSWF_BASE + 0)
// Pixl join locations collected: one flag per story Pixl, in item order (Thoreau 221 .. Dottie 228)
#define AP_GSWF_JOIN_BASE (AP_GSWF_BASE + 1)
#define AP_GSWF_JOIN(pixlItemId) (AP_GSWF_JOIN_BASE + (pixlItemId) - 221)
// Story gift locations collected, one flag each, in story_gifts' order (Peach, Luigi, Helmet,
// Bowser, Bestovius, Brobot L-type)
#define AP_GSWF_GIFT_BASE (AP_GSWF_BASE + 16)
#define AP_GSWF_GIFT(index) (AP_GSWF_GIFT_BASE + (index))
// Pure Hearts held, one flag per heart 1-8; heart N opens tower door N (pure_hearts)
#define AP_GSWF_HEART_BASE (AP_GSWF_BASE + 24)
#define AP_GSWF_HEART(n) (AP_GSWF_HEART_BASE + (n) - 1)
// Chapter Pure Heart locations collected: one flag per chapter 1-7 (pure_hearts), near the end of the
// reserved range so the item spots below can keep growing
#define AP_GSWF_CHAPTER_HEART_BASE (AP_GSWF_BASE + 1008)
#define AP_GSWF_CHAPTER_HEART(chapter) (AP_GSWF_CHAPTER_HEART_BASE + (chapter) - 1)
// The throne room door in 8-4 (endgame_requirements): opened with the Throne Room Key, and the key
// given once the final battle's requirement is met
#define AP_GSWF_THRONE_DOOR_OPEN (AP_GSWF_BASE + 1016)
#define AP_GSWF_THRONE_KEY_GIVEN (AP_GSWF_BASE + 1017)
// The starting character (starting_character.h): switched to once, and Mario's item received
#define AP_GSWF_START_CHARACTER_SET (AP_GSWF_BASE + 1018)
#define AP_GSWF_MARIO_HELD (AP_GSWF_BASE + 1019)
// 7-2's second lock (an2_03 "lock2", Door Key 37) has been opened (npc_story_fixes)
#define AP_GSWF_UNDERWHERE_LOCK (AP_GSWF_BASE + 1020)
// The client has placed a seed for this file at least once (connect_gate)
#define AP_GSWF_CONNECTED (AP_GSWF_BASE + 1021)
// 5-3's lock (gn3_03, Cave Key 33) has been opened (npc_story_fixes)
#define AP_GSWF_CRAG_LOCK (AP_GSWF_BASE + 1022)
// 7-1's lock to Luigi's rooms (an1_03, Door Key 49) is open (npc_story_fixes)
#define AP_GSWF_LUIGI_LOCK (AP_GSWF_BASE + 1023)
// Item spots that have no save flag of their own get one from here, in spot_flags' order
#define AP_GSWF_LOC_BASE (AP_GSWF_BASE + 32)
// Tattle checks: one flag per kind of enemy, in location_flags.h TATTLE_CARDS' order (tattle_checks.h)
#define AP_GSWF_TATTLE_BASE (AP_GSWF_BASE + 512)

// ---- Vanilla flags the mod relies on ----

// Tippi is with Mario: the save code writes it, and loading a save restores her from it
#define GSWF_TIPPI_PRESENT 403

// Reads one reserved GSW byte (0-255).
u8 getGswByte(s32 id);

// Writes one reserved GSW byte (0-255).
void setGswByte(s32 id, u8 value);

// Reads a 2-byte number stored in GSW bytes id and id + 1 (high byte first).
u16 getGswU16(s32 id);

// Writes a 2-byte number into GSW bytes id and id + 1 (high byte first).
void setGswU16(s32 id, u16 value);

}
