#pragma once

#include <common.h>

/*
    Story steps vanilla plays in the hubs (or tied to story values the per-chapter values skip past),
    given directly or replayed on the mod's own conditions. The hubs sit at a fixed story value
    (STORY_HUB_VALUE), so their story scenes never play on their own:
      - Peach joins: her vanilla revival chain is there from the start of the file, on its own story
        value: unconscious in mac_01, Saffron cooks a Fire Burst into Spicy Soup, the soup takes Mario
        to Merlon's house for her join scene, which collects the location
      - Luigi joins: his vanilla scene in the Underwhere (an1_07, vanilla story values 295-296) plays
        until his location is collected (joins.cpp re-gates it)
      - Bowser joins: his vanilla scene after the 3-1 fight (ta1_04), still on Chapter 3's own value
      - the Helmet: on first entering a Chapter 4 map (vanilla: Merlon's Goldfish Bowl quest in the hub,
        story values 131-134; without it Outer Space's movement breaks). Once it's collected (or the
        Helmet is held), Chapter 4 moves to 135, where Outer Space lets Mario swim
      - Sammer's Kingdom is destroyed: Chapter 6 moves from 280 (the duels done) to 281 (vanilla:
        Flipside Tower scene), so the next visit goes to the empty world
      - the Castle Bleck door: once Chapters 1-7 are finished, Flopside Tower (mac_12) gets story value
        357 while Mario is there, which switches its door doa6_l on with Nolrem's door event (story
        values 356-358); going through it plays his last lines and enters Chapter 8

    Peach, Luigi, Bowser, the Helmet and Bestovius's flip lesson (1-1, he1_06) are locations, like the
    Pixl joins: each is collected once, gated on its own save flag (AP_GSWF_GIFT) rather than the story
    value, and gives whatever the seed placed there (mailbox giftItems; its vanilla item without a
    client). The characters' join scenes collect theirs (collectGift, from joins.cpp's ap_char_join),
    the lesson its own (onLesson); the Helmet is given on its trigger. The flip itself follows the
    flip option (flip.h), not the lesson.
*/

namespace mod::hubstory {

// Gift location indices (AP_GSWF_GIFT, mailbox giftItems)
enum Gift
{
    GIFT_PEACH,
    GIFT_LUIGI,
    GIFT_HELMET,
    GIFT_BOWSER,
    GIFT_BESTOVIUS,
};

/*
    Collects a gift location if it isn't yet: sets its flag and gives what was placed there. Returns
    the item given, or 0 if it was already collected.
*/
s32 collectGift(Gift gift);

/*
    The end of Bestovius's flip lesson (joins.cpp ap_lesson): collects its location and moves Chapter
    1 on to value, as the lesson does in vanilla.
*/
void onLesson(s32 value);

/*
    Call on every map change, after storyvalue::onMapChange (it uses the new map's context).
*/
void onMapChange(const char * nextMap);

}
