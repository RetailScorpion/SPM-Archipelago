#include <common.h>
#include <msl/string.h>
#include <spm/spmario.h>
#include <spm/spmario_snd.h>
#include <wii/os.h>

#include "code_patching.h"
#include "game_addresses.h"
#include "options_menu.h"
#include "sound_effect_loops.h"
#include "sound_effect_shuffle.h"

namespace mod::sound_effect_shuffle {

// The game's effect list has 1822 names; this leaves room to spare
#define MAX_EFFECTS 2048
// Effects with this ending are the TV speaker copy of the effect with the same name without it
#define TV_SUFFIX "_TV"
#define TV_SUFFIX_LENGTH 3
// Room for an effect's name, when one is put together
#define NAME_BUFFER 64

// What __spsndSFXOn returns when it plays nothing
#define NO_EFFECT -1

// ---- Which effects take part ----

/*
    Only some kinds of effect are shuffled for now (RetailScorpion's choice: shuffling everything
    sounded awful until good and bad swaps are sorted out). Each kind only swaps among itself, and
    every other effect plays as normal:
      - the player's: the characters' (names starting SFX_P_: steps, jumps, landings, voices) and the
        Pixls' (SFX_F_)
      - the interface's: menus, text boxes, saving and the HUD (SFX_SYS_), and the Save Block's
*/
enum Kind
{
    KIND_NONE,
    KIND_PLAYER,
    KIND_INTERFACE,
    KIND_COUNT
};

// The kind of effect a name is, from how it starts.
static Kind kindOf(const char * name)
{
    if (msl::string::strncmp(name, "SFX_P_", 6) == 0 || msl::string::strncmp(name, "SFX_F_", 6) == 0)
        return KIND_PLAYER;
    if (msl::string::strncmp(name, "SFX_SYS_", 8) == 0 || msl::string::strncmp(name, "SFX_MOBJ_SAVE_BLOCK", 19) == 0)
        return KIND_INTERFACE;
    return KIND_NONE;
}

// Whether the effect with sound archive number `sound` loops (from the table tools/sfxloops.py made).
static bool loops(u16 sound)
{
    return (sSfxLoops[sound / 8] >> (sound % 8)) & 1;
}

// Whether a name is a TV speaker copy (ends in _TV).
static bool isTvVariant(const char * name)
{
    u32 length = msl::string::strlen(name);
    return length > TV_SUFFIX_LENGTH && msl::string::strcmp(name + length - TV_SUFFIX_LENGTH, TV_SUFFIX) == 0;
}

// Which kind an effect is shuffled with, or KIND_NONE if it isn't shuffled: it must be a sound
// effect from the archive, and not a TV speaker copy (those follow their base effect).
static Kind poolOf(const game_addresses::SfxEntry & effect)
{
    if (effect.sound == 0 || effect.sound >= SFX_LOOP_SOUND_COUNT || isTvVariant(effect.name))
        return KIND_NONE;
    return kindOf(effect.name);
}

// ---- The shuffle ----

static u16 sShuffled[MAX_EFFECTS];  // for each entry in the effect list: the entry played instead
static s32 sCount;                  // entries in the effect list
static u32 sSeed;
static bool sBuilt;

// The position of an effect in the game's list, or -1. The list is sorted by name, so this looks
// halfway each time ("binary search").
static s32 find(const char * name)
{
    s32 low = 0, high = sCount - 1;
    while (low <= high)
    {
        s32 middle = (low + high) / 2;
        s32 order = msl::string::strcmp(game_addresses::spssfxlist[middle].name, name);
        if (order == 0)
            return middle;
        if (order < 0)
            low = middle + 1;
        else
            high = middle - 1;
    }
    return -1;
}

// Turns the save file's name into a number, always the same for the same name ("FNV-1a" hash).
// It starts from a different value than the music's, so effects don't follow the music's pattern.
static u32 hashName(const char * name)
{
    u32 hash = 2166136261u ^ 0x5F5F5F5Fu;
    for (; *name; name++)
        hash = (hash ^ static_cast<u8>(*name)) * 16777619u;
    return hash;
}

// The next number in a repeatable random sequence ("xorshift").
static u32 nextRandom(u32 & state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// Shuffles one group (one kind, looping or not) among itself.
static void shuffleGroup(Kind kind, bool looping, u32 & randomState)
{
    static u16 members[MAX_EFFECTS];
    static u16 order[MAX_EFFECTS];
    s32 count = 0;
    for (s32 i = 0; i < sCount; i++)
    {
        const game_addresses::SfxEntry & effect = game_addresses::spssfxlist[i];
        if (poolOf(effect) == kind && loops(effect.sound) == looping)
            members[count++] = static_cast<u16>(i);
    }

    // Shuffle a copy of the group (from the end, swap each place with a random place before it)
    msl::string::memcpy(order, members, count * sizeof(u16));
    for (s32 i = count - 1; i > 0; i--)
    {
        s32 j = nextRandom(randomState) % (i + 1);
        u16 swap = order[i];
        order[i] = order[j];
        order[j] = swap;
    }
    for (s32 i = 0; i < count; i++)
        sShuffled[members[i]] = order[i];
}

// Builds the shuffle for a seed. Effects that aren't shuffled play themselves.
static void build(u32 seed)
{
    sCount = game_addresses::spsnd_sfx_count;
    if (sCount > MAX_EFFECTS)
        sCount = MAX_EFFECTS;
    for (s32 i = 0; i < sCount; i++)
        sShuffled[i] = static_cast<u16>(i);

    u32 randomState = seed ? seed : 1;
    shuffleGroup(KIND_PLAYER, false, randomState);
    shuffleGroup(KIND_PLAYER, true, randomState);
    shuffleGroup(KIND_INTERFACE, false, randomState);
    shuffleGroup(KIND_INTERFACE, true, randomState);
    sSeed = seed;
    sBuilt = true;
    wii::os::OSReport("[spmap] sfxshuffle: %d effects shuffled for \"%s\"\n", sCount, spm::spmario::gp->saveName);
}

// The replacement for a TV speaker copy "X_TV": the TV copy of X's replacement, so both speakers
// play the same sound. Falls back to the name itself if there's no such copy.
static const char * tvReplacement(const char * name)
{
    char base[NAME_BUFFER];
    u32 baseLength = msl::string::strlen(name) - TV_SUFFIX_LENGTH;
    if (baseLength >= sizeof(base))
        return name;
    msl::string::memcpy(base, name, baseLength);
    base[baseLength] = '\0';
    s32 i = find(base);
    if (i < 0)
        return name;

    char tv[NAME_BUFFER];
    const char * other = game_addresses::spssfxlist[sShuffled[i]].name;
    u32 otherLength = msl::string::strlen(other);
    if (otherLength + TV_SUFFIX_LENGTH + 1 > sizeof(tv))
        return name;
    msl::string::memcpy(tv, other, otherLength);
    msl::string::memcpy(tv + otherLength, TV_SUFFIX, TV_SUFFIX_LENGTH + 1);  // with the ending 0
    s32 j = find(tv);
    return j < 0 ? name : game_addresses::spssfxlist[j].name;
}

// The effect to play instead of `name` (itself if the setting is off or it isn't shuffled).
static const char * replacement(const char * name)
{
    if (name == nullptr || game_addresses::spssfxlist == nullptr || !options_menu::get(options_menu::OPTION_SFX_SHUFFLE))
        return name;
    u32 seed = hashName(spm::spmario::gp->saveName);
    if (!sBuilt || seed != sSeed)
        build(seed);

    if (isTvVariant(name))
        return tvReplacement(name);
    s32 i = find(name);
    return i < 0 ? name : game_addresses::spssfxlist[sShuffled[i]].name;
}

// ---- Low health beeping ----

// Whether an effect is the beeping the HUD repeats while health is low.
static bool isLowHealthSound(const char * name)
{
    return msl::string::strncmp(name, "SFX_SYS_PINCH1", 14) == 0 || msl::string::strncmp(name, "SFX_SYS_DANGER1", 15) == 0;
}

// ---- Hooks ----

// Runs instead of the game's "play a sound effect": plays nothing for muted low health beeping
// (checked before any shuffling), otherwise the effect's replacement.
static s32 (*__spsndSFXOnReal)(const char * name, u8 volume, u8 pan, s32 delay, wii::mtx::Vec3 * position, u32 param6);
static s32 __spsndSFXOnHook(const char * name, u8 volume, u8 pan, s32 delay, wii::mtx::Vec3 * position, u32 param6)
{
    if (name != nullptr && !options_menu::get(options_menu::OPTION_LOW_HEALTH_SOUND) && isLowHealthSound(name))
        return NO_EFFECT;
    return __spsndSFXOnReal(replacement(name), volume, pan, delay, position, param6);
}

// The game's "is effect X playing?" and "which id is effect X?" look for X's replacement, since
// that's what is playing for X. Without this, the game could keep restarting a looping effect.
static s32 (*sfxCheckNameReal)(const char * name);
static s32 sfxCheckNameHook(const char * name)
{
    return sfxCheckNameReal(replacement(name));
}

static s32 (*sfxIdPlayingNameReal)(const char * name);
static s32 sfxIdPlayingNameHook(const char * name)
{
    return sfxIdPlayingNameReal(replacement(name));
}

void init()
{
    __spsndSFXOnReal = code_patching::hookFunction(spm::spmario_snd::__spsndSFXOn, __spsndSFXOnHook);
    sfxCheckNameReal = code_patching::hookFunction(game_addresses::sfx_check_name, sfxCheckNameHook);
    sfxIdPlayingNameReal = code_patching::hookFunction(game_addresses::sfx_id_playing_name, sfxIdPlayingNameHook);
    wii::os::OSReport("[spmap] sfxshuffle: ready\n");
}

}
