#include <common.h>
#include <msl/string.h>
#include <spm/spmario.h>
#include <spm/spmario_snd.h>
#include <wii/os.h>

#include "gamesyms.h"
#include "musicshuffle.h"
#include "options.h"
#include "patch.h"

namespace mod::musicshuffle {

// spsbgmlist holds at most this many tracks (spsndBGMOn_f_d's lookup loop)
#define MAX_TRACKS 150

/*
    Tracks the game waits to finish (fanfares and jingles: item get, level up, game over...). A
    looping song in their place would never finish, so they only trade places with each other.
*/
static bool isFanfare(const char * name)
{
    return msl::string::strncmp(name, "BGM_FF_", 7) == 0
        || msl::string::strcmp(name, "BGM_MINI_GAMEOVER1") == 0
        || msl::string::strcmp(name, "BGM_EVT_STG7_RPG_FF1") == 0;
}

static s32 sTrackCount;
static u8 sShuffled[MAX_TRACKS];  // track index -> index of the track played instead
static u32 sSeed;
static bool sBuilt;

static u32 hashName(const char * name)
{
    // FNV-1a
    u32 hash = 2166136261u;
    for (; *name; name++)
        hash = (hash ^ static_cast<u8>(*name)) * 16777619u;
    return hash;
}

static u32 nextRandom(u32 & state)
{
    // xorshift32
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// Shuffles each group (fanfares, the rest) among itself
static void build(u32 seed)
{
    spm::spmario_snd::BgmEntry * list = gamesyms::spsbgmlist;
    sTrackCount = 0;
    while (sTrackCount < MAX_TRACKS && list[sTrackCount].name != nullptr)
        sTrackCount++;

    u32 state = seed ? seed : 1;
    for (s32 group = 0; group < 2; group++)
    {
        u8 members[MAX_TRACKS];
        s32 count = 0;
        for (s32 i = 0; i < sTrackCount; i++)
        {
            if (isFanfare(list[i].name) == (group == 1))
                members[count++] = static_cast<u8>(i);
        }
        u8 order[MAX_TRACKS];
        msl::string::memcpy(order, members, count);
        for (s32 i = count - 1; i > 0; i--)
        {
            s32 j = nextRandom(state) % (i + 1);
            u8 t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
        for (s32 i = 0; i < count; i++)
            sShuffled[members[i]] = order[i];
    }
    sSeed = seed;
    sBuilt = true;
    wii::os::OSReport("[spmap] musicshuffle: %d tracks shuffled for \"%s\"\n", sTrackCount, spm::spmario::gp->saveName);
}

static const char * replacement(const char * name)
{
    u32 seed = hashName(spm::spmario::gp->saveName);
    if (!sBuilt || seed != sSeed)
        build(seed);
    spm::spmario_snd::BgmEntry * list = gamesyms::spsbgmlist;
    for (s32 i = 0; i < sTrackCount; i++)
    {
        if (msl::string::strcmp(list[i].name, name) == 0)
            return list[sShuffled[i]].name;
    }
    return name;
}

static s32 (*spsndBGMOn_f_dReal)(u32 flags, const char * name, s32 fadeMsec);
static s32 spsndBGMOn_f_dHook(u32 flags, const char * name, s32 fadeMsec)
{
    if (name != nullptr && gamesyms::spsbgmlist != nullptr && options::get(options::OPTION_MUSIC_SHUFFLE))
        name = replacement(name);
    return spsndBGMOn_f_dReal(flags, name, fadeMsec);
}

void init()
{
    spsndBGMOn_f_dReal = patch::hookFunction(spm::spmario_snd::spsndBGMOn_f_d, spsndBGMOn_f_dHook);
    wii::os::OSReport("[spmap] musicshuffle: ready\n");
}

}
