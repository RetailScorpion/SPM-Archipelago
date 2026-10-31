#pragma once

#include <common.h>

/*
    Tools for changing the game's own code while it runs.

    The game's code is a list of PowerPC instructions in memory (32 bits each). The mod changes
    behaviour by overwriting some of them:
      - a "branch" instruction sends the game off to the mod's code instead
      - a "hook" puts a branch at the very start of a game function, so the mod's version runs
        whenever the game calls it; the mod can still run the original through a "trampoline"
    After writing instructions, the processor's caches must be refreshed (refreshCode), or it may
    keep running the old instructions it already loaded.
*/

namespace mod::code_patching {

// Writes a branch at `at` that jumps to `destination`. With link = true it's a call (the game comes
// back to the next instruction when the destination returns), otherwise a plain jump.
void writeBranch(void * at, void * destination, bool link = false);

// Overwrites the instruction at `at` with `instruction`.
void writeInstruction(u32 * at, u32 instruction);

// Makes the processor see instructions changed in memory (`size` bytes from `at`).
void refreshCode(void * at, u32 size);

/*
    Hooks a game function: from now on, calls to `function` run `replacement` instead.
    Returns a "trampoline" that still runs the original function, for the replacement to call.

    How: the trampoline holds the original function's first instruction, followed by a jump to its
    second one. Then the original's first instruction is overwritten with a jump to the replacement.
*/
template <typename Func>
Func hookFunction(Func function, Func replacement)
{
    u32 * instructions = reinterpret_cast<u32 *>(function);
    u32 * trampoline = new u32[2];

    trampoline[0] = instructions[0];
    writeBranch(&trampoline[1], &instructions[1]);
    writeBranch(&instructions[0], reinterpret_cast<void *>(replacement));

    return reinterpret_cast<Func>(trampoline);
}

}
