#include <common.h>
#include <msl/string.h>
#include <spm/mario_pouch.h>
#include <spm/msgdrv.h>
#include <spm/pausewin.h>
#include <spm/wpadmgr.h>

#include "endgame_requirements.h"
#include "game_mode.h"
#include "pause_chapters.h"
#include "pure_hearts.h"
#include "starting_character.h"
#include "story_progress.h"

namespace mod::pause_chapters {

#define CHAPTER_COUNT 8
#define SECTION_COUNT 4
#define SAMMER_CHAPTER 6     // its tower door only ever leads to its start (tower_doors SAMMER_DOOR)
#define CASTLE_CHAPTER 8
#define SHOWN (-0x7FFFFFFF)  // any story value is at least this
#define HIDDEN 0x7FFFFFFF    // and none reaches this

// The pause menu's Chapters tab: the story value each section needs to be listed, chapter by chapter
// (main.dol 0x803EA208, 8 x 4 words; Chapter 6's last two are -1 and -2, the post-game flags)
static s32 * const sSectionValues = reinterpret_cast<s32 *>(0x803EA208);

// The value last written for each section (0 = not yet)
static s32 sWritten[CHAPTER_COUNT * SECTION_COUNT];

#define BOOMER 222  // ITEM_ID_FAIRY_BAKUDAN
#define FLEEP 226   // ITEM_ID_FAIRY_KUUKAN

static bool hasPixl(s32 pixl)
{
    for (auto & info : spm::mario_pouch::pouchGetPtr()->pixls)
        if (info.itemType == pixl)
            return true;
    return false;
}

static bool doorOpen(s32 chapter)
{
    // Chapter 8's door is in Flopside: Boomer and Fleep get there, and Mario's flip past the door's
    // own way in (user, 2026-11-09)
    if (chapter == CASTLE_CHAPTER)
        return endgame_requirements::castleOpen() && starting_character::marioHeld() && hasPixl(BOOMER) &&
               hasPixl(FLEEP);
    return pure_hearts::isDoorOpen(chapter);
}

// Whether the tower's door for a chapter leads to a section: the door is open, and the section is the
// first, or one the chapter has reached (the door's section menu)
static bool reachable(s32 chapter, s32 section)
{
    if (!doorOpen(chapter))
        return false;
    if (section == 1)
        return true;
    if (chapter == SAMMER_CHAPTER)
        return false;
    return story_progress::sectionsReached(chapter) >= section;
}

bool sectionReachable(s32 chapter, s32 section)
{
    return reachable(chapter, section);
}

// ---- The cursor ----

/*
    The chapter grid (4 x 2) moves the cursor through a neighbour table (main.dol 0x803EA328: up, down,
    left, right, then that cell's "unlocked?" function) and skips cells whose function says no, which
    left unlocked chapters out of reach past locked ones (user, 2026-11-09). The eight functions (from
    the table) now always say yes, so the hand can rest on a locked (blank) chapter; pressing
    2 there does nothing (the press, read at 0x8018A02C, is dropped). The whole grid also ignored input
    unless Chapter 1 counted as reached (0x8018A024, skipped now).
*/
static const u32 sCellChecks[CHAPTER_COUNT] = {0x80189B5C, 0x80189BA0, 0x80189BE8, 0x80189C30,
                                               0x80189C78, 0x80189CC0, 0x80189D08, 0x80189D50};
#define GRID_INPUT_GATE 0x8018A024  // blt past the grid's input when Chapter 1 isn't reached
#define SELECT_PRESS_READ 0x8018A02C // bl wpadGetButtonsPressed: the press that opens a chapter
#define BTN_SELECT 0x100             // 2
#define CURSOR_CELL 0x164            // in the pluswin work: the cell the hand is on (0-7 the chapters)
#define LI_R3_1 0x38600001
#define BLR 0x4E800020
#define NOP 0x60000000

static u8 * pluswinWork()
{
    return reinterpret_cast<u8 *>(spm::pausewin::pausewin_pluswinWp);
}

static s32 cursorCell()
{
    return *reinterpret_cast<s32 *>(pluswinWork() + CURSOR_CELL);
}

// Whether the hand is on a locked (blank) chapter
static bool onLockedChapter()
{
    s32 cell = cursorCell();
    return cell >= 0 && cell < CHAPTER_COUNT && !reachable(cell + 1, 1);
}

static u32 selectPress(s32 channel)
{
    u32 pressed = spm::wpadmgr::wpadGetButtonsPressed(channel);
    if (onLockedChapter())
        pressed &= ~BTN_SELECT;
    return pressed;
}

/*
    Hovering a locked chapter shows nothing (user, 2026-11-09): no place name in the title box (its
    msgSearch at 0x8018B924) and no description (the message set at 0x8018A4DC, "msg_map_stgN").
*/
#define BLANK_KEY "ap_blank"
#define TITLE_LOOKUP 0x8018B924   // bl msgSearch: the title box's place name
#define HOVER_MESSAGE 0x8018A4DC  // bl pausewinSetMessage: the chapter's description

static const char * titleLookup(const char * msgName)
{
    return spm::msgdrv::msgSearch(onLockedChapter() ? BLANK_KEY : msgName);
}

static void hoverMessage(spm::pausewin::PausewinEntry * entry, s32 itemId, const char * msgName)
{
    spm::pausewin::pausewinSetMessage(entry, itemId, onLockedChapter() ? BLANK_KEY : msgName);
}

/*
    Moving the hand clears the old chapter's highlight and sets the new one's: flag 0x10 of each one's
    button window (0x8018A550-0x8018A5AC), found through the work's button ids (+0xA0, one byte per cell).
    A locked chapter has no button (id -1), so that wrote just before the window list. The mod does it
    instead, skipping missing buttons, and gives back the cursor sound's name the game plays next.
*/
#define HIGHLIGHT_START 0x8018A550
#define HIGHLIGHT_RESUME 0x8018A5B0   // bl spsndSFXOn(r3)
#define BUTTON_IDS 0xA0
#define WINDOW_SIZE 0x78
#define WINDOW_HIGHLIGHT 0x10
#define MR_R3_R19 0x7E639B78           // the old cell is in r19 there
#define CURSOR_SOUND "SFX_SYS_MENU_CURSOR_MOVE2"

static void setHighlight(s32 cell, bool on)
{
    s8 id = static_cast<s8>(pluswinWork()[BUTTON_IDS + cell]);
    if (id < 0)
        return;
    u8 * window = reinterpret_cast<u8 *>(spm::pausewin::pausewin_wp->entries) + id * WINDOW_SIZE;
    u16 & flags = *reinterpret_cast<u16 *>(window);
    flags = on ? (flags | WINDOW_HIGHLIGHT) : (flags & ~WINDOW_HIGHLIGHT);
}

static const char * moveHighlight(s32 oldCell)
{
    setHighlight(oldCell, false);
    setHighlight(cursorCell(), true);
    return CURSOR_SOUND;
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, BLANK_KEY) == 0)
        return "";
    return nullptr;
}

void init()
{
    for (s32 i = 0; i < CHAPTER_COUNT; i++)
    {
        u32 * check = reinterpret_cast<u32 *>(sCellChecks[i]);
        game_mode::writeInstruction(&check[0], LI_R3_1);
        game_mode::writeInstruction(&check[1], BLR);
    }
    game_mode::writeInstruction(reinterpret_cast<u32 *>(GRID_INPUT_GATE), NOP);
    game_mode::writeBranch(reinterpret_cast<void *>(SELECT_PRESS_READ), reinterpret_cast<void *>(selectPress), true);
    game_mode::writeBranch(reinterpret_cast<void *>(TITLE_LOOKUP), reinterpret_cast<void *>(titleLookup), true);
    game_mode::writeBranch(reinterpret_cast<void *>(HOVER_MESSAGE), reinterpret_cast<void *>(hoverMessage), true);
    u32 * highlight = reinterpret_cast<u32 *>(HIGHLIGHT_START);
    game_mode::writeInstruction(&highlight[0], MR_R3_R19);
    game_mode::writeBranch(&highlight[1], reinterpret_cast<void *>(moveHighlight), true);
    game_mode::writeBranch(&highlight[2], reinterpret_cast<void *>(HIGHLIGHT_RESUME));
}

void tick()
{
    for (s32 chapter = 1; chapter <= CHAPTER_COUNT; chapter++)
    {
        for (s32 section = 1; section <= SECTION_COUNT; section++)
        {
            s32 i = (chapter - 1) * SECTION_COUNT + (section - 1);
            s32 value = reachable(chapter, section) ? SHOWN : HIDDEN;
            if (sWritten[i] == value)
                continue;
            sWritten[i] = value;
            game_mode::set<s32>(&sSectionValues[i], value);
        }
    }
}

}
