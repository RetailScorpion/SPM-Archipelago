#include <common.h>
#include <wii/os.h>

#include "code_patching.h"

namespace mod::code_patching {

// PowerPC "b" (branch) instruction: opcode 18 in the top bits, the distance in the middle, and
// the lowest bit set for a call ("bl", which returns afterwards).
#define BRANCH_OPCODE 0x48000000
#define BRANCH_LINK_BIT 0x00000001
#define BRANCH_DISTANCE_MASK 0x03FFFFFC

void writeBranch(void * at, void * destination, bool link)
{
    u32 distance = reinterpret_cast<u32>(destination) - reinterpret_cast<u32>(at);
    u32 instruction = BRANCH_OPCODE | (link ? BRANCH_LINK_BIT : 0) | (distance & BRANCH_DISTANCE_MASK);
    writeInstruction(static_cast<u32 *>(at), instruction);
}

void writeInstruction(u32 * at, u32 instruction)
{
    *at = instruction;
    refreshCode(at, sizeof(u32));
}

void refreshCode(void * at, u32 size)
{
    // Push the change out of the data cache, and drop any stale copy from the instruction cache
    wii::os::DCFlushRange(at, size);
    wii::os::ICInvalidateRange(at, size);
}

}
