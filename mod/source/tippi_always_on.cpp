#include <common.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "game_mode.h"
#include "code_patching.h"

#include "game_addresses.h"
#include "save_data.h"
#include "story_progress.h"
#include "tippi_always_on.h"

namespace mod::tippi_always_on {

// In Tippi's flags: set = she's switched off
#define TIPPI_OFF_FLAG 0x80

// The prologue: until its pillar scene (story value 9), the Tippi on screen is a look-alike
// character, not the real one
#define PROLOGUE_TIPPI_END 9

// Tippi's pointer: "cmpwi r3, 12" on the story value (no pointing below it) becomes 9
#define POINTER_CHECK_VANILLA 0x2C03000C  // cmpwi r3, 12
#define POINTER_CHECK_PATCHED (0x2C030000 | PROLOGUE_TIPPI_END)

/*
    In space (4-1 sp1_02 at least) the current Pixl entry (0x804CD034, the one 0x8011DE60 returns) has
    no Pixl in it (id 0), and vanilla keeps Tippi switched off there. The mod switches her on at every map
    change, so her pointer code (0x80244C58) ran, passed that id to a Pixl-only function (0x80153BF8,
    mario_sbr.c: "fairyId >= ITEM_ID_FAIRY_START && fairyId < ITEM_ID_FAIRY_MAX") and the game halted
    (user, 2026-11-14: froze on entering sp1_02; Dolphin's call stack). Its call for the entry (0x80244E1C)
    now gets "none" for an entry without a Pixl id, which the code already skips (the beq after it).
*/
#define POINTER_PIXL_CALL 0x80244E1C           // bl 0x8011DE60: the current Pixl entry, or 0
#define CURRENT_PIXL_ENTRY 0x8011DE60
#define PIXL_FIRST 0xDC                        // ITEM_ID_FAIRY_START
#define PIXL_END 0xE8                          // ITEM_ID_FAIRY_MAX
#define PIXL_ENTRY_ID 4                        // the entry's Pixl id (s32 at +4)

static u8 * pointerPixlEntry(s32 index)
{
    auto * real = reinterpret_cast<u8 * (*)(s32)>(CURRENT_PIXL_ENTRY);
    u8 * entry = real(index);
    if (entry == nullptr)
        return nullptr;
    s32 id = *reinterpret_cast<s32 *>(entry + PIXL_ENTRY_ID);
    return id >= PIXL_FIRST && id < PIXL_END ? entry : nullptr;
}

void init()
{
    game_mode::writeBranch(reinterpret_cast<void *>(POINTER_PIXL_CALL), reinterpret_cast<void *>(pointerPixlEntry), true);
    u32 * at = game_addresses::tippi_pointer_story_check;
    if (*at != POINTER_CHECK_VANILLA)
    {
        wii::os::OSReport("[spmap] tippi: pointer check isn't what was expected (%08x), not patched\n", *at);
        return;
    }
    game_mode::writeInstruction(at, POINTER_CHECK_PATCHED);
    wii::os::OSReport("[spmap] tippi: pointing allowed from story value %d\n", PROLOGUE_TIPPI_END);
}

// Tippi's flags (the first 2 bytes of her state).
static u16 * tippiFlags()
{
    return static_cast<u16 *>(game_addresses::guideGetWork());
}

void onMapChange(s32 context, s32 storyValue)
{
    if (context == STORY_CONTEXT_NONE || storyValue < PROLOGUE_TIPPI_END)
        return;

    // The save flag makes loading a save restore her; clearing her "off" flag covers this map
    spm::swdrv::swSet(GSWF_TIPPI_PRESENT);
    u16 * flags = tippiFlags();
    if (*flags & TIPPI_OFF_FLAG)
    {
        *flags &= ~TIPPI_OFF_FLAG;
        wii::os::OSReport("[spmap] tippi: switched back on (context %d, value %d)\n", context, storyValue);
    }
}

}
