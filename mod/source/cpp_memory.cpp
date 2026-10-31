/*
    Memory for the mod's C++ code.

    C++'s "new" and "delete" (making and freeing objects in memory) need to know where memory comes
    from. A mod has no operating system to ask, so these hand the job to the game's own main memory
    pool. Standard set-up from SeekyCt's mod template; the mod only uses "new" for hook trampolines.
*/

#include <common.h>
#include <spm/memory.h>

using spm::memory::Heap;

void * operator new(size_t size)
{
    return spm::memory::__memAlloc(Heap::HEAP_MAIN, size);
}

void * operator new[](size_t size)
{
    return spm::memory::__memAlloc(Heap::HEAP_MAIN, size);
}

void operator delete(void * ptr)
{
    spm::memory::__dl__FPv(ptr);
}

void operator delete[](void * ptr)
{
    spm::memory::__dl__FPv(ptr);
}

void operator delete(void * ptr, size_t)
{
    spm::memory::__dl__FPv(ptr);
}

void operator delete[](void * ptr, size_t)
{
    spm::memory::__dl__FPv(ptr);
}
