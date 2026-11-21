#include <common.h>
#include <evt_cmd.h>
#include <msl/string.h>
#include <spm/evt_mario.h>
#include <spm/evt_msg.h>
#include <spm/evtmgr.h>
#include <spm/mario.h>
#include <spm/mario_pouch.h>
#include <spm/spmario.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "mailbox.h"
#include "save_data.h"
#include "starting_character.h"

namespace mod::starting_character {

#define CHARACTER_ITEM_BASE 216  // Mario; Peach, Bowser and Luigi follow
#define OPENING_DONE 11          // the story value once the opening is over (Flipside Tower)

static s32 startingCharacter()
{
    return (mailbox::gMailbox.seedFlags >> SEED_START_CHARACTER_SHIFT) & SEED_START_CHARACTER_MASK;
}

bool marioHeld()
{
    return startingCharacter() == 0 || spm::swdrv::swGet(AP_GSWF_MARIO_HELD);
}

// Takes Mario out of the party (his slot emptied), so the character menu doesn't show him: it lists
// only slots with a character in them (0x8019522c). pouchAddItem puts him back in the first empty slot.
static void removeMario()
{
    for (auto & info : spm::mario_pouch::pouchGetPtr()->characters)
        if (info.itemType == CHARACTER_ITEM_BASE)
        {
            info.itemType = 0;
            info.selectable = false;
            info.selected = false;
            wii::os::OSReport("[spmap] start: Mario out of the party until his item\n");
        }
}

// "Mario has joined your party!": the game shows nothing when Mario arrives (he's always there in
// vanilla), so this is the game's own join message (Peach's, mac_1_100) with his name
#define MARIO_JOINED "ap_mario_joined"

EVT_BEGIN(ap_mario_joined_evt)
    USER_FUNC(spm::evt_mario::evt_mario_key_off, 0)
    USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(MARIO_JOINED), 0, 0)
    USER_FUNC(spm::evt_mario::evt_mario_key_on)
    RETURN()
EVT_END()

void giveMario()
{
    spm::swdrv::swSet(AP_GSWF_MARIO_HELD);
    spm::mario_pouch::pouchAddItem(CHARACTER_ITEM_BASE);  // back in the party, selectable
    spm::evtmgr::evtEntry(ap_mario_joined_evt, 0, 0);
    wii::os::OSReport("[spmap] start: Mario received\n");
}

void onMarioAdded()
{
    // Only once the seed's starting character is in place: a new file adds Mario too, before that
    if (startingCharacter() == 0 || !spm::swdrv::swGet(AP_GSWF_START_CHARACTER_SET)
        || spm::swdrv::swGet(AP_GSWF_MARIO_HELD))
        return;
    spm::swdrv::swSet(AP_GSWF_MARIO_HELD);
    wii::os::OSReport("[spmap] start: Mario joined (found in a level)\n");
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, MARIO_JOINED) == 0)
        return "<system>\n\nMario has joined your party!\n<k>\n<p>\nNow you can flip between\n2-D and 3-D!\n<k>";
    return nullptr;
}

// Whether the starting character applies now: not Mario, the seed's items are in the game (the client
// places them after the save's first map has loaded) and the opening is over.
static bool due()
{
    return startingCharacter() != 0 && mailbox::gMailbox.seedPlaced
        && spm::spmario::gp->gsw0 >= OPENING_DONE;
}

void onMapChange()
{
    if (due() && !spm::swdrv::swGet(AP_GSWF_MARIO_HELD))
        removeMario();
}

EVT_BEGIN(ap_start_character_evt)
    USER_FUNC(spm::evt_mario::evt_mario_set_character, LW(0))
    RETURN()
EVT_END()

void tick()
{
    // Checked every frame, not just on map changes: on a fresh file the seed only arrives after the
    // first map has loaded.
    if (!due() || spm::swdrv::swGet(AP_GSWF_START_CHARACTER_SET))
        return;
    if (spm::mario::marioCtrlOffChk() || spm::mario::marioKeyOffChk())
        return;  // in a cutscene or a map change: once the player has control
    s32 start = startingCharacter();
    spm::swdrv::swSet(AP_GSWF_START_CHARACTER_SET);
    spm::mario_pouch::pouchAddItem(CHARACTER_ITEM_BASE + start);
    if (!spm::swdrv::swGet(AP_GSWF_MARIO_HELD))
        removeMario();
    spm::evtmgr::EvtEntry * evt = spm::evtmgr::evtEntry(ap_start_character_evt, 0, 0);
    evt->lw[0] = start;
    wii::os::OSReport("[spmap] start: starting as character %d\n", start);
}

}
