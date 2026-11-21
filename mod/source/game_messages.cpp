#include <common.h>
#include <spm/msgdrv.h>

#include "archipelago_item.h"
#include "code_patching.h"
#include "endgame_requirements.h"
#include "game_messages.h"
#include "options_menu.h"
#include "save_block.h"
#include "save_lock.h"
#include "starting_character.h"
#include "game_mode.h"
#include "dorguy_answers.h"
#include "connect_gate.h"
#include "pause_chapters.h"
#include "fast_travel.h"
#include "tattle_checks.h"
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
    if (const char * text = endgame_requirements::message(msgName))
        return text;
    if (const char * text = tower_doors::message(msgName))
        return text;
    if (const char * text = tattle_checks::message(msgName))
        return text;
    if (const char * text = fast_travel::message(msgName))
        return text;
    if (const char * text = pause_chapters::message(msgName))
        return text;
    if (const char * text = connect_gate::message(msgName))
        return text;
    if (const char * text = save_lock::message(msgName))
        return text;
    if (const char * text = dorguy_answers::message(msgName))
        return text;
    if (const char * text = game_mode::message(msgName))
        return text;
    if (const char * text = starting_character::message(msgName))
        return text;
    return msgSearchReal(msgName);
}

void init()
{
    msgSearchReal = code_patching::hookFunction(spm::msgdrv::msgSearch, msgSearchHook);
}

}
