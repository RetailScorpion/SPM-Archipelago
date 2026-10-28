#include <common.h>
#include <msl/string.h>
#include <spm/mario_pouch.h>
#include <spm/spmario.h>
#include <wii/os.h>

#include "cutscenes.h"
#include "mailbox.h"
#include "storyvalue.h"

namespace mod::cutscenes {

#define BLECK_SCENE_MAP "ls4_11"

#define ITEM_RETURN_PIPE 50

/*
    A new file: the opening maps (aa1_*, aa2_*) at story value 0. aa4_01 is the title screen's
    background and aa3_01 the ending. Vanilla ends the Tippi scene with SET(GSW(0), 2) and a map change
    to mac_02 (Flipside Tower), where Merlon's scenes (2-9) give the first Pure Heart and the Return
    Pipe. Those are skipped too: the file starts in Flipside Tower at the prologue's end, so the hub
    gets its fixed value (Pure Heart 1 in its pillar, the Lineland door open), and Mario gets the
    Return Pipe Merlon would have given (story value 8).
*/
static bool skipOpening(spm::seqdrv::SeqWork * wp)
{
    const char * map = wp->p0;
    if (spm::spmario::gp->gsw0 != 0
        || (msl::string::strncmp(map, "aa1_", 4) != 0 && msl::string::strncmp(map, "aa2_", 4) != 0))
        return false;
    wii::os::OSReport("[spmap] cutscenes: new file, skipping the opening (%s) and Merlon to Flipside Tower\n", map);
    spm::spmario::gp->gsw0 = STORY_PROLOGUE_END;
    spm::mario_pouch::pouchAddItem(ITEM_RETURN_PIPE);
    wp->p0 = "mac_02";
    wp->p1 = nullptr;
    return true;
}

/*
    The Count Bleck scenes: the game goes to ls4_11 right after the prologue (story value 9) or a
    chapter's ending (its "finished" value). Each entry is where that scene sends Mario at its end:
    after Chapters 1 and 2, the end of the Peach / Luigi intermission that follows it instead.
    The chapters' own values aren't touched (the hub discards what these scenes set); the prologue's
    is, since it continues into Chapter 1 (the scene sets 10).
*/
static const struct
{
    s32 storyValue;     // when the game heads to the scene
    const char * map;   // where Mario ends up
    const char * door;  // nullptr = default entrance
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
    s32 & gsw0 = spm::spmario::gp->gsw0;
    for (auto & scene : sBleckScenes)
    {
        if (scene.storyValue != gsw0)
            continue;
        wii::os::OSReport("[spmap] cutscenes: skipping the Count Bleck scene at story value %d, to %s\n",
                          gsw0, scene.map);
        if (scene.newValue >= 0)
            gsw0 = scene.newValue;
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
