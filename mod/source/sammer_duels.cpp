#include <common.h>
#include <msl/stdio.h>
#include <msl/string.h>
#include <wii/os.h>

#include "mailbox.h"
#include "sammer_duels.h"
#include "script_patching.h"
#include "story_progress.h"

namespace mod::sammer_duels {

#define ENTRANCE_DOOR "doa1_l"   // each duel room's left door, from the room before
#define CHAPTER_6_POST_GAME 421  // from here Sammer's Kingdom has its 100 duels

#define STORY_GROUP 0
#define STORY_DUELS 20           // gates 1-20; Rolling Thwomp is gate 20
#define STORY_WALK_ROOM 22       // wa1_22: gate 21, the first room whose Sammer Guy gives up
#define STORY_LAST_ROOM 26       // wa1_26: gate 25, then the save prompt and the Pure Heart scene
#define STORY_GATE_25_WON 271    // Chapter 6's value once gate 25 is won
#define STORY_ROOMS 0xd1938      // the story's Sammer room table (relF section 5), 10 words per gate
#define STORY_ROOM_WORDS 10
#define STORY_ROOM_WON 4         // word: Chapter 6's value once the room's duel is won

#define POST_GAME_GROUP_GATES 25

// The room Mario is sent to instead (the map name has to outlive this call)
static char sTarget[16];

// Whether every gate in a group plays: as in vanilla, so nothing changes.
static bool allGates(u32 mask, s32 gates)
{
    return mask == 0 || mask == (1u << gates) - 1;
}

// The first gate from `gate` on (1-based) that plays in a group; its last always does.
static s32 nextGate(u32 mask, s32 gate, s32 gates)
{
    while (gate < gates && !(mask & (1u << (gate - 1))))
        gate++;
    return gate;
}

// Reads a Sammer map name "waG_NN". Returns false for any other map.
static bool parseRoom(const char * map, s32 * group, s32 * room)
{
    if (map[0] != 'w' || map[1] != 'a' || map[2] < '1' || map[2] > '4' || map[3] != '_' || map[4] < '0'
        || map[4] > '9' || map[5] < '0' || map[5] > '9' || map[6] != '\0')
        return false;
    *group = map[2] - '0';
    *room = (map[4] - '0') * 10 + (map[5] - '0');
    return true;
}

// Whether Mario is going forwards into a room: by its left door, or a group's start (no door).
static bool forwards(spm::seqdrv::SeqWork * wp)
{
    return wp->p1 == nullptr || wp->p1[0] == '\0' || msl::string::strcmp(wp->p1, ENTRANCE_DOOR) == 0;
}

static void sendTo(spm::seqdrv::SeqWork * wp, s32 group, s32 room)
{
    msl::stdio::sprintf(sTarget, "wa%d_%02d", group, room);
    wii::os::OSReport("[spmap] sammer: %s -> %s\n", wp->p0, sTarget);
    wp->p0 = sTarget;
    wp->p1 = ENTRANCE_DOOR;
}

// The story: wa1_03-wa1_21 are gates 2-20, wa1_22 the first room after Rolling Thwomp.
static void storyRoom(spm::seqdrv::SeqWork * wp, s32 group, s32 room)
{
    u32 mask = mailbox::gMailbox.sammerGates[STORY_GROUP];
    if (group != 1 || allGates(mask, STORY_DUELS) || msl::string::strcmp(wp->p1 ? wp->p1 : "", ENTRANCE_DOOR) != 0)
        return;
    if (room == STORY_WALK_ROOM)
    {
        sendTo(wp, 1, STORY_LAST_ROOM);
        story_progress::advanceChapter(6, STORY_GATE_25_WON);
        return;
    }
    s32 gate = room - 1;
    if (gate < 2 || gate > STORY_DUELS)
        return;
    s32 next = nextGate(mask, gate, STORY_DUELS);
    if (next == gate)
        return;
    sendTo(wp, 1, next + 1);
    // The gate before it counts as won, and every one before that
    const s32 * rooms = script_patching::relfScript(STORY_ROOMS);
    story_progress::advanceChapter(6, rooms[(next - 2) * STORY_ROOM_WORDS + STORY_ROOM_WON]);
}

// The post-game: group 1's rooms are wa1_02-wa1_26, the others' waG_01-waG_25.
static void postGameRoom(spm::seqdrv::SeqWork * wp, s32 group, s32 room)
{
    u32 mask = mailbox::gMailbox.sammerGates[group];
    if (allGates(mask, POST_GAME_GROUP_GATES) || !forwards(wp))
        return;
    s32 gate = group == 1 ? room - 1 : room;
    if (gate < 1 || gate > POST_GAME_GROUP_GATES)
        return;
    s32 next = nextGate(mask, gate, POST_GAME_GROUP_GATES);
    if (next != gate)
        sendTo(wp, group, group == 1 ? next + 1 : next);
}

void onMapChange(spm::seqdrv::SeqWork * wp)
{
    s32 group, room;
    if (wp->p0 == nullptr || !parseRoom(wp->p0, &group, &room))
        return;
    if (story_progress::getChapterValue(6) < CHAPTER_6_POST_GAME)
        storyRoom(wp, group, room);
    else
        postGameRoom(wp, group, room);
}

}
