// C++ allocation operators backed by the game's main heap

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
