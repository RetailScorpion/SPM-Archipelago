#include <common.h>
#include <msl/string.h>
#include <spm/spmario.h>
#include <wii/os.h>

#include "save_data.h"
#include "story_progress.h"

namespace mod::story_progress {

// The value each chapter starts at: what the game sets when you go through that chapter's door
static const s32 sChapterStart[STORY_CHAPTER_COUNT + 1] = {
    STORY_HUB_VALUE, 11, 66, 101, 129, 179, 225, 303, 359,
};

// The value after each chapter's ending cutscene
static const s32 sChapterFinished[STORY_CHAPTER_COUNT + 1] = {
    0, 53, 95, 125, 173, 220, 286, 353, 419,
};

// The value after each section's ending (X-1, X-2, X-3): reaching it means the next section is
// reached. 0 = the chapter doesn't have regular sections.
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

// Which chapter a map belongs to, by the start of its name. (Chapter 7's "an" maps also host the
// Underwhere scenes after Chapter 6; those aren't handled separately yet.)
static const struct
{
    const char * prefix;
    s32 context;
} sMapPrefixes[] = {
    {"mac_", STORY_CONTEXT_HUB},
    {"he", 1}, {"mi", 2}, {"ta", 3}, {"sp", 4}, {"gn", 5}, {"wa", 6}, {"an", 7}, {"ls", 8},
};

StoryState gStory;

// The save byte where a chapter's slot is stored.
static s32 slotSaveByte(s32 chapter)
{
    return AP_GSW_STORY_SLOTS + (chapter - 1) * 2;
}

// A chapter's saved value, or its starting value if it has none yet.
static s32 loadSlot(s32 chapter)
{
    s32 value = save_data::getGswU16(slotSaveByte(chapter));
    return value != 0 ? value : sChapterStart[chapter];
}

// Saves a chapter's value.
static void storeSlot(s32 chapter, s32 value)
{
    save_data::setGswU16(slotSaveByte(chapter), static_cast<u16>(value));
}

// Refreshes the copy of every slot the developer tools display.
static void refreshToolCopy()
{
    gStory.slots[0] = STORY_HUB_VALUE;
    for (s32 chapter = 1; chapter <= STORY_CHAPTER_COUNT; chapter++)
        gStory.slots[chapter] = loadSlot(chapter);
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
    for (auto & entry : sMapPrefixes)
    {
        if (msl::string::strncmp(map, entry.prefix, msl::string::strlen(entry.prefix)) == 0)
            return entry.context;
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
    for (s32 sectionEnd : sSectionEnd[chapter])
    {
        if (sectionEnd != 0 && value >= sectionEnd)
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
    refreshToolCopy();
    wii::os::OSReport("[spmap] story: chapter %d advanced to %d\n", chapter, value);
}

// Which chapter the map being entered counts as. Two exceptions to the plain map-name rule.
static s32 nextContext(const char * nextMap, s32 storyValue)
{
    s32 next = contextForMap(nextMap);

    // A new file's prologue: its Flipside scenes need the real early values, so per-chapter values
    // only start once the prologue reaches the Lineland door.
    if (gStory.context <= STORY_CONTEXT_HUB && storyValue < STORY_PROLOGUE_END)
        return STORY_CONTEXT_NONE;

    // Castle Bleck between chapters: the Count Bleck scene after the prologue or a chapter, and
    // the Peach / Luigi intermissions, use Chapter 8's maps (ls*) at the story value from before
    // Chapter 8. They keep the value they arrive with, rather than getting Chapter 8's. Chapter 8
    // itself is entered from the hub (Flopside), or loaded from a save at 359 or more.
    if (next == STORY_CHAPTER_COUNT && gStory.context != STORY_CONTEXT_HUB
        && gStory.context != STORY_CHAPTER_COUNT && storyValue < sChapterStart[STORY_CHAPTER_COUNT])
        return STORY_CONTEXT_NONE;

    return next;
}

void onMapChange(const char * nextMap)
{
    s32 & storyValue = spm::spmario::gp->gsw0;
    s32 next = nextContext(nextMap, storyValue);

    // Save the chapter being left (sync kept it up to date; this catches the very last change).
    // Never backwards: a scene played late still sets its old value.
    if (gStory.context > STORY_CONTEXT_HUB && storyValue > loadSlot(gStory.context))
        storeSlot(gStory.context, storyValue);
    else if (gStory.context == STORY_CONTEXT_HUB && storyValue != STORY_HUB_VALUE)
        wii::os::OSReport("[spmap] story: hub wrote %d, discarded\n", storyValue);

    // Load the chapter being entered
    s32 old = storyValue;
    if (next == STORY_CONTEXT_HUB)
        storyValue = STORY_HUB_VALUE;
    else if (next != STORY_CONTEXT_NONE)
        storyValue = loadSlot(next);

    wii::os::OSReport("[spmap] story: map %s, context %d -> %d, value %d -> %d\n",
                      nextMap, gStory.context, next, old, storyValue);
    gStory.context = next;
    refreshToolCopy();
}

void sync()
{
    if (gStory.context <= STORY_CONTEXT_HUB)
        return;
    // Only forwards; a lower value set by a late scene is undone at the next map change
    s32 value = spm::spmario::gp->gsw0;
    if (value > gStory.slots[gStory.context])
    {
        storeSlot(gStory.context, value);
        gStory.slots[gStory.context] = value;
    }
}

}
