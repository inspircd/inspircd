/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2015, 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011, 2014, 2019 Adam <Adam@anope.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 burlex <burlex@e03df62e-2008-0410-955e-edbf42e46eb7>
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


#pragma once

/* Windows Port
   Wrapper Functions/Definitions
   By Burlex */
/*
 * Starting with PSAPI version 2 for Windows 7 and Windows Server 2008 R2, this function is defined as K32GetProcessMemoryInfo in Psapi.h and exported
 * in Kernel32.lib and Kernel32.dll. However, you should always call this function as GetProcessMemoryInfo. To ensure correct resolution of symbols
 * for programs that will run on earlier versions of Windows, add Psapi.lib to the TARGETLIBS macro and compile the program with PSAPI_VERSION=1.
 *
 * We do this before anything to make sure it's done.
 */
#define PSAPI_VERSION 1

#include "win32service.h"

/* This defaults to 64, way too small for an ircd! */

#define FD_SETSIZE 24000

/* Make builds smaller, leaner and faster */
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN

/* Macros for exporting symbols - dependent on what is being compiled */

#ifdef DLL_BUILD
#define CoreExport __declspec(dllimport)
#define DllExport __declspec(dllexport)
#else
#define CoreExport __declspec(dllexport)
#define DllExport __declspec(dllimport)
#endif

/* Redirect main() through a different method in win32service.cpp, to intercept service startup */
#define ENTRYPOINT CoreExport int smain(int argc, char** argv)

/* Disable the deprecation warnings.. it spams :P */
#define _CRT_SECURE_NO_DEPRECATE
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// Windows doesn't support getopt_long so we use ya_getopt instead.
#include "ya_getopt.h"

/* Normal windows (platform-specific) includes */
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#include <windows.h>
#include <ws2tcpip.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <process.h>
#include <io.h>

#define F_OK            0       /* test for existence of file */
#define X_OK            (1<<0)  /* test for execute or search permission */
#define W_OK            (1<<1)  /* test for write permission */
#define R_OK            (1<<2)  /* test for read permission */

// Windows defines these already.
#undef ERROR
#undef min
#undef max

/* strcasecmp is not defined on windows by default */
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

typedef SSIZE_T ssize_t;

/* _popen, _pclose */
#define popen _popen
#define pclose _pclose
#define getpid _getpid

// warning: 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
// Normally, this is a huge problem, but due to our new/delete remap, we can ignore it.
#pragma warning(disable:4251)

// warning: DLL-interface classkey 'identifier' used as base for DLL-interface classkey 'identifier'
#pragma warning(disable:4275)

// warning: unreferenced formal parameter
// Unimportant for now, but for the next version, we should take a look at these again.
#pragma warning(disable:4100)

// warning: 'class' : assignment operator could not be generated
#pragma warning(disable:4512)

// warning C4127: conditional expression is constant
// This will be triggered like crazy because FOREACH_MOD and similar macros are wrapped in do { ... } while(0) constructs
#pragma warning(disable:4127)

// warning C4996: The POSIX name for this item is deprecated.
#pragma warning(disable:4996)

// warning C4244: conversion from 'x' to 'y', possible loss of data
#pragma warning(disable:4244)

// warning C4267: 'var' : conversion from 'size_t' to 'type', possible loss of data
#pragma warning(disable:4267)

// warning C4706: assignment within conditional expression
#pragma warning(disable:4706)

// warning C4800: 'type' : forcing value to bool 'true' or 'false' (performance warning)
#pragma warning(disable:4800)

/* Shared memory allocation functions */
void * ::operator new(size_t iSize);
void ::operator delete(void * ptr);

#include <exception>

class CWin32Exception : public std::exception
{
public:
	CWin32Exception();
	CWin32Exception(const CWin32Exception& other);
	virtual const char* what() const throw();
	DWORD GetErrorCode();

private:
	char szErrorString[500];
	DWORD dwErrorCode;
};

// Same value as EXIT_STATUS_FORK (EXIT_STATUS_FORK is unused on Windows)
#define EXIT_STATUS_SERVICE 4

// POSIX iovec
struct iovec
{
	void* iov_base; // Starting address
	size_t iov_len; // Number of bytes to transfer
};

// Windows WSABUF with POSIX field names
struct WindowsIOVec
{
	// POSIX iovec has iov_base then iov_len, WSABUF in Windows has the fields in reverse order
	u_long iov_len; // Number of bytes to transfer
	char FAR* iov_base; // Starting address
};

inline ssize_t writev(int fd, const WindowsIOVec* iov, int count)
{
	DWORD sent;
	int ret = WSASend(fd, reinterpret_cast<LPWSABUF>(const_cast<WindowsIOVec*>(iov)), count, &sent, 0, NULL, NULL);
	if (ret == 0)
		return sent;
	return -1;
}

// This wrapper is just so we don't need to do #ifdef _WIN32 everywhere in the socket code. It is
// not actually used and does not need to be the same size as sockaddr_un on UNIX systems.
struct sockaddr_un
{
	ADDRESS_FAMILY sun_family;
	char sun_path[6];
};
