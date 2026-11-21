#include <common.h>
#include <evt_cmd.h>
#include <msl/string.h>
#include <spm/evt_mario.h>
#include <spm/evt_msg.h>
#include <spm/evtmgr.h>
#include <spm/mario.h>
#include <spm/seqdrv.h>
#include <wii/os.h>

#include "mailbox.h"
#include "save_data.h"
#include "save_lock.h"

namespace mod::save_lock {

#define WRONG_SEED_KEY "ap_wrong_seed"

// The box is up (or the game over is on its way)
static bool sKicking;

static u32 savedId()
{
    return static_cast<u32>(save_data::getGswU16(AP_GSW_SEED_ID)) << 16 | save_data::getGswU16(AP_GSW_SEED_ID + 2);
}

static void saveId(u32 id)
{
    save_data::setGswU16(AP_GSW_SEED_ID, static_cast<u16>(id >> 16));
    save_data::setGswU16(AP_GSW_SEED_ID + 2, static_cast<u16>(id));
}

static bool playerHasControl()
{
    return !spm::mario::marioCtrlOffChk() && !spm::mario::marioKeyOffChk();
}

// ap_lock_kick(): off to the game over screen, which leads back to the title
static EVT_DEFINE_USER_FUNC(ap_lock_kick)
{
    (void) evt;
    (void) isFirstCall;
    wii::os::OSReport("[spmap] savelock: leaving the file\n");
    spm::seqdrv::seqSetSeq(spm::seqdrv::SEQ_GAMEOVER, nullptr, nullptr);
    return 2;
}
static constexpr s32 ap_lock_kick_parameter_count = 0;

EVT_BEGIN(ap_wrong_seed_evt)
    USER_FUNC(spm::evt_mario::evt_mario_key_off, 0)
    USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(WRONG_SEED_KEY), 0, 0)
    USER_FUNC(ap_lock_kick)
    RETURN()
EVT_END()

bool mismatched()
{
    u32 id = mailbox::gMailbox.seedId;
    u32 saved = savedId();
    return id != 0 && saved != 0 && saved != id;
}

void tick()
{
    if (sKicking)
        return;
    u32 id = mailbox::gMailbox.seedId;
    if (id == 0)
        return;  // no client in a slot: nothing to compare with
    u32 saved = savedId();
    if (saved == 0)
    {
        saveId(id);
        wii::os::OSReport("[spmap] savelock: file claimed for seed %08x\n", id);
        return;
    }
    if (saved == id || !playerHasControl())
        return;  // a scene first: the box comes once the player would have control
    sKicking = true;
    wii::os::OSReport("[spmap] savelock: file is for seed %08x, the client is on %08x\n", saved, id);
    spm::evtmgr::evtEntry(ap_wrong_seed_evt, 0, 0);
}

void onNoSaveLoaded()
{
    sKicking = false;
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, WRONG_SEED_KEY) == 0)
        return "<system>\nThis file belongs to a\ndifferent Archipelago seed\nor slot.\n<k>";
    return nullptr;
}

}
