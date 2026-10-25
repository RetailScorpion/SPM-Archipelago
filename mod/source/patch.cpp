#include <common.h>
#include <wii/os.h>

#include "patch.h"

namespace mod::patch {

void writeBranch(void * at, void * destination, bool link)
{
    u32 delta = reinterpret_cast<u32>(destination) - reinterpret_cast<u32>(at);
    *reinterpret_cast<u32 *>(at) = (link ? 0x48000001 : 0x48000000) | (delta & 0x03FFFFFC);

    wii::os::DCFlushRange(at, sizeof(u32));
    wii::os::ICInvalidateRange(at, sizeof(u32));
}

}
