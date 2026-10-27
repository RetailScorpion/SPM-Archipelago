#include <common.h>
#include <spm/spmario.h>
#include <wii/os.h>
#include <msl/string.h>

#include "savedata.h"
#include "storyvalue.h"

namespace mod::storyvalue {

// Value each chapter starts at: what the game sets when you go through that chapter's door
static const s32 sChapterStart[STORY_CHAPTER_COUNT + 1] = {
    STORY_HUB_VALUE, 11, 66, 101, 129, 179, 225, 303, 359,
};

// Value after each chapter's ending cutscene
static const s32 sChapterFinished[STORY_CHAPTER_COUNT + 1] = {
    0, 53, 95, 125, 173, 220, 286, 353, 419,
};

// Value after each section's ending (X-1, X-2, X-3): reaching it means the next section is reached.
// From the sequence list; 0 = chapter doesn't have regular sections.
static const s32 sSectionEnd[STORY_CHAPTER_COUNT + 1][3] = {
    {0, 0, 0},
    {18, 29, 38},
    {77, 83, 89},
    {106, 112, 118},
    {139, 150, 161},
    {187, 195, 203},
    {0, 0, 0},  // Sammer's Kingdom, then the World of Nothing
    {307, 328, 340},
    {0, 0, 0},  // Castle Bleck: entered from Flopside
};

// Map name prefix -> context. Chapter 7 (an) also hosts the Underwhere intermission at the end of
// Chapter 6; that's not handled yet.
static const struct { const char * prefix; s32 context; } sPrefixes[] = {
    {"mac_", STORY_CONTEXT_HUB},
    {"he", 1}, {"mi", 2}, {"ta", 3}, {"sp", 4}, {"gn", 5}, {"wa", 6}, {"an", 7}, {"ls", 8},
};

StoryState gStory;

static s32 loadSlot(s32 chapter)
{
    s32 id = AP_GSW_STORY_SLOTS + (chapter - 1) * 2;
    s32 value = savedata::getGswByte(id) << 8 | savedata::getGswByte(id + 1);
    return value != 0 ? value : sChapterStart[chapter];
}

static void storeSlot(s32 chapter, s32 value)
{
    s32 id = AP_GSW_STORY_SLOTS + (chapter - 1) * 2;
    savedata::setGswByte(id, static_cast<u8>(value >> 8));
    savedata::setGswByte(id + 1, static_cast<u8>(value));
}

static void refreshDebugCopy()
{
    gStory.slots[0] = STORY_HUB_VALUE;
    for (s32 ch = 1; ch <= STORY_CHAPTER_COUNT; ch++)
        gStory.slots[ch] = loadSlot(ch);
}

void init()
{
    gStory.context = STORY_CONTEXT_NONE;
}

void resetContext()
{
    gStory.context = STORY_CONTEXT_NONE;
}

s32 contextForMap(const char * map)
{
    for (auto & p : sPrefixes)
    {
        if (msl::string::strncmp(map, p.prefix, msl::string::strlen(p.prefix)) == 0)
            return p.context;
    }
    return STORY_CONTEXT_NONE;
}

s32 getChapterValue(s32 chapter)
{
    return loadSlot(chapter);
}

bool isChapterFinished(s32 chapter)
{
    return loadSlot(chapter) >= sChapterFinished[chapter];
}

s32 sectionsReached(s32 chapter)
{
    if (isChapterFinished(chapter))
        return 4;
    s32 value = loadSlot(chapter);
    s32 reached = 1;
    for (s32 end : sSectionEnd[chapter])
    {
        if (end != 0 && value >= end)
            reached++;
    }
    return reached;
}

void advanceChapter(s32 chapter, s32 value)
{
    if (value <= loadSlot(chapter))
        return;
    storeSlot(chapter, value);
    if (gStory.context == chapter)
        spm::spmario::gp->gsw0 = value;
    refreshDebugCopy();
    wii::os::OSReport("[spmap] story: chapter %d advanced to %d\n", chapter, value);
}

void onMapChange(const char * nextMap)
{
    s32 & gsw0 = spm::spmario::gp->gsw0;
    s32 next = contextForMap(nextMap);

    // Leave a new file's prologue alone: its Flipside scenes need the real early values. Per-chapter
    // values start once the prologue reaches the Lineland door. (The randomizer will skip the
    // prologue at file start; until then this keeps new files playable.)
    if (gStory.context <= STORY_CONTEXT_HUB && gsw0 < STORY_PROLOGUE_END)
        next = STORY_CONTEXT_NONE;

    // Castle Bleck between chapters: the Count Bleck scene after the prologue or a chapter, and the
    // Peach / Luigi intermissions, use Chapter 8's maps (ls*) at the story's value from before
    // Chapter 8. Those keep the value they arrive with (no context), rather than Chapter 8's. Chapter
    // 8 itself is entered from the hub (Flopside), or loaded from a save at 359 or more.
    if (next == STORY_CHAPTER_COUNT && gStory.context != STORY_CONTEXT_HUB
        && gStory.context != STORY_CHAPTER_COUNT && gsw0 < sChapterStart[STORY_CHAPTER_COUNT])
        next = STORY_CONTEXT_NONE;

    // The chapter being left was kept in sync every frame; a final store catches the last change.
    // Never backwards: a scene played late (a Pixl join, say) still sets its original value.
    if (gStory.context > STORY_CONTEXT_HUB && gsw0 > loadSlot(gStory.context))
        storeSlot(gStory.context, gsw0);
    else if (gStory.context == STORY_CONTEXT_HUB && gsw0 != STORY_HUB_VALUE)
        wii::os::OSReport("[spmap] story: hub wrote %d, discarded\n", gsw0);

    s32 old = gsw0;
    if (next == STORY_CONTEXT_HUB)
        gsw0 = STORY_HUB_VALUE;
    else if (next != STORY_CONTEXT_NONE)
        gsw0 = loadSlot(next);

    wii::os::OSReport("[spmap] story: map %s, context %d -> %d, value %d -> %d\n",
                      nextMap, gStory.context, next, old, gsw0);
    gStory.context = next;
    refreshDebugCopy();
}

void sync()
{
    if (gStory.context > STORY_CONTEXT_HUB)
    {
        // Only forwards; a lower value set by a late scene is undone at the next map change
        s32 value = spm::spmario::gp->gsw0;
        if (value > gStory.slots[gStory.context])
        {
            storeSlot(gStory.context, value);
            gStory.slots[gStory.context] = value;
        }
    }
}

}
