#include <common.h>
#include <msl/string.h>
#include <spm/mario_pouch.h>
#include <spm/spmario.h>
#include <wii/os.h>

#include "cutscene_skips.h"
#include "mailbox.h"
#include "story_progress.h"

namespace mod::cutscene_skips {

#define BLECK_SCENE_MAP "ls4_11"  // Castle Bleck's throne room
#define ITEM_RETURN_PIPE 50

// Whether a map is one of the opening's (aa1_*, aa2_*). aa3_01 is the ending and aa4_01 the title
// screen's background, so they don't count.
static bool isOpeningMap(const char * map)
{
    return msl::string::strncmp(map, "aa1_", 4) == 0 || msl::string::strncmp(map, "aa2_", 4) == 0;
}

/*
    A new file: an opening map at story value 0. Vanilla ends the Tippi scene by setting the story
    value to 2 and going to Flipside Tower (mac_02), where Merlon's scenes (story values 2-9) give the
    first Pure Heart and the Return Pipe. All of that is skipped: the file starts in Flipside Tower
    at the prologue's end, and Mario gets the Return Pipe Merlon would have given. Which doors are
    open comes from the seed's Pure Hearts (pure_hearts).
*/
static bool skipOpening(spm::seqdrv::SeqWork * wp)
{
    if (spm::spmario::gp->gsw0 != 0 || !isOpeningMap(wp->p0))
        return false;
    wii::os::OSReport("[spmap] cutscenes: new file, skipping the opening (%s) and Merlon to Flipside Tower\n", wp->p0);
    spm::spmario::gp->gsw0 = STORY_PROLOGUE_END;
    spm::mario_pouch::pouchAddItem(ITEM_RETURN_PIPE);
    wp->p0 = "mac_02";
    wp->p1 = nullptr;
    return true;
}

/*
    The Count Bleck scenes: the game heads to ls4_11 right after the prologue (story value 9) or a
    chapter's ending (its "finished" value). Each entry says where that scene sends Mario at its end.
    After Chapters 1 and 2, that's the end of the Peach / Luigi scene that follows it.
    The chapters' own values aren't changed (the hub ignores what these scenes set); the prologue's
    is, since it carries on into Chapter 1 (the scene sets 10).
*/
static const struct
{
    s32 storyValue;     // the story value when the game heads to the scene
    const char * map;   // where Mario ends up
    const char * door;  // which door he arrives by (nullptr = the map's default spot)
    s32 newValue;       // story value to set, or -1 to leave it
} sBleckScenes[] = {
    {9, "he1_01", "doa1_l", 10},     // prologue -> 1-1
    {53, "mac_02", "doa6_l", -1},    // Chapter 1 -> Bleck (54) -> Peach (55-57)
    {95, "mac_02", "doa8_l", -1},    // Chapter 2 -> Bleck (96) -> Luigi (97-98)
    {125, "mac_02", "doa9_l", -1},   // Chapter 3 -> Bleck (126)
    {173, "mac_02", "doa10_l", -1},  // Chapter 4 -> Bleck (174)
    {220, "mac_02", nullptr, -1},    // Chapter 5 -> Bleck (221)
    {286, "mac_02", "doa12_l", -1},  // Chapter 6 -> Bleck (287)
    {353, "mac_02", nullptr, -1},    // Chapter 7 -> Bleck (354)
};

static bool skipBleckScene(spm::seqdrv::SeqWork * wp)
{
    if (msl::string::strcmp(wp->p0, BLECK_SCENE_MAP) != 0)
        return false;
    s32 & storyValue = spm::spmario::gp->gsw0;
    for (auto & scene : sBleckScenes)
    {
        if (scene.storyValue != storyValue)
            continue;
        wii::os::OSReport("[spmap] cutscenes: skipping the Count Bleck scene at story value %d, to %s\n",
                          storyValue, scene.map);
        if (scene.newValue >= 0)
            storyValue = scene.newValue;
        wp->p0 = scene.map;
        wp->p1 = scene.door;
        return true;
    }
    return false;
}

void onMapChange(spm::seqdrv::SeqWork * wp)
{
    if (wp->p0 == nullptr || mailbox::gMailbox.playCutscenes)
        return;
    if (!skipOpening(wp))
        skipBleckScene(wp);
}

}
