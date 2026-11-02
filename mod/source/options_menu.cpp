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

#include "code_patching.h"
#include "game_addresses.h"
#include "options_menu.h"
#include "save_data.h"

namespace mod::options_menu {

// Each setting's name in the window and its default, in Option order
static const struct
{
    const char * name;
    bool defaultOn;
} sOptions[OPTION_COUNT] = {
    {"Music shuffle", false},
    {"Hold 1 to skip text", true},
    {"Shuffle fanfares", false},
    {"Shuffle sound effects", false},
    {"Low health beeping", true},
    {"Fast climbing", true},
};

// The order the window lists them in
static const Option sOrder[OPTION_COUNT] = {OPTION_MUSIC_SHUFFLE, OPTION_FANFARE_SHUFFLE, OPTION_SFX_SHUFFLE,
                                            OPTION_LOW_HEALTH_SOUND, OPTION_SKIP_TEXT, OPTION_FAST_CLIMB};

// ---- Layout (pause menu coordinates: 0 = screen centre, y going up) ----
#define WIN_X -110.0f        // to the right of the buttons
#define WIN_Y 185.0f
#define WIN_WIDTH 390.0f
#define PAD 18.0f            // space inside the window's edges
#define TITLE_SCALE 0.9f
#define ROW_SCALE 0.75f
#define FOOTER_SCALE 0.6f
#define ROWS_TOP 62.0f       // from the window's top to the first setting
#define ROW_HEIGHT 32.0f
#define VALUE_WIDTH 60.0f    // room for "ON" / "OFF"
#define CORNER 12.0f
// Tall enough for every row and the footer; it must stay above the help box at the bottom (y -120)
#define WIN_HEIGHT (ROWS_TOP + OPTION_COUNT * ROW_HEIGHT + 48.0f)

static const wii::gx::GXColor WHITE = {255, 255, 255, 255};
static const wii::gx::GXColor YELLOW = {255, 230, 60, 255};
static const wii::gx::GXColor GREY = {170, 170, 170, 255};
static const wii::gx::GXColor SEE_THROUGH_BLACK = {0, 0, 0, 0xC0};

// The pause menu's own sounds
#define SOUND_BACK "SFX_SYS_MENU_CANCEL1"
#define SOUND_CHANGE "SFX_SYS_MENU_DESIDE1"
#define SOUND_MOVE "SFX_SYS_MENU_CURSOR_MOVE1"

// The state the game gives a new pause window so it appears (and starts running)
#define WINDOW_STATE_APPEAR 10

// The help line the game shows while the cursor is on button 7 (the Options button)
#define HELP_KEY "menu_help_007"

// PowerPC "li r3, 1": register 3 = 1, which is how a function returns 1
#define INSTRUCTION_RETURN_1 0x38600001
// PowerPC "bl" (call): the top 6 bits are 18 and the lowest bit is 1
#define CALL_INSTRUCTION_MASK 0xFC000003
#define CALL_INSTRUCTION 0x48000001

// Which row the cursor is on
static s32 sCursor;

// ---- Settings ----

bool get(Option option)
{
    bool changed = (save_data::getGswByte(AP_GSW_OPTIONS) >> option) & 1;
    return changed != sOptions[option].defaultOn;
}

// Flips a setting (its saved bit) on or off.
static void toggle(Option option)
{
    save_data::setGswByte(AP_GSW_OPTIONS, save_data::getGswByte(AP_GSW_OPTIONS) ^ (1 << option));
    wii::os::OSReport("[spmap] options: %s %s\n", sOptions[option].name, get(option) ? "on" : "off");
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, HELP_KEY) == 0)
        return "Change additional settings for this file.";
    return nullptr;
}

// ---- The window ----

// Draws text at a spot, in the game's font.
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

// Draws the window: a dark box, the title, one row per setting (the cursor's row in yellow), and
// the button help. The pause menu calls this every frame.
static void windowDraw(spm::pausewin::PausewinEntry * entry)
{
    f32 x = entry->pos.x;
    f32 y = entry->pos.y;

    wii::mtx::Mtx34 noTransform;
    wii::mtx::PSMTXIdentity(noTransform);
    wii::gx::GXColor box = SEE_THROUGH_BLACK;
    spm::windowdrv::windowDispGX2_Waku_col(noTransform, 0, &box, x, y, WIN_WIDTH, WIN_HEIGHT, CORNER);

    drawText(x + PAD, y - PAD, TITLE_SCALE, WHITE, "Options");
    for (s32 row = 0; row < OPTION_COUNT; row++)
    {
        f32 rowY = y - ROWS_TOP - row * ROW_HEIGHT;
        bool on = get(sOrder[row]);
        const wii::gx::GXColor & colour = row == sCursor ? YELLOW : WHITE;
        drawText(x + PAD, rowY, ROW_SCALE, colour, sOptions[sOrder[row]].name);
        drawText(x + WIN_WIDTH - PAD - VALUE_WIDTH, rowY, ROW_SCALE, on ? colour : GREY, on ? "ON" : "OFF");
    }
    drawText(x + PAD, y - WIN_HEIGHT + PAD + 22.0f, FOOTER_SCALE, GREY, "2: change    1: back");
}

// Handles the buttons: 1 closes the window, 2 changes the setting under the cursor, the d-pad moves
// the cursor. The pause menu calls this every frame.
static void windowUpdate(spm::pausewin::PausewinEntry * entry)
{
    u32 pressed = spm::wpadmgr::wpadGetButtonsPressed(0);
    u32 repeat = spm::wpadmgr::wpadGetButtonsHeldRepeat(0);

    if (pressed & WPAD_BTN_1)
    {
        spm::spmario_snd::spsndSFXOn(SOUND_BACK);
        // The pause menu goes back to its buttons once this window is gone
        spm::pausewin::pausewinDelete(entry->id);
        return;
    }
    if (pressed & WPAD_BTN_2)
    {
        spm::spmario_snd::spsndSFXOn(SOUND_CHANGE);
        toggle(sOrder[sCursor]);
        return;
    }
    // The Wii Remote is held sideways: the d-pad's right is up, its left is down
    s32 move = (repeat & WPAD_BTN_RIGHT) ? -1 : (repeat & WPAD_BTN_LEFT) ? 1 : 0;
    if (move != 0)
    {
        sCursor = (sCursor + move + OPTION_COUNT) % OPTION_COUNT;
        spm::spmario_snd::spsndSFXOn(SOUND_MOVE);
    }
}

// Opens the window. Runs instead of the game's Maps window; the pause menu has already played its
// select sound, and afterwards waits until the window in its first sub-window slot is gone.
static void windowOpen()
{
    spm::pausewin::PluswinWork * pauseMenu = spm::pausewin::pausewin_pluswinWp;
    sCursor = 0;
    s32 id = spm::pausewin::pausewinEntry(WIN_X, WIN_Y, WIN_WIDTH, WIN_HEIGHT, true, 0, 100, nullptr,
                                          windowUpdate, windowDraw, nullptr, nullptr);
    // A new window does nothing until it's set to appear, as the game's own menus do
    spm::pausewin::PausewinEntry * window = spm::pausewin::pausewinGetEntry(id);
    window->state = WINDOW_STATE_APPEAR;
    window->stateProgress = 0;
    for (s8 & slot : pauseMenu->submenuEntryIds)
        slot = -1;
    pauseMenu->submenuEntryIds[0] = static_cast<s8>(id);
    pauseMenu->submenuFlags = 0;
    pauseMenu->submenuIsFullClose = false;
}

// ---- Setup ----

// Makes one "count maps found" call always answer 1, so the Maps button shows up.
static bool makeMapCountOne(u32 * at, const char * what)
{
    if ((*at & CALL_INSTRUCTION_MASK) != CALL_INSTRUCTION)
    {
        wii::os::OSReport("[spmap] options: unexpected instruction %08x in %s, not patched\n", *at, what);
        return false;
    }
    code_patching::writeInstruction(at, INSTRUCTION_RETURN_1);
    return true;
}

void init()
{
    if (!makeMapCountOne(game_addresses::pluswin_open_maps_count, "the button loop")
        || !makeMapCountOne(game_addresses::pluswin_check_maps_count, "the cursor check"))
        return;
    code_patching::writeBranch(reinterpret_cast<void *>(game_addresses::pluswinMapsWinOpen), reinterpret_cast<void *>(windowOpen));
    wii::os::OSReport("[spmap] options: pause menu Options button on\n");
}

}
