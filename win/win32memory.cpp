/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <exception>
#include <new>

#include <windows.h>

/** On windows, all dll files and executables have their own private heap,
 * whereas on POSIX systems, shared objects loaded into an executable share
 * the executable's heap. This means that if we pass an arbitrary pointer to
 * a windows DLL which is not allocated in that dll, without some form of
 * marshalling, we get a page fault. To fix this, these overridden operators
 * new and delete use the windows HeapAlloc and HeapFree functions to claim
 * memory from the windows global heap. This makes windows 'act like' POSIX
 * when it comes to memory usage between dlls and exes.
 */

void * ::operator new(size_t iSize)
{
	void* ptr = HeapAlloc(GetProcessHeap(), 0, iSize);
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
	if (ptr)
		HeapFree(GetProcessHeap(), 0, ptr);
}

void * operator new[] (size_t iSize)
{
	void* ptr = HeapAlloc(GetProcessHeap(), 0, iSize);
	if (!ptr)
		throw std::bad_alloc();
	else
		return ptr;
}

void operator delete[] (void* ptr)
{
	if (ptr)
		HeapFree(GetProcessHeap(), 0, ptr);
}
