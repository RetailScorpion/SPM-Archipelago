#include <common.h>
#include <msl/string.h>
#include <spm/mario_pouch.h>
#include <spm/spmario.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "apitem.h"
#include "delivery.h"
#include "hubstory.h"
#include "mailbox.h"
#include "savedata.h"
#include "storyvalue.h"

namespace mod::hubstory {

#define CHAPTER_6_DUELS_DONE 280   // Sammer's Kingdom, end of the story's duels
#define CHAPTER_6_WORLD_ENDED 281  // set by the Flipside Tower scene after them

// Chapter 4 starts at 129 (vanilla: just teleported back to Flipside, without the Helmet). With the
// Helmet it moves to 135, past vanilla's Helmet intermission and its "put the Helmet on" scene (134,
// which would play over the Helmet's item-get): the Outer Space maps turn on swimming from 135
#define HELMET_ITEM 24
#define CHAPTER_4_HELMET 135

#define CASTLE_BLECK_DOOR_MAP "mac_12"  // Flopside Tower
#define CASTLE_BLECK_DOOR_VALUE 357     // its door doa6_l leads to Chapter 8 for 356-358

/*
    Story gift locations. Each is collected once, when its trigger first holds: its own save flag
    (AP_GSWF_GIFT) is the "collected" state, like a Pixl join's, so nothing depends on story values.
    It gives what the client placed there (mailbox giftItems), or its vanilla item without a client.
*/
enum Trigger
{
    SCENE,              // collected by its join scene (joins.cpp ap_char_join), never here
    ENTERED_CHAPTER_4,
};

static const struct
{
    s32 vanillaItem;
    Trigger trigger;
    const char * what;
} sGifts[] = {
    {217, SCENE, "Peach joins"},               // Merlon's house, after her revival chain (vanilla 60-62)
    {219, SCENE, "Luigi joins"},               // the Underwhere, an1_07 (vanilla 295-296)
    {24, ENTERED_CHAPTER_4, "the Helmet"},     // vanilla: Merlon's Goldfish Bowl quest, 131-134
    {218, SCENE, "Bowser joins"},              // 3-1, after his fight (Chapter 3's own value, < 104)
    {80, SCENE, "Bestovius's lesson"},         // 1-1, he1_06 (vanilla 15-16); a Shroom Shake stands in
                                               // for the flip, which isn't an item
};

#define CHARACTER_ITEM_FIRST 217
#define CHARACTER_ITEM_LAST 219

// Peach's join scene: mac_02's init runs it for CASE_SMALL(63) of its story value SWITCH, which
// joins.cpp points at AP_GSW_PEACH_SCENE. Past the hub value when the scene is due, 0 (never) otherwise
#define PEACH_SCENE_ON (STORY_HUB_VALUE + 1)

// Bestovius's talk script (joins.cpp LESSON_IF) compares with AP_GSW_LESSON instead of Chapter 1's
// value: 0 offers the lesson, LESSON_DONE (past the lesson, below his later chats) only chats
#define LESSON_DONE 17

void onLesson(s32 value)
{
    collectGift(GIFT_BESTOVIUS);
    savedata::setGswByte(AP_GSW_LESSON, LESSON_DONE);
    storyvalue::advanceChapter(1, value);
}

s32 collectGift(Gift gift)
{
    s32 flag = AP_GSWF_GIFT(gift);
    if (spm::swdrv::swGet(flag))
        return 0;
    // The flag first: it marks the location collected (the client reports it) and, for another
    // world's item, tells the AP item-get whose names to show
    spm::swdrv::swSet(flag);
    u16 placed = mailbox::gMailbox.giftItems[gift];
    s32 item = placed == 0 ? sGifts[gift].vanillaItem : placed == JOIN_ITEM_OTHER_WORLD ? apitem::AP_ITEM_ID : placed;
    // A scene's own character joins quietly, as in vanilla; anything else gets the item-get
    if (item == sGifts[gift].vanillaItem && item >= CHARACTER_ITEM_FIRST && item <= CHARACTER_ITEM_LAST)
        spm::mario_pouch::pouchAddItem(item);
    else
        delivery::giveLocal(item);
    wii::os::OSReport("[spmap] hubstory: %s location collected, giving item %d\n", sGifts[gift].what, item);
    return item;
}

static void collectGifts(s32 context)
{
    for (u32 i = 0; i < sizeof(sGifts) / sizeof(sGifts[0]); i++)
    {
        if (sGifts[i].trigger == ENTERED_CHAPTER_4 && context == 4)
            collectGift(static_cast<Gift>(i));
    }
}

static bool chaptersFinished(s32 last)
{
    for (s32 chapter = 1; chapter <= last; chapter++)
    {
        if (!storyvalue::isChapterFinished(chapter))
            return false;
    }
    return true;
}

void onMapChange(const char * nextMap)
{
    s32 context = storyvalue::gStory.context;

    collectGifts(context);

    // The chain starts over from the beginning on a save from before it had its own value
    s32 chain = savedata::getGswByte(AP_GSW_PEACH_CHAIN);
    if (spm::swdrv::swGet(AP_GSWF_GIFT(GIFT_PEACH)))
        chain = PEACH_CHAIN_DONE;
    else if (chain < PEACH_CHAIN_START || chain >= PEACH_CHAIN_DONE)
        chain = PEACH_CHAIN_START;
    savedata::setGswByte(AP_GSW_PEACH_CHAIN, chain);
    // Due once she can be given the soup, as in vanilla (61-62): a Spicy Soup from anywhere works
    savedata::setGswByte(AP_GSW_PEACH_SCENE, chain > PEACH_CHAIN_START && chain < PEACH_CHAIN_DONE ? PEACH_SCENE_ON : 0);
    savedata::setGswByte(AP_GSW_LESSON, spm::swdrv::swGet(AP_GSWF_GIFT(GIFT_BESTOVIUS)) ? LESSON_DONE : 0);

    if (spm::swdrv::swGet(AP_GSWF_GIFT(GIFT_HELMET)) || spm::mario_pouch::pouchCheckHaveItem(HELMET_ITEM))
        storyvalue::advanceChapter(4, CHAPTER_4_HELMET);

    if (storyvalue::getChapterValue(6) == CHAPTER_6_DUELS_DONE)
        storyvalue::advanceChapter(6, CHAPTER_6_WORLD_ENDED);

    if (context == STORY_CONTEXT_HUB && nextMap != nullptr
        && msl::string::strcmp(nextMap, CASTLE_BLECK_DOOR_MAP) == 0 && chaptersFinished(7))
    {
        spm::spmario::gp->gsw0 = CASTLE_BLECK_DOOR_VALUE;
        wii::os::OSReport("[spmap] hubstory: Chapters 1-7 finished, Castle Bleck door open in %s\n", nextMap);
    }
}

}
