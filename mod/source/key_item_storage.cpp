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

#include "key_item_storage.h"
#include "save_data.h"

namespace mod::key_item_storage {

// The notice: how long it stays up (60 frames = 1 second) and how it looks. Screen coordinates
// have 0 at the centre and y going up.
#define NOTICE_FRAMES 300
#define NOTICE_Y -120.0f      // top of the text
#define NOTICE_SCALE 0.7f     // text size
#define FONT_HEIGHT 20.0f     // the font's height at size 1
#define BOX_PAD_X 14.0f       // space between the text and the box's sides
#define BOX_PAD_Y 8.0f        // space between the text and the box's top and bottom
#define BOX_CORNER 8.0f       // how rounded the box's corners are

static char sNotice[96];
static s32 sNoticeFrames;

bool keyItemsFull()
{
    for (u16 item : spm::mario_pouch::pouchGetPtr()->keyItem)
    {
        if (item == 0)
            return false;
    }
    return true;
}

// The save byte where storage slot `slot` is kept (2 bytes per slot).
static s32 slotSaveByte(s32 slot)
{
    return AP_GSW_KEY_STORAGE + 2 * slot;
}

// The key item in storage slot `slot`, or 0 if it's empty.
static u16 storedAt(s32 slot)
{
    return save_data::getGswU16(slotSaveByte(slot));
}

// Starts showing "Key items full: X went to storage".
static void showNotice(s32 itemId)
{
    const char * name = spm::msgdrv::msgSearch(spm::item_data::itemDataTable[itemId].nameMsg);
    msl::stdio::sprintf(sNotice, "Key items full: %s went to storage", name);
    sNoticeFrames = NOTICE_FRAMES;
}

bool store(s32 itemId)
{
    for (s32 slot = 0; slot < KEY_STORAGE_SLOTS; slot++)
    {
        if (storedAt(slot) != 0)
            continue;
        save_data::setGswU16(slotSaveByte(slot), static_cast<u16>(itemId));
        showNotice(itemId);
        wii::os::OSReport("[spmap] keystorage: key items full, item %d stored in slot %d\n", itemId, slot);
        return true;
    }
    wii::os::OSReport("[spmap] keystorage: key items and storage full, item %d lost\n", itemId);
    return false;
}

// Draws the notice: red text on a dark see-through box, so it's readable on any map. The game calls
// this while drawing the screen (registered each frame by tick).
static void drawNotice(s32 cameraId, void * param)
{
    (void) cameraId;
    (void) param;
    f32 textWidth = spm::fontmgr::FontGetMessageWidth(sNotice) * NOTICE_SCALE;
    f32 x = -textWidth / 2;  // centred

    wii::mtx::Mtx34 noTransform;
    wii::mtx::PSMTXIdentity(noTransform);
    wii::gx::GXColor boxColour = {0, 0, 0, 0xC0};  // black, mostly opaque
    spm::windowdrv::windowDispGX2_Waku_col(noTransform, 0, &boxColour, x - BOX_PAD_X, NOTICE_Y + BOX_PAD_Y,
                                           textWidth + 2 * BOX_PAD_X, FONT_HEIGHT * NOTICE_SCALE + 2 * BOX_PAD_Y, BOX_CORNER);

    wii::gx::GXColor textColour = {255, 80, 80, 255};  // red
    spm::fontmgr::FontDrawStart();
    spm::fontmgr::FontDrawEdge();
    spm::fontmgr::FontDrawColor(&textColour);
    spm::fontmgr::FontDrawScale(NOTICE_SCALE);
    spm::fontmgr::FontDrawNoiseOff();
    spm::fontmgr::FontDrawRainbowColorOff();
    spm::fontmgr::FontDrawString(x, NOTICE_Y, sNotice);
}

void tick()
{
    if (sNoticeFrames <= 0)
        return;
    sNoticeFrames--;
    spm::dispdrv::dispEntry(spm::camdrv::CAM_ID_DEBUG_3D, 2, 0.0f, drawNotice, nullptr);
}

}
