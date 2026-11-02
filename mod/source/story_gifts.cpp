#include <common.h>
#include <spm/mario_pouch.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "archipelago_item.h"
#include "mailbox.h"
#include "received_items.h"
#include "save_data.h"
#include "story_gifts.h"
#include "story_progress.h"

namespace mod::story_gifts {

// What triggers a gift here (the others are collected by their scenes, in other files)
enum Trigger
{
    BY_ITS_SCENE,       // collected by its own scene (character_joins, npc_story_fixes), never here
    ENTERED_CHAPTER_4,  // collected on first entering a Chapter 4 map
    FINISHED_CHAPTER_6, // collected on the first map change once Chapter 6 is finished
};

static const struct
{
    s32 vanillaItem;
    Trigger trigger;
    const char * what;
} sGifts[] = {
    {217, BY_ITS_SCENE, "Peach joins"},          // Flipside: giving her the Spicy Soup, or her join scene
    {219, BY_ITS_SCENE, "Luigi joins"},          // the Underwhere, an1_07
    {24, ENTERED_CHAPTER_4, "the Helmet"},       // vanilla: Merlon's Goldfish Bowl quest
    {218, BY_ITS_SCENE, "Bowser joins"},         // 3-1, after his fight
    {80, BY_ITS_SCENE, "Bestovius's lesson"},    // 1-1, he1_06; a Shroom Shake stands in for the
                                                 // flip, which isn't an item
    {82, FINISHED_CHAPTER_6, "beating Brobot L-type"},  // wa1_27; an Ultra Shroom Shake stands
                                                        // in, since vanilla gives nothing
};

// Peach, Bowser and Luigi as items
#define CHARACTER_ITEM_FIRST 217
#define CHARACTER_ITEM_LAST 219

#define HELMET_ITEM 24

// Chapter 4 starts at 129 (vanilla: just back in Flipside, without the Helmet). With the Helmet it
// moves to 135: past vanilla's Helmet scenes (134 would play "put the Helmet on" over the item-get),
// and the value from which the Outer Space maps let Mario swim
#define CHAPTER_4_WITH_HELMET 135

// The item a gift location gives: what the client placed there, or the vanilla item.
static s32 placedItem(Gift gift)
{
    u16 placed = mailbox::gMailbox.giftItems[gift];
    if (placed == 0)
        return sGifts[gift].vanillaItem;
    if (placed == JOIN_ITEM_OTHER_WORLD)
        return archipelago_item::AP_ITEM_ID;
    return placed;
}

// Whether an item is the gift's own character (Peach, Bowser or Luigi at their own scene).
static bool isOwnCharacter(Gift gift, s32 item)
{
    return item == sGifts[gift].vanillaItem && item >= CHARACTER_ITEM_FIRST && item <= CHARACTER_ITEM_LAST;
}

s32 collectGift(Gift gift)
{
    s32 flag = AP_GSWF_GIFT(gift);
    if (spm::swdrv::swGet(flag))
        return 0;
    // The flag first: it marks the location collected (the client sends it to the server) and, for
    // another world's item, tells the Archipelago item-get which names to show
    spm::swdrv::swSet(flag);
    s32 item = placedItem(gift);
    // A scene's own character joins quietly, as in vanilla; anything else gets an item-get
    if (isOwnCharacter(gift, item))
        spm::mario_pouch::pouchAddItem(item);
    else
        received_items::giveLocal(item);
    wii::os::OSReport("[spmap] hubstory: %s location collected, giving item %d\n", sGifts[gift].what, item);
    return item;
}

void onMapChange()
{
    bool chapter4 = story_progress::gStory.context == 4;
    bool chapter6Finished = story_progress::isChapterFinished(6);
    for (u32 i = 0; i < sizeof(sGifts) / sizeof(sGifts[0]); i++)
    {
        if ((sGifts[i].trigger == ENTERED_CHAPTER_4 && chapter4) ||
            (sGifts[i].trigger == FINISHED_CHAPTER_6 && chapter6Finished))
            collectGift(static_cast<Gift>(i));
    }

    if (spm::swdrv::swGet(AP_GSWF_GIFT(GIFT_HELMET)) || spm::mario_pouch::pouchCheckHaveItem(HELMET_ITEM))
        story_progress::advanceChapter(4, CHAPTER_4_WITH_HELMET);
}

}
