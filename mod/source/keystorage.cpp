#include <common.h>
#include <msl/stdio.h>
#include <spm/camdrv.h>
#include <spm/dispdrv.h>
#include <spm/fontmgr.h>
#include <spm/item_data.h>
#include <spm/mario_pouch.h>
#include <spm/msgdrv.h>
#include <spm/windowdrv.h>
#include <wii/gx.h>
#include <wii/mtx.h>
#include <wii/os.h>

#include "keystorage.h"
#include "savedata.h"

namespace mod::keystorage {

// How long the "went to storage" message stays up
#define MESSAGE_FRAMES 300
#define MESSAGE_Y -120.0f   // top of the text (2D coordinates: y up, 0 = centre)
#define MESSAGE_SCALE 0.7f
#define FONT_HEIGHT 20.0f
#define BOX_PAD_X 14.0f
#define BOX_PAD_Y 8.0f

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

// The message on a dark box, so it reads on any map
static void dispMessage(s32 cameraId, void * param)
{
    (void) cameraId;
    (void) param;
    f32 textWidth = spm::fontmgr::FontGetMessageWidth(sMessage) * MESSAGE_SCALE;
    f32 x = -textWidth / 2;

    wii::mtx::Mtx34 mtx;
    wii::mtx::PSMTXIdentity(mtx);
    wii::gx::GXColor box = {0, 0, 0, 0xC0};
    spm::windowdrv::windowDispGX2_Waku_col(mtx, 0, &box, x - BOX_PAD_X, MESSAGE_Y + BOX_PAD_Y,
                                           textWidth + 2 * BOX_PAD_X, FONT_HEIGHT * MESSAGE_SCALE + 2 * BOX_PAD_Y, 8.0f);

    wii::gx::GXColor colour = {255, 255, 255, 255};
    spm::fontmgr::FontDrawStart();
    spm::fontmgr::FontDrawEdge();
    spm::fontmgr::FontDrawColor(&colour);
    spm::fontmgr::FontDrawScale(MESSAGE_SCALE);
    spm::fontmgr::FontDrawNoiseOff();
    spm::fontmgr::FontDrawRainbowColorOff();
    spm::fontmgr::FontDrawString(x, MESSAGE_Y, sMessage);
}

void tick()
{
    if (sMessageFrames <= 0)
        return;
    sMessageFrames--;
    spm::dispdrv::dispEntry(spm::camdrv::CAM_ID_DEBUG_3D, 2, 0.0f, dispMessage, nullptr);
}

}
