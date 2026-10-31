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
    Tracks that play once (their stream's loop flag is off: fanfares, chapter starts, the credits...).
    The game waits for some of them to finish, and a looping song in their place never would, while
    a jingle in a looping song's place leaves silence after it. So they only trade with each other.
    Found by reading each track's .brstm header (sound/, via wiimario_snd.brsar), US 1.0.
*/
static const char * const sPlayOnce[] = {
    "BGM_EVT_OPENING1", "BGM_EVT_STAFF1", "BGM_EVT_YADO1", "BGM_EVT_HAMERU1",
    "BGM_EVT_MAP_START1", "BGM_EVT_MAP_START2", "BGM_EVT_MAP_START3", "BGM_EVT_MAP_START4",
    "BGM_EVT_MAP_START5", "BGM_EVT_MAP_START6", "BGM_EVT_MAP_START7", "BGM_EVT_MAP_START8",
    "BGM_EVT_HOUKAI1", "BGM_EVT_STG7_RPG_FF1", "BGM_EVT_BOOK_OPEN1", "BGM_EVT_HAPPY_FLOWER1",
    "BGM_FF_AREA_CLEAR1", "BGM_FF_AREA_START1", "BGM_FF_GAMEOVER1", "BGM_FF_MISS1",
    "BGM_FF_CORRECT1", "BGM_FF_FAULT1", "BGM_FF_UMAI1", "BGM_FF_CORRECT2",
    "BGM_FF_COMPANION1", "BGM_FF_ITEM_GET1", "BGM_FF_PURE_HEART_GET1_S2", "BGM_FF_FAIRLIN_GET1",
    "BGM_FF_KEYITEM_GET1", "BGM_FF_LEVELUP1", "BGM_FF_ZIGENWAZA_GET1", "BGM_FF_CARD_GET1",
};

static bool isPlayOnce(const char * name)
{
    for (const char * once : sPlayOnce)
    {
        if (msl::string::strcmp(name, once) == 0)
            return true;
    }
    return false;
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

// Shuffles each group (looping tracks, tracks that play once) among itself
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
            if (isPlayOnce(list[i].name) == (group == 1))
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

// The track played instead, if its group's setting (Music shuffle / Shuffle fanfares) is on
static const char * replacement(const char * name)
{
    bool once = isPlayOnce(name);
    if (!options::get(once ? options::OPTION_FANFARE_SHUFFLE : options::OPTION_MUSIC_SHUFFLE))
        return name;
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

/*
    A shuffled jingle plays all the way through. The game times what follows a jingle by the original
    one (the item window stops music player 2 and brings the others back up after 1-3 seconds), so
    while a replacement jingle is still playing, what the game asks of its player (stop it, or start
    another track on it) waits until it ends, and so does bringing the other players' volume back up.
*/
#define BGM_PLAYERS 3
#define BGM_FLAGS_PLAYER 0xF
// Longest wait for a jingle (the credits are a play-once track too, minutes long)
#define HOLD_MAX_FRAMES (60 * 20)

struct Hold
{
    bool active;        // a replacement jingle was started on this player
    s32 frames;
    bool pendingStop;
    s32 stopFade;
    bool pendingOn;     // a track the game started meanwhile (the original name)
    u32 onFlags;
    const char * onName;
    s32 onFade;
};
static Hold sHolds[BGM_PLAYERS];
static bool sRestorePending[BGM_PLAYERS];
static s32 sRestoreFade[BGM_PLAYERS];

static bool stillPlaying(s32 player)
{
    spm::spmario_snd::BgmPlayer & p = gamesyms::spsndBgmPlayers[player];
    return p.bgmEntry != nullptr && p.soundHandle.mSound != nullptr;
}

static bool holding(s32 player)
{
    return player >= 0 && player < BGM_PLAYERS && sHolds[player].active && stillPlaying(player);
}

static bool anyHolding()
{
    for (s32 i = 0; i < BGM_PLAYERS; i++)
    {
        if (holding(i))
            return true;
    }
    return false;
}

static s32 (*spsndBGMOn_f_dReal)(u32 flags, const char * name, s32 fadeMsec);
static s32 startTrack(u32 flags, const char * name, s32 fadeMsec)
{
    s32 player = flags & BGM_FLAGS_PLAYER;
    const char * played = name != nullptr && gamesyms::spsbgmlist != nullptr ? replacement(name) : name;
    s32 result = spsndBGMOn_f_dReal(flags, played, fadeMsec);
    if (player < BGM_PLAYERS)
        sHolds[player] = Hold{played != name && isPlayOnce(name), 0, false, 0, false, 0, nullptr, 0};
    return result;
}

static s32 spsndBGMOn_f_dHook(u32 flags, const char * name, s32 fadeMsec)
{
    s32 player = flags & BGM_FLAGS_PLAYER;
    if (holding(player))
    {
        Hold & h = sHolds[player];
        h.pendingStop = false;
        h.pendingOn = true;
        h.onFlags = flags;
        h.onName = name;
        h.onFade = fadeMsec;
        return 1;
    }
    return startTrack(flags, name, fadeMsec);
}

static bool (*spsndBGMOff_f_dReal)(s32 player, u32 fadeoutTime);
static bool spsndBGMOff_f_dHook(s32 player, u32 fadeoutTime)
{
    if (holding(player))
    {
        Hold & h = sHolds[player];
        h.pendingOn = false;
        h.pendingStop = true;
        h.stopFade = fadeoutTime;
        return true;
    }
    return spsndBGMOff_f_dReal(player, fadeoutTime);
}

static s32 (*bgmVolumeRestoreReal)(s32 player, s32 fadeMsec);
static s32 bgmVolumeRestoreHook(s32 player, s32 fadeMsec)
{
    if (player >= 0 && player < BGM_PLAYERS && anyHolding())
    {
        sRestorePending[player] = true;
        sRestoreFade[player] = fadeMsec;
        return 1;
    }
    return bgmVolumeRestoreReal(player, fadeMsec);
}

void tick()
{
    for (s32 i = 0; i < BGM_PLAYERS; i++)
    {
        Hold & h = sHolds[i];
        if (!h.active)
            continue;
        if (stillPlaying(i) && ++h.frames < HOLD_MAX_FRAMES)
            continue;
        // The jingle is over (or ran too long): do what the game asked meanwhile
        Hold done = h;
        h.active = false;
        if (done.pendingStop)
            spsndBGMOff_f_dReal(i, done.stopFade);
        else if (done.pendingOn)
            startTrack(done.onFlags, done.onName, done.onFade);
    }
    if (!anyHolding())
    {
        for (s32 i = 0; i < BGM_PLAYERS; i++)
        {
            if (sRestorePending[i])
            {
                sRestorePending[i] = false;
                bgmVolumeRestoreReal(i, sRestoreFade[i]);
            }
        }
    }
}

void init()
{
    spsndBGMOn_f_dReal = patch::hookFunction(spm::spmario_snd::spsndBGMOn_f_d, spsndBGMOn_f_dHook);
    spsndBGMOff_f_dReal = patch::hookFunction(spm::spmario_snd::spsndBGMOff_f_d, spsndBGMOff_f_dHook);
    bgmVolumeRestoreReal = patch::hookFunction(gamesyms::bgm_volume_restore, bgmVolumeRestoreHook);
    wii::os::OSReport("[spmap] musicshuffle: ready\n");
}

}
