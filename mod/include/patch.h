#pragma once

#include <common.h>

namespace mod::patch {

void writeBranch(void * at, void * destination, bool link = false);

/*
    Replaces the first instruction of `function` with a branch to `destination`.
    Returns a trampoline that runs the original function.
*/
template <typename Func>
Func hookFunction(Func function, Func destination)
{
    u32 * instructions = reinterpret_cast<u32 *>(function);
    u32 * trampoline = new u32[2];

    trampoline[0] = instructions[0];
    writeBranch(&trampoline[1], &instructions[1]);
    writeBranch(&instructions[0], reinterpret_cast<void *>(destination));

    return reinterpret_cast<Func>(trampoline);
}

}
