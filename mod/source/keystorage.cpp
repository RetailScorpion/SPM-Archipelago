#include <common.h>
#include <msl/stdio.h>
#include <spm/fontmgr.h>
#include <spm/item_data.h>
#include <spm/mario_pouch.h>
#include <spm/msgdrv.h>
#include <wii/gx.h>
#include <wii/os.h>

#include "keystorage.h"
#include "savedata.h"

namespace mod::keystorage {

// How long the "went to storage" message stays up
#define MESSAGE_FRAMES 300
#define MESSAGE_Y -120.0f

static char sMessage[96];
static s32 sMessageFrames;

bool keyItemsFull()
{
    for (u16 item : spm::mario_pouch::pouchGetPtr()->keyItem)
    {
        if (item == 0)
            return false;
    }
    return true;
}

static u16 storedAt(s32 slot)
{
    s32 id = AP_GSW_KEY_STORAGE + 2 * slot;
    return static_cast<u16>(savedata::getGswByte(id) << 8 | savedata::getGswByte(id + 1));
}

bool store(s32 itemId)
{
    for (s32 slot = 0; slot < KEY_STORAGE_SLOTS; slot++)
    {
        if (storedAt(slot) != 0)
            continue;
        s32 id = AP_GSW_KEY_STORAGE + 2 * slot;
        savedata::setGswByte(id, static_cast<u8>(itemId >> 8));
        savedata::setGswByte(id + 1, static_cast<u8>(itemId));
        const char * name = spm::msgdrv::msgSearch(spm::item_data::itemDataTable[itemId].nameMsg);
        msl::stdio::sprintf(sMessage, "Key items full: %s went to storage", name);
        sMessageFrames = MESSAGE_FRAMES;
        wii::os::OSReport("[spmap] keystorage: key items full, item %d stored in slot %d\n", itemId, slot);
        return true;
    }
    wii::os::OSReport("[spmap] keystorage: key items and storage full, item %d lost\n", itemId);
    return false;
}

void tick()
{
    if (sMessageFrames <= 0)
        return;
    sMessageFrames--;
    f32 scale = 0.7f;
    wii::gx::GXColor colour = {255, 255, 255, 255};
    spm::fontmgr::FontDrawStart();
    spm::fontmgr::FontDrawEdge();
    spm::fontmgr::FontDrawColor(&colour);
    spm::fontmgr::FontDrawScale(scale);
    spm::fontmgr::FontDrawNoiseOff();
    spm::fontmgr::FontDrawRainbowColorOff();
    f32 x = -((spm::fontmgr::FontGetMessageWidth(sMessage) * scale) / 2);
    spm::fontmgr::FontDrawString(x, MESSAGE_Y, sMessage);
}

}
