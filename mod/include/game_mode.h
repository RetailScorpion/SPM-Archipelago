#pragma once

#include <common.h>
#include <spm/nandmgr.h>
#include <spm/seqdrv.h>

/*
    Archipelago files and vanilla files (user, 2026-11-08). Making a new file asks which it is, in the
    game's own choice box (as the Save Block's "Save?" uses), right after "Use this name?" on the file
    select (user: not during the opening); the answer is saved with the file (AP_GSW_MODE) when it
    starts. Should that not happen, the file's first map asks instead. A vanilla file plays the game as it is, story order, items and all, keeping
    only the quality-of-life features (the Options settings: skipping text and the cutscenes inside
    the story, fast climbing and elevators, enemy culling, the shuffles; and the 5-1 block hints). The
    file select shows the Archipelago icon by Archipelago files (file_select_hearts).

    Most of the randomizer is written into the game's memory once at startup: map script commands
    changed, instructions branched to the mod, function hooks, item names and icons. Every such write
    goes through this module, which keeps the word that was there before. Whenever a map loads, apply()
    puts all of them back to the game's own for a vanilla file, or in again for an Archipelago file.
    The randomizer's every-frame and map-change work is skipped for a vanilla file (main.cpp).
*/

namespace mod::game_mode {

// Whether the loaded file is a vanilla file.
bool vanilla();

// Whether a save on the memory card is a vanilla file / an Archipelago file (the file select).
bool fileIsVanilla(const spm::nandmgr::SaveFile * save);
bool fileIsArchipelago(const spm::nandmgr::SaveFile * save);

// A new file that hasn't been asked yet: its first map loads as it is, nothing of the mod's runs, and
// the question comes up on it; the answer loads that map again. Returns true while that's so. Call
// first on every map change.
bool onMapChange(spm::seqdrv::SeqWork * wp);

// Whether the loaded file is still waiting for its answer (the mod's other work waits too). A new file
// with the file select's answer gets it here.
bool asking();

// Drops the file select's answer. Call on the title screen (before the next file select).
void forgetAnswer();

// The file select is showing (a new file may follow). Call every frame on the file select.
void onFileSelect();

// Puts the question up once the new file's map is running. Call every frame.
void tick();

// The question's text (game_messages), or nullptr.
const char * message(const char * msgName);

// Asks on the file select, right after a new file's name is confirmed (the in-game question is then
// only a fallback). Call once at startup.
void init();

// Writes a randomizer-only word (a script command's argument, a pointer in a table...).
void writeWord(void * at, u32 value);

// The same for a 2-byte value (an item's icon).
void writeHalf(void * at, u16 value);

// Writes a randomizer-only instruction.
void writeInstruction(u32 * at, u32 instruction);

// Writes a randomizer-only branch at `at` to `destination` (a call with link = true).
void writeBranch(void * at, void * destination, bool link = false);

// A branch written straight away, not undone (hook's trampoline, in the mod's own memory).
void writeBranchRaw(void * at, void * destination);

// Any 4-byte value or pointer, as writeWord.
template <typename T>
void set(T * at, T value)
{
    static_assert(sizeof(T) == 4, "set writes 4-byte values");
    u32 word;
    __builtin_memcpy(&word, &value, sizeof(word));
    writeWord(at, word);
}

// A randomizer-only hook: as code_patching::hookFunction, with the branch at the start of `function`
// taken out for vanilla files (the game's function then runs as it is).
template <typename Func>
Func hook(Func function, Func replacement)
{
    u32 * instructions = reinterpret_cast<u32 *>(function);
    u32 * trampoline = new u32[2];
    trampoline[0] = instructions[0];
    writeBranchRaw(&trampoline[1], &instructions[1]);
    writeBranch(&instructions[0], reinterpret_cast<void *>(replacement));
    return reinterpret_cast<Func>(trampoline);
}

// Puts the randomizer's writes in for an Archipelago file or takes them out for a vanilla one. Call
// on every map change, once a new file has its settings (cutscene_skips::onMapChange).
void apply();

}
