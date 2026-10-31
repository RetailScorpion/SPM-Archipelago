/*
    The mod's entry points, called by the rel loader.

    The mod is built as a "REL": a relocatable module, the Wii's own format for code loaded while a
    game runs. The loader calls _prolog when it loads the mod. Before main() runs, C++ needs its
    "constructors" run (code that sets up some global objects), which the build lists between
    _ctors_start and _ctors_end. Standard set-up from SeekyCt's mod template.
*/

#include <common.h>

extern "C" {

typedef void (*PFN_voidfunc)();
__attribute__((section(".ctors"))) extern PFN_voidfunc _ctors_start[];
__attribute__((section(".ctors"))) extern PFN_voidfunc _ctors_end[];
__attribute__((section(".dtors"))) extern PFN_voidfunc _dtors_start[];
__attribute__((section(".dtors"))) extern PFN_voidfunc _dtors_end[];

void _prolog();
void _epilog();
void _unresolved();

}

namespace mod {
extern void main();
}

// Called once when the mod is loaded: runs the constructors, then the mod's main().
void _prolog()
{
    for (PFN_voidfunc * ctor = _ctors_start; ctor != _ctors_end && *ctor; ++ctor)
        (*ctor)();

    mod::main();
}

// Called if the mod were unloaded (it never is): runs the "destructors", the constructors' opposite.
void _epilog()
{
    for (PFN_voidfunc * dtor = _dtors_start; dtor != _dtors_end && *dtor; ++dtor)
        (*dtor)();
}

// Called if the mod used a game function the loader couldn't find (never, if it built correctly).
void _unresolved()
{
}
