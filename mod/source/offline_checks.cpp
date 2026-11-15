#include <common.h>
#include <wii/os.h>

#include "location_flags.h"
#include "mailbox.h"
#include "offline_checks.h"
#include "save_data.h"

namespace mod::offline_checks {

static_assert(location_flags::OFFLINE_LOCATION_COUNT <= AP_GSW_OFFLINE_CHECKS_BYTES * 8);

// The location with this save flag (its position in the list), or -1.
static s32 locationIndex(s32 flag)
{
    for (s32 i = 0; i < location_flags::OFFLINE_LOCATION_COUNT; i++)
    {
        if (location_flags::FLAGS[i] == flag)
            return i;
    }
    return -1;
}

bool active()
{
    return mailbox::gMailbox.seedPlaced == 0;
}

bool isLocation(s32 flag)
{
    return locationIndex(flag) >= 0;
}

bool log(s32 flag)
{
    s32 index = locationIndex(flag);
    if (index < 0)
        return false;
    s32 byte = AP_GSW_OFFLINE_CHECKS + (index >> 3);
    save_data::setGswByte(byte, save_data::getGswByte(byte) | (1 << (index & 7)));
    wii::os::OSReport("[spmap] offline: location %d (flag %d) collected offline, gives nothing for now\n", index, flag);
    return true;
}

}
