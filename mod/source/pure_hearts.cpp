#include <common.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "mailbox.h"
#include "pure_hearts.h"
#include "save_data.h"

namespace mod::pure_hearts {

bool isHeartItem(s32 itemId)
{
    return itemId >= PURE_HEART_ITEM_FIRST && itemId < PURE_HEART_ITEM_FIRST + PURE_HEART_COUNT;
}

void giveHeart(s32 heart)
{
    spm::swdrv::swSet(AP_GSWF_HEART(heart));
    wii::os::OSReport("[spmap] doors: Pure Heart %d received\n", heart);
}

bool isDoorOpen(s32 heart)
{
    return mailbox::gMailbox.startingHearts == STARTING_HEARTS_NO_CLIENT || spm::swdrv::swGet(AP_GSWF_HEART(heart));
}

// Whether the seed starts with Pure Heart `heart` (bit heart - 1 of startingHearts).
static bool startsWithHeart(u32 startingHearts, s32 heart)
{
    return startingHearts & (1 << (heart - 1));
}

void onMapChange()
{
    u32 starting = mailbox::gMailbox.startingHearts;
    if (starting == STARTING_HEARTS_NO_CLIENT)
        return;
    for (s32 heart = 1; heart <= PURE_HEART_COUNT; heart++)
    {
        if (startsWithHeart(starting, heart) && !spm::swdrv::swGet(AP_GSWF_HEART(heart)))
        {
            spm::swdrv::swSet(AP_GSWF_HEART(heart));
            wii::os::OSReport("[spmap] doors: starting with Pure Heart %d\n", heart);
        }
    }
}

}
