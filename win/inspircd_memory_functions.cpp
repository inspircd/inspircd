// Use the global heap for this process for all allocate/free operations.
#include "inspircd_win32wrapper.h"

void * ::operator new(size_t iSize)
{
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, iSize);		// zero memory for unix compatibility
}

void ::operator delete(void * ptr)
{
	HeapFree(GetProcessHeap(), 0, ptr);
}
