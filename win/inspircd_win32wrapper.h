/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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


/* Windows Port
   Wrapper Functions/Definitions
   By Burlex */

#ifndef INSPIRCD_WIN32WRAPPER_H
#define INSPIRCD_WIN32WRAPPER_H

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

/* They just have to be *different*, don't they. */
#define PATH_MAX MAX_PATH

/* Macros for exporting symbols - dependant on what is being compiled */

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

/* Normal windows (platform-specific) includes */
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#include <windows.h>
#include <ws2tcpip.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <process.h>

/* strcasecmp is not defined on windows by default */
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

typedef int ssize_t;

/* Convert formatted (xxx.xxx.xxx.xxx) string to in_addr struct */
CoreExport int insp_inet_pton(int af, const char * src, void * dst);

/* Convert struct to formatted (xxx.xxx.xxx.xxx) string */
CoreExport const char * insp_inet_ntop(int af, const void * src, char * dst, socklen_t cnt);

/* inet_pton/ntop require at least NT 6.0 */
#define inet_pton insp_inet_pton
#define inet_ntop insp_inet_ntop

/* Safe printf functions aren't defined in VC++ releases older than v14 */
#if _MSC_VER <= 1800
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif

/* Unix-style sleep (argument is in seconds) */
__inline void sleep(int seconds) { Sleep(seconds * 1000); }

/* _popen, _pclose */
#define popen _popen
#define pclose _pclose

/* IPV4 only convert string to address struct */
__inline int inet_aton(const char *cp, struct in_addr *addr)
{ 
	addr->s_addr = inet_addr(cp);
	return (addr->s_addr == INADDR_NONE) ? 0 : 1;
};

/* getopt() wrapper */
#define no_argument            0
#define required_argument      1
#define optional_argument      2
struct option
{
	char *name;
	int has_arg;
	int *flag;
	int val;
};
extern int optind;
extern char optarg[514];
int getopt_long(int ___argc, char *const *___argv, const char *__shortopts, const struct option *__longopts, int *__longind);

struct dirent
{
	char d_name[MAX_PATH];
};

struct DIR
{
	dirent dirent_pointer;
	HANDLE find_handle;
	WIN32_FIND_DATAA find_data;
	bool first;
};

#if _MSC_VER <= 1800
struct timespec
{
	time_t tv_sec;
	long tv_nsec;
};
#endif

CoreExport DIR * opendir(const char * path);
CoreExport dirent * readdir(DIR * handle);
CoreExport void closedir(DIR * handle);

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

// warning C4355: 'this' : used in base member initializer list
// This warning is disabled by default since VC2012
#if _MSC_VER < 1700
#pragma warning(disable:4355)
#endif

/* Shared memory allocation functions */
void * ::operator new(size_t iSize);
void ::operator delete(void * ptr);

#define DISABLE_WRITEV

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

#endif

