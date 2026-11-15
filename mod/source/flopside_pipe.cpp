#include <common.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "flopside_pipe.h"
#include "pixl_joins.h"
#include "starting_character.h"

namespace mod::flopside_pipe {

#define GSWF_TOWER_PIPE 534
#define PIXL_BOOMER 222
#define PIXL_FLEEP 226

void onMapChange()
{
    bool pipeOpen = pixl_joins::havePixl(PIXL_BOOMER) && pixl_joins::havePixl(PIXL_FLEEP)
                    && starting_character::marioHeld();
    if (pipeOpen == spm::swdrv::swGet(GSWF_TOWER_PIPE))
        return;
    if (pipeOpen)
        spm::swdrv::swSet(GSWF_TOWER_PIPE);
    else
        spm::swdrv::swClear(GSWF_TOWER_PIPE);
    wii::os::OSReport("[spmap] flopside pipe: %s\n", pipeOpen ? "works" : "is closed");
}

}
