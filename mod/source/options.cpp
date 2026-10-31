#include <common.h>
#include <msl/string.h>
#include <spm/fontmgr.h>
#include <spm/pausewin.h>
#include <spm/spmario_snd.h>
#include <spm/windowdrv.h>
#include <spm/wpadmgr.h>
#include <wii/gx.h>
#include <wii/mtx.h>
#include <wii/os.h>
#include <wii/wpad.h>

#include "gamesyms.h"
#include "options.h"
#include "patch.h"
#include "savedata.h"

namespace mod::options {

struct OptionInfo
{
    const char * name;
    bool defaultOn;
};

static const OptionInfo sOptions[OPTION_COUNT] = {
    {"Music shuffle", false},
    {"Hold 1 to skip text", true},
};

#define INSTR_LI_R3_1 0x38600001

// The window, to the right of the buttons (pause menu coordinates: y up, 0 = screen centre)
#define WIN_X -110.0f
#define WIN_Y 185.0f
#define WIN_WIDTH 390.0f
#define WIN_HEIGHT 245.0f
#define PAD 18.0f
#define TITLE_SCALE 0.9f
#define ROW_SCALE 0.75f
#define ROWS_TOP 62.0f
#define ROW_HEIGHT 34.0f
#define VALUE_WIDTH 60.0f

static s32 sCursor;

bool get(Option option)
{
    bool changed = (savedata::getGswByte(AP_GSW_OPTIONS) >> option) & 1;
    return changed != sOptions[option].defaultOn;
}

static void toggle(Option option)
{
    savedata::setGswByte(AP_GSW_OPTIONS, savedata::getGswByte(AP_GSW_OPTIONS) ^ (1 << option));
    wii::os::OSReport("[spmap] options: %s %s\n", sOptions[option].name, get(option) ? "on" : "off");
}

const char * message(const char * msgName)
{
    // The help line shown while the cursor is on button 7
    if (msl::string::strcmp(msgName, "menu_help_007") == 0)
        return "Change the Archipelago settings for this file.";
    return nullptr;
}

static void drawText(f32 x, f32 y, f32 scale, const wii::gx::GXColor & colour, const char * text)
{
    wii::gx::GXColor c = colour;
    spm::fontmgr::FontDrawStart();
    spm::fontmgr::FontDrawEdge();
    spm::fontmgr::FontDrawColor(&c);
    spm::fontmgr::FontDrawScale(scale);
    spm::fontmgr::FontDrawNoiseOff();
    spm::fontmgr::FontDrawRainbowColorOff();
    spm::fontmgr::FontDrawString(x, y, text);
}

static void windowDisp(spm::pausewin::PausewinEntry * entry)
{
    f32 x = entry->pos.x;
    f32 y = entry->pos.y;

    wii::mtx::Mtx34 mtx;
    wii::mtx::PSMTXIdentity(mtx);
    wii::gx::GXColor box = {0, 0, 0, 0xC0};
    spm::windowdrv::windowDispGX2_Waku_col(mtx, 0, &box, x, y, WIN_WIDTH, WIN_HEIGHT, 12.0f);

    static const wii::gx::GXColor white = {255, 255, 255, 255};
    static const wii::gx::GXColor yellow = {255, 230, 60, 255};
    static const wii::gx::GXColor grey = {170, 170, 170, 255};
    drawText(x + PAD, y - PAD, TITLE_SCALE, white, "Options");

    for (s32 i = 0; i < OPTION_COUNT; i++)
    {
        f32 rowY = y - ROWS_TOP - i * ROW_HEIGHT;
        bool on = get(static_cast<Option>(i));
        const wii::gx::GXColor & colour = i == sCursor ? yellow : white;
        drawText(x + PAD, rowY, ROW_SCALE, colour, sOptions[i].name);
        drawText(x + WIN_WIDTH - PAD - VALUE_WIDTH, rowY, ROW_SCALE, on ? colour : grey, on ? "ON" : "OFF");
    }
    drawText(x + PAD, y - WIN_HEIGHT + PAD + 22.0f, 0.6f, grey, "2: change    1: back");
}

static void windowMain(spm::pausewin::PausewinEntry * entry)
{
    u32 pressed = spm::wpadmgr::wpadGetButtonsPressed(0);
    u32 repeat = spm::wpadmgr::wpadGetButtonsHeldRepeat(0);

    if (pressed & WPAD_BTN_1)
    {
        spm::spmario_snd::spsndSFXOn("SFX_SYS_MENU_CANCEL1");
        // pluswinMain (state 50) goes back to the buttons once this window is gone
        spm::pausewin::pausewinDelete(entry->id);
        return;
    }
    if (pressed & WPAD_BTN_2)
    {
        spm::spmario_snd::spsndSFXOn("SFX_SYS_MENU_DESIDE1");
        toggle(static_cast<Option>(sCursor));
        return;
    }
    // The Wii Remote is held sideways: right is up, left is down
    s32 move = (repeat & WPAD_BTN_RIGHT) ? -1 : (repeat & WPAD_BTN_LEFT) ? 1 : 0;
    if (move != 0)
    {
        sCursor = (sCursor + move + OPTION_COUNT) % OPTION_COUNT;
        spm::spmario_snd::spsndSFXOn("SFX_SYS_MENU_CURSOR_MOVE1");
    }
}

// Replaces pluswinMapsWinOpen: pluswinMain has played the select sound and sets state 50 after this
static void windowOpen()
{
    spm::pausewin::PluswinWork * wp = spm::pausewin::pausewin_pluswinWp;
    sCursor = 0;
    s32 id = spm::pausewin::pausewinEntry(WIN_X, WIN_Y, WIN_WIDTH, WIN_HEIGHT, true, 0, 100, nullptr,
                                          windowMain, windowDisp, nullptr, nullptr);
    for (s8 & sub : wp->submenuEntryIds)
        sub = -1;
    wp->submenuEntryIds[0] = static_cast<s8>(id);
    wp->submenuFlags = 0;
    wp->submenuIsFullClose = false;
}

static bool patchCount(u32 * at, const char * what)
{
    // Expect "bl <count maps found>" (0x48xxxxx1)
    if ((*at & 0xFC000003) != 0x48000001)
    {
        wii::os::OSReport("[spmap] options: unexpected instruction %08x in %s, not patched\n", *at, what);
        return false;
    }
    *at = INSTR_LI_R3_1;
    wii::os::DCFlushRange(at, sizeof(u32));
    wii::os::ICInvalidateRange(at, sizeof(u32));
    return true;
}

void init()
{
    if (!patchCount(gamesyms::pluswin_open_maps_count, "the button loop")
        || !patchCount(gamesyms::pluswin_check_maps_count, "the cursor check"))
        return;
    patch::writeBranch(reinterpret_cast<void *>(gamesyms::pluswinMapsWinOpen), reinterpret_cast<void *>(windowOpen));
    wii::os::OSReport("[spmap] options: pause menu Options button on\n");
}

}
