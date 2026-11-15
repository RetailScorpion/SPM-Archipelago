#include <common.h>
#include <msl/string.h>
#include <spm/mario.h>
#include <spm/spmario.h>

#include "tower_top.h"

namespace mod::tower_top {

#define TOWER_MAP "mac_02"
#define TOP_HEIGHT 1499.0f   // from here up, Mario is on the top
#define TOP_DEPTH -250.0f    // the top's depth (z)

void tick()
{
    if (msl::string::strcmp(spm::spmario::gp->mapName, TOWER_MAP) != 0)
        return;
    // In 3D he moves in depth freely (user, 2026-11-08: only while in 2D)
    if (spm::mario::marioCheck3d())
        return;
    wii::mtx::Vec3 & position = spm::mario::marioGetPtr()->position;
    if (position.y >= TOP_HEIGHT)
        position.z = TOP_DEPTH;
}

}
