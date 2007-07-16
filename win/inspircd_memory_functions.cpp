/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd_win32wrapper.h"
#include <exception>
#include <new>
#include <new.h>

/** On windows, all dll files and executables have their own private heap,
 * whereas on POSIX systems, shared objects loaded into an executable share
 * the executable's heap. This means that if we pass an arbitrary pointer to
 * a windows DLL which is not allocated in that dll, without some form of
 * marshalling, we get a page fault. To fix this, these overrided operators
 * new and delete use the windows HeapAlloc and HeapFree functions to claim
 * memory from the windows global heap. This makes windows 'act like' POSIX
 * when it comes to memory usage between dlls and exes.
 */

void * ::operator new(size_t iSize)
{
	void* ptr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, iSize);		/* zero memory for unix compatibility */
	/* This is the correct behaviour according to C++ standards for out of memory,
	 * not returning null -- Brain
	 */
	if (!ptr)
		throw std::bad_alloc();
	else
		return ptr;
}

void ::operator delete(void * ptr)
{
	HeapFree(GetProcessHeap(), 0, ptr);
}
