#include <common.h>
#include <spm/icondrv.h>
#include <msl/stdio.h>
#include <msl/string.h>
#include <spm/camdrv.h>
#include <spm/dispdrv.h>
#include <spm/fontmgr.h>
#include <spm/pausewin.h>
#include <spm/seq_load_sub.h>
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
    // Defaults: the user's own settings, 2026-11-14 (low health beeping off; Squirps limit, no ninjas,
    // Dorguy answers and rapid fire on). A save stores which settings differ from these, so a save made
    // before then reads those five the other way round once.
    {"Music shuffle", false},
    {"Hold 1 to skip text", true},
    {"Shuffle fanfares", false},
    {"Shuffle sound effects", false},
    {"Low health beeping", false},
    {"Fast climbing", true},
    {"Fast elevators", true},
    {"Skip cutscenes", true},
    {"Cull far enemies", true},
    {"Remove Squirps projectile limit", true},
    {"No ninjas in Sammer duels", true},
    {"Highlight Dorguy answers", true},
    {"Hold 2 for rapid fire Squirps", true},
};

// Each setting's help line (the help box under the list), in Option order
static const char * const sHelp[OPTION_COUNT] = {
    "Looping songs play in place of\neach other.",
    "Hold 1 to rush through text.",
    "Jingles that play once trade\nplaces too.",
    "Some sound effects play in place\nof each other.",
    "The beeping while your health\nis low.",
    "Climb ladders and vines twice\nas fast.",
    "Elevators skip their rides.",
    "Skip the story scenes the\nrandomizer can skip.",
    "Enemies far off screen pause and\nhide, so the game runs smoother.",
    "Squirps can have more than four\nshots out at once.",
    "Hold 2 to keep Squirps firing.",
    "No ninjas in the Sammer duels.",
    "The Dorguys' right answers show\nin red.",
};
static char sHelpKeys[OPTION_COUNT][24];

// The order the window lists them in
static const Option sOrder[OPTION_COUNT] = {OPTION_SKIP_CUTSCENES, OPTION_SKIP_TEXT, OPTION_FAST_CLIMB,
                                            OPTION_FAST_ELEVATORS, OPTION_CULL_ENEMIES, OPTION_MUSIC_SHUFFLE,
                                            OPTION_FANFARE_SHUFFLE, OPTION_SFX_SHUFFLE, OPTION_LOW_HEALTH_SOUND,
                                            OPTION_SQUIRPS_SHOTS, OPTION_SQUIRPS_RAPID, OPTION_NO_SAMMER_NINJAS,
                                            OPTION_DORGUY_ANSWERS};
#define FILE_SELECT_ROWS OPTION_COUNT
#define PAUSE_ROWS OPTION_COUNT
static const Option * const sPauseOrder = sOrder;

// ---- Layout (pause menu coordinates: 0 = screen centre, y going up) ----
#define WIN_X -110.0f        // to the right of the buttons
#define WIN_Y 200.0f
#define WIN_WIDTH 390.0f
#define PAD 18.0f            // space inside the window's edges
#define TITLE_SCALE 0.9f
#define ROW_SCALE 0.7f
#define FOOTER_SCALE 0.6f
#define ROWS_TOP 56.0f       // from the window's top to the first setting
#define ROW_HEIGHT 18.0f
// The window shows this many settings at once and scrolls through the rest with the cursor (user,
// 2026-11-12: rather than squeezing rows in as settings are added)
#define VISIBLE_ROWS 10
// The arrows saying there's more above / below: the game's small red arrow (icon 319, wicon.tpl picture
// 352, 32x32; user's pick, 2026-11-12), the down one turned round
#define ARROW_ICON 319
#define ARROW_SCALE 0.7f     // 32 pixels -> about 22
#define ARROW_SIZE (32.0f * ARROW_SCALE)
#define ARROW_FLAGS 0x4010   // as file_select_hearts' icons
#define VALUE_WIDTH 60.0f    // room for "ON" / "OFF"
#define CORNER 12.0f
// Tall enough for every row and the footer; in the pause menu it must stay above the help box at the
// bottom (y -120)
#define WIN_HEIGHT_FOR(rows) (ROWS_TOP + (rows) * ROW_HEIGHT + 48.0f)
#define SHOWN_ROWS (OPTION_COUNT < VISIBLE_ROWS ? OPTION_COUNT : VISIBLE_ROWS)
#define WIN_HEIGHT WIN_HEIGHT_FOR(SHOWN_ROWS)

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
static s32 sScroll;  // the first setting shown

// Moves the cursor by `move` rows (wrapping round) and scrolls to keep it shown.
static void moveCursor(s32 move, s32 rows, s32 visible)
{
    sCursor = (sCursor + move + rows) % rows;
    if (sCursor < sScroll)
        sScroll = sCursor;
    else if (sCursor >= sScroll + visible)
        sScroll = sCursor - visible + 1;
}

static void resetCursor()
{
    sCursor = 0;
    sScroll = 0;
}

// ---- Settings ----

// The loaded save's settings bits (both bytes).
static u32 savedBits()
{
    return save_data::getGswByte(AP_GSW_OPTIONS) | (save_data::getGswByte(AP_GSW_OPTIONS_2) << 8);
}

static void setSavedBits(u32 bits)
{
    save_data::setGswByte(AP_GSW_OPTIONS, bits & 0xFF);
    save_data::setGswByte(AP_GSW_OPTIONS_2, (bits >> 8) & 0xFF);
}

// Whether a setting is on, from its saved bits (a bit set = changed from its default).
static bool isOn(u32 bits, Option option)
{
    bool changed = (bits >> option) & 1;
    return changed != sOptions[option].defaultOn;
}

bool get(Option option)
{
    return isOn(savedBits(), option);
}

static_assert(OPTION_COUNT <= 16, "the settings are two save bytes: a 17th needs a third");

// The settings of the last save played (its saved bits; 0 = every default), for the next new file
static u16 sRemembered;

void remember()
{
    sRemembered = static_cast<u16>(savedBits());
}

void carryOverToNewFile()
{
    setSavedBits(sRemembered);
    wii::os::OSReport("[spmap] options: new file, settings carried over (%02x)\n", sRemembered);
}

// Flips a setting (its saved bit) on or off.
static void toggle(Option option)
{
    setSavedBits(savedBits() ^ (1 << option));
    remember();
    wii::os::OSReport("[spmap] options: %s %s\n", sOptions[option].name, get(option) ? "on" : "off");
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, HELP_KEY) == 0)
        return "Change additional settings for this file.";
    for (s32 option = 0; option < OPTION_COUNT; option++)
    {
        if (sHelpKeys[option][0] != '\0' && msl::string::strcmp(msgName, sHelpKeys[option]) == 0)
            return sHelp[option];
    }
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

// Draws the red arrow with its top middle at (x, y), pointing up or (turned round) down.
static void drawArrow(f32 x, f32 y, bool down)
{
    wii::mtx::Mtx34 at, turn, scale;
    // icons hang down from the point they're drawn at; turned round, the down one hangs up from its
    // bottom, so that's where it's put
    wii::mtx::PSMTXTrans(at, x, down ? y - ARROW_SIZE : y, 0.0f);
    wii::mtx::PSMTXRotRad(down ? 3.14159265f : 0.0f, turn, 'z');
    wii::mtx::PSMTXScale(scale, ARROW_SCALE, ARROW_SCALE, ARROW_SCALE);
    wii::mtx::PSMTXConcat(at, turn, at);
    wii::mtx::PSMTXConcat(at, scale, at);
    spm::icondrv::iconDispGxCol(at, ARROW_FLAGS, ARROW_ICON, {255, 255, 255, 255});
}

// Draws the window: a dark box, the title, one row per setting (the cursor's row in yellow), and
// the button help. The pause menu calls this every frame.
static void drawWindow(f32 x, f32 y, u32 bits, const Option * order, s32 rows, const char * title,
                       const char * footer)
{
    wii::mtx::Mtx34 noTransform;
    wii::mtx::PSMTXIdentity(noTransform);
    wii::gx::GXColor box = SEE_THROUGH_BLACK;
    s32 shown = rows < VISIBLE_ROWS ? rows : VISIBLE_ROWS;
    f32 height = WIN_HEIGHT_FOR(shown);
    spm::windowdrv::windowDispGX2_Waku_col(noTransform, 0, &box, x, y, WIN_WIDTH, height, CORNER);

    drawText(x + PAD, y - PAD, TITLE_SCALE, WHITE, title);
    // over the ON/OFF column: up beside the title, down beside the footer
    const f32 arrowX = x + WIN_WIDTH - PAD - VALUE_WIDTH / 2;
    if (sScroll > 0)
        drawArrow(arrowX, y - PAD + 2.0f, false);
    if (sScroll + shown < rows)
        drawArrow(arrowX, y - height + PAD + 24.0f, true);
    for (s32 row = sScroll; row < sScroll + shown; row++)
    {
        f32 rowY = y - ROWS_TOP - (row - sScroll) * ROW_HEIGHT;
        bool on = isOn(bits, order[row]);
        const wii::gx::GXColor & colour = row == sCursor ? YELLOW : WHITE;
        // A name too long for its room is drawn smaller to fit
        const char * name = sOptions[order[row]].name;
        f32 room = WIN_WIDTH - 2 * PAD - VALUE_WIDTH - 8.0f;
        f32 width = spm::fontmgr::FontGetMessageWidth(name) * ROW_SCALE;
        f32 scale = width > room ? ROW_SCALE * room / width : ROW_SCALE;
        drawText(x + PAD, rowY, scale, colour, name);
        drawText(x + WIN_WIDTH - PAD - VALUE_WIDTH, rowY, ROW_SCALE, on ? colour : GREY, on ? "ON" : "OFF");
    }
    drawText(x + PAD, y - height + PAD + 22.0f, FOOTER_SCALE, GREY, footer);
}

/*
    In the pause menu the window opens the way the game's own submenus do (user, 2026-11-12, from a video of
    them; copied from pluswinKeyItemWinOpen and pluswinKeyItemMain, US 0x80187A20 and 0x80187D78): the
    other buttons and the stats panel fade out, the Options button slides to the panel's top left, a light
    list box opens in the panel, the game's hand cursor points at the setting, and the help box below
    describes it. Closing undoes it all.
*/
#define BUTTON_OPTIONS 7       // PluswinWork entryIds: the 8 buttons, then
#define ENTRY_PANEL 8          // the coloured panel behind everything
#define ENTRY_HELP 9           // the help box at the bottom
#define ENTRY_STATS 10         // the stats table
#define STATE_APPEAR 10        // PausewinEntry states the game's menus use
#define STATE_DISAPPEAR 20
#define STATE_MOVE 30
#define ENTRY_IN_USE 0x1
#define ENTRY_DELETE_AFTER 0x2    // set with STATE_DISAPPEAR: the entry is deleted once it's gone
#define BUTTON_SELECTABLE 0x100   // cleared on the button while its submenu is open
#define BUTTON_CORNER_X 10.0f     // the button's spot while open: the panel's top left, in by this
#define BUTTON_CORNER_Y 10.0f
// Sizes from the user's tests (2026-11-13): first "a bit too small, and a bit too high up", then about
// 80% the size of the Cards list (513 x 257 there), centred where that list sits, 8 rows like a column
// of cards
#define LIST_TOP 75.0f            // the list box's top, below the panel's
#define LIST_VISIBLE_ROWS 8
#define LIST_SHOWN_ROWS (PAUSE_ROWS < LIST_VISIBLE_ROWS ? PAUSE_ROWS : LIST_VISIBLE_ROWS)
#define LIST_WIDTH 410.0f
#define LIST_PAD 16.0f
#define LIST_TEXT_IN 30.0f        // from the box's left to the settings' names
#define LIST_SCALE 0.84f          // text size in the list
#define LIST_ROW_HEIGHT 22.0f
#define LIST_VALUE_WIDTH 72.0f    // room for "ON" / "OFF"
#define LIST_ROWS_TOP 16.0f       // from the box's top to the first setting's text
#define LIST_HEIGHT (LIST_ROWS_TOP + LIST_SHOWN_ROWS * LIST_ROW_HEIGHT + 16.0f)
#define HAND_IN 10.0f             // the hand cursor's spot: this far in from the box's left (first try 0
                                  // was too far left)
#define HAND_BELOW_TEXT 17.0f     // the hand cursor points this far below a row's text top
#define ARROW_BLINK_FRAMES 40     // the red arrows blink, as in the game's lists: shown for the first
#define ARROW_SHOWN_FRAMES 26     // this many frames of every ARROW_BLINK_FRAMES
#define HAND_BACK_X 25.0f         // where the game puts the hand on a button: its spot less these
#define HAND_BACK_Y 30.0f
#define HELP_FORMAT_KEY "ap_option_help_%02d"

static const wii::gx::GXColor LIST_BOX = {250, 250, 255, 230};
static const wii::gx::GXColor LIST_TEXT = {45, 45, 55, 255};
static const wii::gx::GXColor LIST_ON = {30, 130, 40, 255};
static const wii::gx::GXColor LIST_OFF = {150, 150, 160, 255};


static bool sClosing;  // the window is fading out
static u32 sBlinkFrame;  // counts frames for the arrows' blinking

static spm::pausewin::PausewinEntry * pauseEntry(s32 index)
{
    s32 id = spm::pausewin::pausewin_pluswinWp->entryIds[index];
    if (id < 0)
        return nullptr;
    spm::pausewin::PausewinEntry * entry = spm::pausewin::pausewinGetEntry(id);
    return (entry->flags & ENTRY_IN_USE) ? entry : nullptr;
}

static void setState(spm::pausewin::PausewinEntry * entry, s32 state)
{
    if (entry == nullptr)
        return;
    entry->state = state;
    entry->stateProgress = 0;
}

static void slideTo(spm::pausewin::PausewinEntry * entry, f32 x, f32 y)
{
    if (entry == nullptr)
        return;
    entry->moveStart = entry->pos;
    entry->moveDest.x = x;
    entry->moveDest.y = y;
    setState(entry, STATE_MOVE);
}

// The help box shows the setting under the cursor.
static void showHelp()
{
    s32 option = sPauseOrder[sCursor];
    msl::stdio::sprintf(sHelpKeys[option], HELP_FORMAT_KEY, option);
    spm::pausewin::PausewinEntry * help = pauseEntry(ENTRY_HELP);
    if (help != nullptr)
        spm::pausewin::pausewinSetMessage(help, 0, sHelpKeys[option]);
}

// Text in the list: dark, without the white edge the window's other text has.
static void drawListText(f32 x, f32 y, f32 scale, const wii::gx::GXColor & colour, const char * text)
{
    wii::gx::GXColor c = colour;
    spm::fontmgr::FontDrawStart();
    spm::fontmgr::FontDrawColor(&c);
    spm::fontmgr::FontDrawScale(scale);
    spm::fontmgr::FontDrawNoiseOff();
    spm::fontmgr::FontDrawRainbowColorOff();
    spm::fontmgr::FontDrawString(x, y, text);
}

static void windowDraw(spm::pausewin::PausewinEntry * entry)
{
    f32 x = entry->pos.x, y = entry->pos.y;
    wii::mtx::Mtx34 noTransform;
    wii::mtx::PSMTXIdentity(noTransform);
    wii::gx::GXColor box = LIST_BOX;
    box.a = static_cast<u8>(box.a * entry->alpha / 255);
    spm::windowdrv::windowDispGX2_Waku_col(noTransform, 0, &box, x, y, LIST_WIDTH, LIST_HEIGHT, CORNER);
    if (entry->alpha < 255)
        return;  // the text comes once the box is fully there, as in the game's lists
    u32 bits = savedBits();
    for (s32 row = sScroll; row < sScroll + LIST_SHOWN_ROWS; row++)
    {
        f32 rowY = y - LIST_ROWS_TOP - (row - sScroll) * LIST_ROW_HEIGHT;
        Option option = sPauseOrder[row];
        bool on = isOn(bits, option);
        const char * name = sOptions[option].name;
        const f32 valueX = x + LIST_WIDTH - LIST_PAD - LIST_VALUE_WIDTH + 14.0f;
        f32 room = valueX - (x + LIST_TEXT_IN) - 10.0f;
        f32 width = spm::fontmgr::FontGetMessageWidth(name) * LIST_SCALE;
        f32 scale = width > room ? LIST_SCALE * room / width : LIST_SCALE;
        drawListText(x + LIST_TEXT_IN, rowY, scale, LIST_TEXT, name);
        drawListText(valueX, rowY, LIST_SCALE, on ? LIST_ON : LIST_OFF, on ? "ON" : "OFF");
    }
    if (sBlinkFrame++ % ARROW_BLINK_FRAMES >= ARROW_SHOWN_FRAMES)
        return;
    const f32 arrowX = x + LIST_WIDTH - 14.0f;
    if (sScroll > 0)
        drawArrow(arrowX, y - 2.0f, false);
    if (sScroll + LIST_SHOWN_ROWS < PAUSE_ROWS)
        drawArrow(arrowX, y - LIST_HEIGHT + 24.0f, true);
}

// Points the game's hand cursor at the setting under the cursor.
static void pointHand(spm::pausewin::PausewinEntry * entry)
{
    spm::pausewin::PluswinWork * pauseMenu = spm::pausewin::pausewin_pluswinWp;
    pauseMenu->cursorMoveDest.x = entry->pos.x + HAND_IN;
    pauseMenu->cursorMoveDest.y =
        entry->pos.y - LIST_ROWS_TOP - (sCursor - sScroll) * LIST_ROW_HEIGHT - HAND_BELOW_TEXT;
}

// Closes the window the way the game's submenus close: it fades out (and is then deleted, which sends
// the pause menu back to its buttons), everything else comes back, the button slides home.
static void windowClose(spm::pausewin::PausewinEntry * entry)
{
    sClosing = true;
    entry->flags |= ENTRY_DELETE_AFTER;
    setState(entry, STATE_DISAPPEAR);
    for (s32 i = 0; i < BUTTON_OPTIONS; i++)
        setState(pauseEntry(i), STATE_APPEAR);
    setState(pauseEntry(ENTRY_STATS), STATE_APPEAR);
    spm::pausewin::PausewinEntry * button = pauseEntry(BUTTON_OPTIONS);
    if (button != nullptr)
    {
        slideTo(button, button->originalPos.x, button->originalPos.y);
        button->flags |= BUTTON_SELECTABLE;
        spm::pausewin::pausewin_pluswinWp->cursorMoveDest.x = button->originalPos.x - HAND_BACK_X;
        spm::pausewin::pausewin_pluswinWp->cursorMoveDest.y = button->originalPos.y - HAND_BACK_Y;
    }
    spm::pausewin::PausewinEntry * help = pauseEntry(ENTRY_HELP);
    if (help != nullptr)
        spm::pausewin::pausewinSetMessage(help, 0, HELP_KEY);
}

// Handles the buttons: 1 closes the window, 2 changes the setting under the cursor, the d-pad moves
// the cursor. The pause menu calls this every frame.
static void windowUpdate(spm::pausewin::PausewinEntry * entry)
{
    if (sClosing)
        return;
    u32 pressed = spm::wpadmgr::wpadGetButtonsPressed(0);
    u32 repeat = spm::wpadmgr::wpadGetButtonsHeldRepeat(0);

    if (pressed & WPAD_BTN_1)
    {
        spm::spmario_snd::spsndSFXOn(SOUND_BACK);
        windowClose(entry);
        return;
    }
    if (pressed & WPAD_BTN_2)
    {
        spm::spmario_snd::spsndSFXOn(SOUND_CHANGE);
        toggle(sPauseOrder[sCursor]);
        return;
    }
    // The Wii Remote is held sideways: the d-pad's right is up, its left is down
    s32 move = (repeat & WPAD_BTN_RIGHT) ? -1 : (repeat & WPAD_BTN_LEFT) ? 1 : 0;
    if (move != 0)
    {
        moveCursor(move, PAUSE_ROWS, LIST_SHOWN_ROWS);
        spm::spmario_snd::spsndSFXOn(SOUND_MOVE);
        showHelp();
    }
    pointHand(entry);
}

// Opens the window. Runs instead of the game's Maps window; the pause menu has already played its
// select sound, and afterwards waits until the window in its first sub-window slot is gone.
static void windowOpen()
{
    spm::pausewin::PluswinWork * pauseMenu = spm::pausewin::pausewin_pluswinWp;
    resetCursor();
    sClosing = false;
    sBlinkFrame = 0;
    spm::pausewin::PausewinEntry * panel = pauseEntry(ENTRY_PANEL);
    f32 x = panel ? panel->pos.x + panel->width / 2 - LIST_WIDTH / 2 : WIN_X;
    f32 y = panel ? panel->pos.y - LIST_TOP : WIN_Y;
    s32 id = spm::pausewin::pausewinEntry(x, y, LIST_WIDTH, LIST_HEIGHT, true, 0, 100, nullptr, windowUpdate,
                                          windowDraw, nullptr, nullptr);
    // A new window does nothing until it's set to appear, as the game's own menus do
    spm::pausewin::PausewinEntry * window = spm::pausewin::pausewinGetEntry(id);
    setState(window, STATE_APPEAR);
    for (s8 & slot : pauseMenu->submenuEntryIds)
        slot = -1;
    pauseMenu->submenuEntryIds[0] = static_cast<s8>(id);
    pauseMenu->submenuFlags = 0;
    pauseMenu->submenuIsFullClose = false;

    // Everything else fades out, and the button slides to the panel's top left
    for (s32 i = 0; i < BUTTON_OPTIONS; i++)
        setState(pauseEntry(i), STATE_DISAPPEAR);
    setState(pauseEntry(ENTRY_STATS), STATE_DISAPPEAR);
    spm::pausewin::PausewinEntry * button = pauseEntry(BUTTON_OPTIONS);
    if (button != nullptr && panel != nullptr)
    {
        slideTo(button, panel->pos.x + BUTTON_CORNER_X, panel->pos.y - BUTTON_CORNER_Y);
        button->flags &= ~BUTTON_SELECTABLE;
    }
    pointHand(window);
    showHelp();
}

// ---- On the file select ----

#define FILE_SELECT_WIN_X (-WIN_WIDTH / 2)  // centred

static bool sFileSelectOpen;

static void drawFileSelect(s32 cameraId, void * param)
{
    (void) cameraId;
    (void) param;
    if (sFileSelectOpen)
        drawWindow(FILE_SELECT_WIN_X, WIN_Y, sRemembered, sOrder, FILE_SELECT_ROWS, "Options for new files",
                   "2: change    1: back");
}

/*
    The Options button. The game knows seven buttons (SeqLoadSubWork curSelection: 0-3 the files, 4-6
    Erase Data, Copy Data, Back), so Options is selection 7, which no game button answers to: none of
    them lights up while it's selected. Its cursor table entry would be copy mode's first (background
    colour 4, as the bottom row's; cursor position 0, 0, which copy mode doesn't use), where the
    Options hand position is kept while it's selected. The mod takes the d-pad and 2 itself there.
    It's drawn right after Back, through the button loop's own drawing calls, the way the game draws a
    button: dim when not selected, raised over a shadow when it is.
*/
#define SEL_ERASE 4
#define SEL_COPY 5
#define SEL_BACK 6
#define SEL_OPTIONS 7
#define FILESEL_STATE_CHOOSE 10      // SeqLoadSubWork state while choosing a button (seq_load_sub_loadMain)
#define SEL_FILE_2 1
#define SEL_FILE_4 3
#define BACK_PICTURE 7
#define FILESEL_OPTIONS_PICTURE 32   // added to filesel.tpl by the patcher (patcher.py patch_filesel)
#define ROW_COUNT 3
static const f32 sRowVanillaX[ROW_COUNT] = {-200.0f, 0.0f, 200.0f};
static const f32 sHandVanillaX[ROW_COUNT] = {-280.0f, -80.0f, 120.0f};
// Erase, Copy, Back; Options goes between Copy and Back (146 apart: the borders 4 apart)
static const f32 sRowX[ROW_COUNT] = {-219.0f, -73.0f, 219.0f};
#define OPTIONS_X 73.0f
#define ROW_Y -180.0f
#define HAND_Y -190.0f
#define HAND_OFFSET_X -72.0f   // vanilla -80; less, so the Erase hand stays on screen
#define SELECTED_AFTER 12      // the game shows a button selected once colorIntplProgress passes this
#define RAISED_BY 5.0f
// The button loop's two drawing calls made once per button per frame (vanilla: bl fileselDrawPicture):
// a selected button's shadow, and an unselected button
#define DRAW_SELECTED_CALL 0x8017f5ec
#define DRAW_UNSELECTED_CALL 0x8017f674

// The game's d-pad on the file select, the Wii Remote held sideways
#define SCREEN_LEFT WPAD_BTN_UP
#define SCREEN_RIGHT WPAD_BTN_DOWN
#define SCREEN_UP WPAD_BTN_RIGHT
#define SCREEN_DOWN WPAD_BTN_LEFT

static bool sRowMoved;
static wii::mtx::Vec2 sSavedHand;  // copy mode's first cursor position, while the Options one is there

static void drawPicture(s32 picture, f32 x, f32 y, wii::gx::GXColor colour)
{
    wii::mtx::Mtx34 mtx;
    wii::mtx::PSMTXTrans(mtx, x, y, 0.0f);
    game_addresses::fileselDrawPicture(picture, &mtx, &colour);
}

// The button loop's drawing calls: the game's button, then Options right after Back
static void drawButtonHook(s32 picture, wii::mtx::Mtx34 * mtx, wii::gx::GXColor * colour)
{
    game_addresses::fileselDrawPicture(picture, mtx, colour);
    if (picture != BACK_PICTURE)
        return;
    spm::seq_load_sub::SeqLoadSubWork * wp = spm::seq_load_sub::seq_load_sub_wp;
    if (wp->curSelection == SEL_OPTIONS && wp->colorIntplProgress > SELECTED_AFTER)
    {
        drawPicture(FILESEL_OPTIONS_PICTURE, OPTIONS_X, ROW_Y, {0, 0, 0, 96});
        drawPicture(FILESEL_OPTIONS_PICTURE, OPTIONS_X - RAISED_BY, ROW_Y + RAISED_BY, {255, 255, 255, 255});
    }
    else
        drawPicture(FILESEL_OPTIONS_PICTURE, OPTIONS_X, ROW_Y, {127, 127, 127, 255});
}

// Moves the file select's cursor (the game animates the hand and colours from there).
static void select(spm::seq_load_sub::SeqLoadSubWork * wp, s32 selection)
{
    spm::seq_load_sub::SeqLoadButtonDef & optionsEntry = game_addresses::fileselect_button_defs[SEL_OPTIONS];
    if (selection == SEL_OPTIONS && wp->curSelection != SEL_OPTIONS)
    {
        sSavedHand = optionsEntry.cursorPos;
        optionsEntry.cursorPos = {OPTIONS_X + HAND_OFFSET_X, HAND_Y};
    }
    else if (selection != SEL_OPTIONS && wp->curSelection == SEL_OPTIONS)
        optionsEntry.cursorPos = sSavedHand;
    wp->prevSelection = wp->curSelection;
    wp->curSelection = selection;
    wp->colorIntplProgress = 0;
    spm::spmario_snd::spsndSFXOn(SOUND_MOVE);
}

// Squeezes the bottom row and points the button loop's drawing at drawButtonHook. Once, after
// checking every word is vanilla.
static void moveRow()
{
    sRowMoved = true;
    auto * row = game_addresses::fileselect_bottom_buttons;
    auto * defs = game_addresses::fileselect_button_defs;
    for (s32 i = 0; i < ROW_COUNT; i++)
    {
        if (row[i].x != sRowVanillaX[i] || row[i].y != ROW_Y || defs[SEL_ERASE + i].cursorPos.x != sHandVanillaX[i])
        {
            wii::os::OSReport("[spmap] options: the file select's buttons aren't where expected, no Options button\n");
            return;
        }
    }
    static const u32 sCalls[] = {DRAW_SELECTED_CALL, DRAW_UNSELECTED_CALL};
    for (u32 at : sCalls)
    {
        u32 * instruction = reinterpret_cast<u32 *>(at);
        u32 expected = CALL_INSTRUCTION | ((reinterpret_cast<u32>(game_addresses::fileselDrawPicture) - at) & 0x03FFFFFC);
        if (*instruction != expected)
        {
            wii::os::OSReport("[spmap] options: unexpected instruction %08x at %08x, no Options button\n", *instruction, at);
            return;
        }
    }
    for (s32 i = 0; i < ROW_COUNT; i++)
    {
        row[i].x = sRowX[i];
        defs[SEL_ERASE + i].cursorPos.x = sRowX[i] + HAND_OFFSET_X;
    }
    code_patching::writeBranch(reinterpret_cast<void *>(DRAW_SELECTED_CALL), reinterpret_cast<void *>(drawButtonHook), true);
    code_patching::writeBranch(reinterpret_cast<void *>(DRAW_UNSELECTED_CALL), reinterpret_cast<void *>(drawButtonHook), true);
    wii::os::OSReport("[spmap] options: file select Options button added\n");
}

// The d-pad and 2 around the Options button. Returns true if the mod took this frame's buttons.
static bool optionsButtonInput(u32 pressed)
{
    spm::seq_load_sub::SeqLoadSubWork * wp = spm::seq_load_sub::seq_load_sub_wp;
    // Only while choosing a button (state 10); erasing (1000s), copying (2000s), loading and the rest
    // are the file select's own. (flags bits 0 and 1 aren't those modes: they make Erase and Copy
    // answer with a buzzer, set while there's no save to erase or copy, so don't test them.)
    if (wp == nullptr || wp->state != FILESEL_STATE_CHOOSE)
        return false;
    s32 sel = wp->curSelection;
    if ((sel == SEL_COPY && (pressed & SCREEN_RIGHT)) || (sel == SEL_BACK && (pressed & SCREEN_LEFT)))
    {
        select(wp, SEL_OPTIONS);
        return true;
    }
    if (sel != SEL_OPTIONS)
        return false;
    if (pressed & WPAD_BTN_2)
    {
        sFileSelectOpen = true;
        resetCursor();
        spm::spmario_snd::spsndSFXOn(SOUND_CHANGE);
    }
    else if (pressed & SCREEN_LEFT)
        select(wp, SEL_COPY);
    else if (pressed & SCREEN_RIGHT)
        select(wp, SEL_BACK);
    else if (pressed & SCREEN_UP)
        select(wp, SEL_FILE_4);
    else if (pressed & SCREEN_DOWN)
        select(wp, SEL_FILE_2);
    return true;  // nothing reaches the game while Options is selected
}

// Hides this frame's buttons from the file select (the game reads them from the Wii Remote work).
static void hideButtons()
{
    auto & status = game_addresses::wpadmgr_wp->statuses[0][0];
    status.buttonsHeld = 0;
    status.buttonsPressed = 0;
    status.buttonsReleased = 0;
}

void fileSelectTick()
{
    if (!sRowMoved)
        moveRow();
    u32 pressed = spm::wpadmgr::wpadGetButtonsPressed(0);
    u32 repeat = spm::wpadmgr::wpadGetButtonsHeldRepeat(0);
    if (!sFileSelectOpen)
    {
        if (optionsButtonInput(pressed))
            hideButtons();
    }
    else
    {
        if (pressed & WPAD_BTN_1)
        {
            sFileSelectOpen = false;
            spm::spmario_snd::spsndSFXOn(SOUND_BACK);
        }
        else if (pressed & WPAD_BTN_2)
        {
            sRemembered ^= static_cast<u16>(1 << sOrder[sCursor]);
            spm::spmario_snd::spsndSFXOn(SOUND_CHANGE);
            wii::os::OSReport("[spmap] options: new files: %s %s\n", sOptions[sOrder[sCursor]].name,
                              isOn(sRemembered, sOrder[sCursor]) ? "on" : "off");
        }
        else
        {
            s32 move = (repeat & WPAD_BTN_RIGHT) ? -1 : (repeat & WPAD_BTN_LEFT) ? 1 : 0;
            if (move != 0)
            {
                moveCursor(move, FILE_SELECT_ROWS, VISIBLE_ROWS);
                spm::spmario_snd::spsndSFXOn(SOUND_MOVE);
            }
        }
        hideButtons();
    }
    spm::dispdrv::dispEntry(spm::camdrv::CAM_ID_DEBUG_3D, 2, 0.0f, drawFileSelect, nullptr);
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
