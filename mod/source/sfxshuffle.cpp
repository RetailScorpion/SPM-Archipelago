#include <common.h>
#include <msl/string.h>
#include <spm/spmario.h>
#include <spm/spmario_snd.h>
#include <wii/os.h>

#include "gamesyms.h"
#include "options.h"
#include "patch.h"
#include "sfxloops_data.h"
#include "sfxshuffle.h"

namespace mod::sfxshuffle {

// The game's effect table has 1822 names; room to spare
#define MAX_EFFECTS 2048
#define TV_SUFFIX "_TV"

static u16 sShuffled[MAX_EFFECTS];  // table index -> index of the effect played instead
static s32 sCount;
static u32 sSeed;
static bool sBuilt;

static bool loops(u16 sound)
{
    return (sSfxLoops[sound / 8] >> (sound % 8)) & 1;
}

static bool isTvVariant(const char * name)
{
    u32 len = msl::string::strlen(name);
    return len > 3 && msl::string::strcmp(name + len - 3, TV_SUFFIX) == 0;
}

/*
    Only some effects take part for now (user's choice: shuffling everything sounds awful until good
    and bad swaps are sorted out), each kind swapping among itself; every other effect stays as it is:
      - the player's: the characters' (SFX_P_: steps, jumps, landings, voices) and the Pixls' (SFX_F_)
      - the interface's: menus, text boxes, saving and the HUD (SFX_SYS_), and the Save Block's
*/
enum Kind
{
    KIND_NONE,
    KIND_PLAYER,
    KIND_INTERFACE,
    KIND_COUNT
};

static Kind kindOf(const char * name)
{
    if (msl::string::strncmp(name, "SFX_P_", 6) == 0 || msl::string::strncmp(name, "SFX_F_", 6) == 0)
        return KIND_PLAYER;
    if (msl::string::strncmp(name, "SFX_SYS_", 8) == 0 || msl::string::strncmp(name, "SFX_MOBJ_SAVE_BLOCK", 19) == 0)
        return KIND_INTERFACE;
    return KIND_NONE;
}

// Which pool an effect is shuffled in (KIND_NONE: not shuffled): a sound from the archive, and not a
// TV speaker variant (those follow their base effect)
static Kind poolOf(const gamesyms::SfxEntry & e)
{
    if (e.sound == 0 || e.sound >= SFX_LOOP_SOUND_COUNT || isTvVariant(e.name))
        return KIND_NONE;
    return kindOf(e.name);
}

// The table is sorted by name (the game looks names up with bsearch)
static s32 find(const char * name)
{
    s32 lo = 0, hi = sCount - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        s32 c = msl::string::strcmp(gamesyms::spssfxlist[mid].name, name);
        if (c == 0)
            return mid;
        if (c < 0)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

static u32 hashName(const char * name)
{
    // FNV-1a, salted so effects don't follow the same pattern as the music
    u32 hash = 2166136261u ^ 0x5F5F5F5Fu;
    for (; *name; name++)
        hash = (hash ^ static_cast<u8>(*name)) * 16777619u;
    return hash;
}

static u32 nextRandom(u32 & state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// Within each kind, looping effects swap with looping effects, the rest with the rest
static void build(u32 seed)
{
    sCount = gamesyms::spsnd_sfx_count;
    if (sCount > MAX_EFFECTS)
        sCount = MAX_EFFECTS;
    for (s32 i = 0; i < sCount; i++)
        sShuffled[i] = static_cast<u16>(i);

    u32 state = seed ? seed : 1;
    static u16 members[MAX_EFFECTS];
    static u16 order[MAX_EFFECTS];
    for (s32 group = 0; group < (KIND_COUNT - 1) * 2; group++)
    {
        Kind kind = static_cast<Kind>(KIND_PLAYER + group / 2);
        bool looping = group % 2;
        s32 count = 0;
        for (s32 i = 0; i < sCount; i++)
        {
            const gamesyms::SfxEntry & e = gamesyms::spssfxlist[i];
            if (poolOf(e) == kind && loops(e.sound) == looping)
                members[count++] = static_cast<u16>(i);
        }
        msl::string::memcpy(order, members, count * sizeof(u16));
        for (s32 i = count - 1; i > 0; i--)
        {
            s32 j = nextRandom(state) % (i + 1);
            u16 t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
        for (s32 i = 0; i < count; i++)
            sShuffled[members[i]] = order[i];
    }
    sSeed = seed;
    sBuilt = true;
    wii::os::OSReport("[spmap] sfxshuffle: %d effects shuffled for \"%s\"\n", sCount, spm::spmario::gp->saveName);
}

static const char * replacement(const char * name)
{
    if (name == nullptr || gamesyms::spssfxlist == nullptr || !options::get(options::OPTION_SFX_SHUFFLE))
        return name;
    u32 seed = hashName(spm::spmario::gp->saveName);
    if (!sBuilt || seed != sSeed)
        build(seed);

    if (isTvVariant(name))
    {
        // X_TV plays the TV speaker version of X's replacement, so both speakers agree
        char base[64];
        u32 len = msl::string::strlen(name) - 3;
        if (len >= sizeof(base))
            return name;
        msl::string::memcpy(base, name, len);
        base[len] = '\0';
        s32 i = find(base);
        if (i < 0)
            return name;
        char tv[64];
        const char * other = gamesyms::spssfxlist[sShuffled[i]].name;
        u32 otherLen = msl::string::strlen(other);
        if (otherLen + 4 > sizeof(tv))
            return name;
        msl::string::memcpy(tv, other, otherLen);
        msl::string::memcpy(tv + otherLen, TV_SUFFIX, 4);
        s32 j = find(tv);
        return j < 0 ? name : gamesyms::spssfxlist[j].name;
    }
    s32 i = find(name);
    return i < 0 ? name : gamesyms::spssfxlist[sShuffled[i]].name;
}

// The beeping the HUD repeats while health is low (Low health beeping setting)
static bool isLowHealthSound(const char * name)
{
    return msl::string::strncmp(name, "SFX_SYS_PINCH1", 14) == 0 || msl::string::strncmp(name, "SFX_SYS_DANGER1", 15) == 0;
}

// Starting an effect plays its replacement (or nothing: muted low health beeping, whatever replaces it)
static s32 (*__spsndSFXOnReal)(const char * name, u8 volume, u8 pan, s32 delay, wii::mtx::Vec3 * position, u32 param6);
static s32 __spsndSFXOnHook(const char * name, u8 volume, u8 pan, s32 delay, wii::mtx::Vec3 * position, u32 param6)
{
    if (name != nullptr && !options::get(options::OPTION_LOW_HEALTH_SOUND) && isLowHealthSound(name))
        return -1;  // what __spsndSFXOn returns when it plays nothing
    return __spsndSFXOnReal(replacement(name), volume, pan, delay, position, param6);
}

// "Is X playing" looks for X's replacement, which is what's playing for it
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
    __spsndSFXOnReal = patch::hookFunction(spm::spmario_snd::__spsndSFXOn, __spsndSFXOnHook);
    sfxCheckNameReal = patch::hookFunction(gamesyms::sfx_check_name, sfxCheckNameHook);
    sfxIdPlayingNameReal = patch::hookFunction(gamesyms::sfx_id_playing_name, sfxIdPlayingNameHook);
    wii::os::OSReport("[spmap] sfxshuffle: ready\n");
}

}
