#include <common.h>
#include <evt_cmd.h>
#include <spm/mario_pouch.h>
#include <wii/os.h>

#include "game_mode.h"
#include "key_groups.h"
#include "script_patching.h"

namespace mod::key_groups {

using namespace mod::script_patching;

// Each grouped key and the key it becomes
static const struct
{
    s16 key;
    s16 canonical;
} sKeys[] = {
    {19, 20}, {21, 20},            // 3-4 Fort Keys
    {28, 27}, {29, 27},            // 4-4 Dimension Keys
    {34, 33},                      // 5-3/5-4 Cave Keys
    {37, 40}, {38, 40}, {49, 40},  // 7-1 to 7-4 Underwhere Door Keys
};

s32 canonical(s32 itemId)
{
    for (auto & entry : sKeys)
    {
        if (entry.key == itemId)
            return entry.canonical;
    }
    return itemId;
}

/*
    The words naming a lock's key (relF script section offsets; relF.rel file offset - 0xAF300): its
    evt_mobj_zyo item, its item menu list's entry, its open script's evt_pouch_check_have_item, and its
    IF_NOT_EQUAL test of the chosen item (3-4's lock: the evt_pouch_remove_item in its after-open
    script instead). Found by the 2026-11-12 scan of every lock's scripts.
*/
static const struct
{
    u32 offsets[4];
    s32 key;
    const char * what;
} sLocks[] = {
    {{0x88860, 0x88140, 0x88150, 0x881ac}, 19, "3-4 lock (ta4_01)"},
    {{0xa786c, 0xa7618, 0xa7628, 0xa765c}, 28, "4-4 lock (sp4_07)"},
    {{0xa8378, 0xa81a8, 0xa81b8, 0xa81ec}, 29, "4-4 lock (sp4_11)"},
    {{0xc5dd4, 0xc5208, 0xc5218, 0xc524c}, 34, "5-4 lock (gn4_02)"},
    {{0xe7904, 0xe7470, 0xe7480, 0xe74b4}, 49, "7-1 lock (an1_03)"},
    {{0xf2d54, 0xf1878, 0xf1888, 0xf18bc}, 37, "7-2 lock (an2_03)"},
    {{0xf6ef4, 0xf6a28, 0xf6a38, 0xf6a6c}, 38, "7-2 lock (an2_06)"},
};

void init()
{
    for (auto & lock : sLocks)
    {
        bool all = true;
        for (u32 offset : lock.offsets)
            all = all && matches(relfScript(offset), &lock.key, 1, lock.what);
        if (!all)
            continue;
        for (u32 offset : lock.offsets)
            game_mode::set<s32>(relfScript(offset), canonical(lock.key));
    }
    wii::os::OSReport("[spmap] keys: grouped locks take their chapter's key\n");
}

void onMapChange()
{
    auto * pouch = spm::mario_pouch::pouchGetPtr();
    for (auto & slot : pouch->keyItem)
    {
        s32 key = canonical(slot);
        if (key != slot)
        {
            wii::os::OSReport("[spmap] keys: held key %d is now %d\n", slot, key);
            slot = static_cast<u16>(key);
        }
    }
}

}
