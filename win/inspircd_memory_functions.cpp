// Use the global heap for this process for all allocate/free operations.
#include "inspircd_win32wrapper.h"
#include <exception.h>

void * ::operator new(size_t iSize)
{
	void* ptr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, iSize);		// zero memory for unix compatibility
	/* This is the correct behaviour according to C++ standards for out of memory,
	 * not returning null -- Brain*/
	if (!ptr)
		throw std::bad_alloc;
	else
		return ptr;
}

void ::operator delete(void * ptr)
{
	HeapFree(GetProcessHeap(), 0, ptr);
}
