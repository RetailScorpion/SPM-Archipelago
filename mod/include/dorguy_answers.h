#pragma once

#include <common.h>

/*
    "Highlight Dorguy answers" (Options setting, on by default since 2026-11-14; user, 2026-11-08): the right answer of
    each of the Dorguys' quiz questions in 7-2 is shown in red.
*/

namespace mod::dorguy_answers {

// The quiz's answer list with the right one in red (game_messages), or nullptr.
const char * message(const char * msgName);

}
