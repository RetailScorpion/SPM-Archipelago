#include <common.h>
#include <spm/msgdrv.h>

#include "archipelago_item.h"
#include "code_patching.h"
#include "game_messages.h"
#include "options_menu.h"
#include "save_block.h"
#include "tower_doors.h"

namespace mod::game_messages {

// Runs instead of the game's text lookup: the first feature with its own text for the name wins.
static const char * (*msgSearchReal)(const char * msgName);
static const char * msgSearchHook(const char * msgName)
{
    if (const char * text = archipelago_item::message(msgName))
        return text;
    if (const char * text = save_block::message(msgName))
        return text;
    if (const char * text = options_menu::message(msgName))
        return text;
    if (const char * text = tower_doors::message(msgName))
        return text;
    return msgSearchReal(msgName);
}

void init()
{
    msgSearchReal = code_patching::hookFunction(spm::msgdrv::msgSearch, msgSearchHook);
}

}
