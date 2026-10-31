#include <common.h>
#include <spm/spmario.h>

#include "save_data.h"

namespace mod::save_data {

// These read and write the table directly. The game's own swByteGet/swByteSet treat the bytes as
// signed (-128 to 127) and stop the game on anything above 127, which the mod's values can be.

u8 getGswByte(s32 id)
{
    return static_cast<u8>(spm::spmario::gp->gsw[id]);
}

void setGswByte(s32 id, u8 value)
{
    spm::spmario::gp->gsw[id] = static_cast<s8>(value);
}

u16 getGswU16(s32 id)
{
    return static_cast<u16>(getGswByte(id) << 8 | getGswByte(id + 1));
}

void setGswU16(s32 id, u16 value)
{
    setGswByte(id, static_cast<u8>(value >> 8));
    setGswByte(id + 1, static_cast<u8>(value));
}

}
