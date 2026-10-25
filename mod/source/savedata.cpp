#include <common.h>
#include <spm/spmario.h>

#include "savedata.h"

namespace mod::savedata {

// Direct access: swByteGet/swByteSet treat GSW bytes as signed and assert on values over 127

u8 getGswByte(s32 id)
{
    return static_cast<u8>(spm::spmario::gp->gsw[id]);
}

void setGswByte(s32 id, u8 value)
{
    spm::spmario::gp->gsw[id] = static_cast<s8>(value);
}

}
