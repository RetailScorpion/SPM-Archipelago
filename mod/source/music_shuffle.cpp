#include <common.h>
#include <msl/string.h>
#include <spm/spmario.h>
#include <spm/spmario_snd.h>
#include <wii/os.h>

#include "code_patching.h"
#include "game_addresses.h"
#include "music_shuffle.h"
#include "options_menu.h"

namespace mod::music_shuffle {

// The track list holds at most this many tracks (the game's own lookup stops there)
#define MAX_TRACKS 150

/*
    Tracks that play once (fanfares, chapter starts, the credits...): the ones whose music file has
    its "loop" setting off. Found by reading each track's .brstm file header on the disc (through
    sound/wiimario_snd.brsar), US 1.0.
    The game waits for some of these to finish; a looping song in their place never would. And a
    jingle in a looping song's place leaves silence once it ends. So they only trade with each other.
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

// Whether a track plays once (is in the list above).
static bool isPlayOnce(const char * name)
{
    for (const char * once : sPlayOnce)
    {
        if (msl::string::strcmp(name, once) == 0)
            return true;
    }
    return false;
}

// ---- The shuffle ----

static s32 sTrackCount;
static u8 sShuffled[MAX_TRACKS];  // for each track number: the number of the track played instead
static u32 sSeed;                 // the seed the shuffle was made with
static bool sBuilt;

// Turns the save file's name into a number, always the same for the same name ("FNV-1a" hash).
static u32 hashName(const char * name)
{
    u32 hash = 2166136261u;
    for (; *name; name++)
        hash = (hash ^ static_cast<u8>(*name)) * 16777619u;
    return hash;
}

// The next number in a repeatable random sequence ("xorshift"): the same seed always gives the
// same sequence.
static u32 nextRandom(u32 & state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// Shuffles the tracks in one group (looping, or play-once) among themselves.
static void shuffleGroup(bool playOnce, u32 & randomState)
{
    spm::spmario_snd::BgmEntry * list = game_addresses::spsbgmlist;
    u8 members[MAX_TRACKS] = {};
    s32 count = 0;
    for (s32 i = 0; i < sTrackCount; i++)
    {
        if (isPlayOnce(list[i].name) == playOnce)
            members[count++] = static_cast<u8>(i);
    }

    // Shuffle a copy of the group (the "Fisher-Yates" method: from the end, swap each place with a
    // random place before it)
    u8 order[MAX_TRACKS];
    msl::string::memcpy(order, members, count);
    for (s32 i = count - 1; i > 0; i--)
    {
        s32 j = nextRandom(randomState) % (i + 1);
        u8 swap = order[i];
        order[i] = order[j];
        order[j] = swap;
    }
    // Each track plays the one now in its place
    for (s32 i = 0; i < count; i++)
        sShuffled[members[i]] = order[i];
}

// Builds the shuffle for a seed: looping tracks first, then the play-once tracks.
static void build(u32 seed)
{
    sTrackCount = 0;
    while (sTrackCount < MAX_TRACKS && game_addresses::spsbgmlist[sTrackCount].name != nullptr)
        sTrackCount++;

    u32 randomState = seed ? seed : 1;
    shuffleGroup(false, randomState);
    shuffleGroup(true, randomState);
    sSeed = seed;
    sBuilt = true;
    wii::os::OSReport("[spmap] musicshuffle: %d tracks shuffled for \"%s\"\n", sTrackCount, spm::spmario::gp->saveName);
}

// The track to play instead of `name`, if its group's setting (Music shuffle or Shuffle fanfares)
// is on; otherwise `name` itself.
static const char * replacement(const char * name)
{
    bool playOnce = isPlayOnce(name);
    if (!options_menu::get(playOnce ? options_menu::OPTION_FANFARE_SHUFFLE : options_menu::OPTION_MUSIC_SHUFFLE))
        return name;
    u32 seed = hashName(spm::spmario::gp->saveName);
    if (!sBuilt || seed != sSeed)
        build(seed);
    spm::spmario_snd::BgmEntry * list = game_addresses::spsbgmlist;
    for (s32 i = 0; i < sTrackCount; i++)
    {
        if (msl::string::strcmp(list[i].name, name) == 0)
            return list[sShuffled[i]].name;
    }
    return name;
}

// ---- Letting a shuffled jingle play to the end ----

/*
    The game times what comes after a jingle by the original jingle's length. For example, getting
    an item stops music player 2 (the jingle) and turns the others back up after 1 to 3 seconds. So
    while a replacement jingle is still playing:
      - a request to stop its player, or to start another track on it, waits until it ends
      - turning the other players' volume back up waits too
    The game has 3 music players. Each track request says which one in the lowest 4 bits of its flags.
*/
#define MUSIC_PLAYERS 3
#define FLAGS_PLAYER_BITS 0xF
// The longest a jingle is waited for: 20 seconds (the credits count as a play-once track)
#define HOLD_MAX_FRAMES (60 * 20)

// What's being held back on one music player
struct Hold
{
    bool active;         // a replacement jingle was started on this player
    s32 frames;          // how long it has been playing
    bool stopWaiting;    // the game asked to stop the player meanwhile
    s32 stopFade;
    bool trackWaiting;   // the game asked to start a track on it meanwhile (its original name)
    u32 trackFlags;
    const char * trackName;
    s32 trackFade;
};
static Hold sHolds[MUSIC_PLAYERS];
static bool sVolumeRestoreWaiting[MUSIC_PLAYERS];
static s32 sVolumeRestoreFade[MUSIC_PLAYERS];

// Whether a music player is still playing its track (the sound library clears the handle when a
// stream ends).
static bool stillPlaying(s32 player)
{
    spm::spmario_snd::BgmPlayer & p = game_addresses::spsndBgmPlayers[player];
    return p.bgmEntry != nullptr && p.soundHandle.mSound != nullptr;
}

// Whether a player is playing a replacement jingle that's still going.
static bool holding(s32 player)
{
    return player >= 0 && player < MUSIC_PLAYERS && sHolds[player].active && stillPlaying(player);
}

// Whether any player is.
static bool anyHolding()
{
    for (s32 player = 0; player < MUSIC_PLAYERS; player++)
    {
        if (holding(player))
            return true;
    }
    return false;
}

// Starts a track (or its replacement), and notes whether it's a replacement jingle to hold for.
static s32 (*spsndBGMOn_f_dReal)(u32 flags, const char * name, s32 fadeMsec);
static s32 startTrack(u32 flags, const char * name, s32 fadeMsec)
{
    s32 player = flags & FLAGS_PLAYER_BITS;
    const char * played = name != nullptr && game_addresses::spsbgmlist != nullptr ? replacement(name) : name;
    s32 result = spsndBGMOn_f_dReal(flags, played, fadeMsec);
    if (player < MUSIC_PLAYERS)
        sHolds[player] = Hold{played != name && isPlayOnce(name), 0, false, 0, false, 0, nullptr, 0};
    return result;
}

// Runs instead of the game's "start a track". Every song starts here.
static s32 spsndBGMOn_f_dHook(u32 flags, const char * name, s32 fadeMsec)
{
    s32 player = flags & FLAGS_PLAYER_BITS;
    if (holding(player))
    {
        Hold & h = sHolds[player];
        h.stopWaiting = false;
        h.trackWaiting = true;
        h.trackFlags = flags;
        h.trackName = name;
        h.trackFade = fadeMsec;
        return 1;
    }
    return startTrack(flags, name, fadeMsec);
}

// Runs instead of the game's "stop a music player".
static bool (*spsndBGMOff_f_dReal)(s32 player, u32 fadeoutTime);
static bool spsndBGMOff_f_dHook(s32 player, u32 fadeoutTime)
{
    if (holding(player))
    {
        Hold & h = sHolds[player];
        h.trackWaiting = false;
        h.stopWaiting = true;
        h.stopFade = fadeoutTime;
        return true;
    }
    return spsndBGMOff_f_dReal(player, fadeoutTime);
}

// Runs instead of the game's "turn a music player back up".
static s32 (*bgmVolumeRestoreReal)(s32 player, s32 fadeMsec);
static s32 bgmVolumeRestoreHook(s32 player, s32 fadeMsec)
{
    if (player >= 0 && player < MUSIC_PLAYERS && anyHolding())
    {
        sVolumeRestoreWaiting[player] = true;
        sVolumeRestoreFade[player] = fadeMsec;
        return 1;
    }
    return bgmVolumeRestoreReal(player, fadeMsec);
}

void tick()
{
    // A held jingle that's over (or ran too long): do what the game asked for meanwhile
    for (s32 player = 0; player < MUSIC_PLAYERS; player++)
    {
        Hold & h = sHolds[player];
        if (!h.active)
            continue;
        if (stillPlaying(player) && ++h.frames < HOLD_MAX_FRAMES)
            continue;
        Hold done = h;
        h.active = false;
        if (done.stopWaiting)
            spsndBGMOff_f_dReal(player, done.stopFade);
        else if (done.trackWaiting)
            startTrack(done.trackFlags, done.trackName, done.trackFade);
    }

    // Once no jingle is held, turn the other players back up
    if (anyHolding())
        return;
    for (s32 player = 0; player < MUSIC_PLAYERS; player++)
    {
        if (sVolumeRestoreWaiting[player])
        {
            sVolumeRestoreWaiting[player] = false;
            bgmVolumeRestoreReal(player, sVolumeRestoreFade[player]);
        }
    }
}

void init()
{
    spsndBGMOn_f_dReal = code_patching::hookFunction(spm::spmario_snd::spsndBGMOn_f_d, spsndBGMOn_f_dHook);
    spsndBGMOff_f_dReal = code_patching::hookFunction(spm::spmario_snd::spsndBGMOff_f_d, spsndBGMOff_f_dHook);
    bgmVolumeRestoreReal = code_patching::hookFunction(game_addresses::bgm_volume_restore, bgmVolumeRestoreHook);
    wii::os::OSReport("[spmap] musicshuffle: ready\n");
}

}
