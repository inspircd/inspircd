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


#ifndef INSPIRCD_WIN32WRAPPER_H
#define INSPIRCD_WIN32WRAPPER_H

/*
 * Starting with PSAPI version 2 for Windows 7 and Windows Server 2008 R2, this function is defined as K32GetProcessMemoryInfo in Psapi.h and exported
 * in Kernel32.lib and Kernel32.dll. However, you should always call this function as GetProcessMemoryInfo. To ensure correct resolution of symbols
 * for programs that will run on earlier versions ofWindows, add Psapi.lib to the TARGETLIBS macro and compile the program with PSAPI_VERSION=1.
 * 
 * We do this before anything to make sure it's done.
 */
#define PSAPI_VERSION 1

/* Do not #define min or max */
#define NOMINMAX
#undef min
#undef max

#ifndef CONFIGURE_BUILD
#include "win32service.h"
#endif

/* Define the WINDOWS macro. This means we're building on windows to the rest of the server.
   I think this is more reasonable than using WIN32, especially if we're gonna be doing 64-bit compiles */
#define WINDOWS 1
#define ENABLE_CRASHDUMPS 0

/* This defaults to 64, way too small for an ircd! */
/* CRT memory debugging */
#ifdef DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#define FD_SETSIZE 24000

typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;

/* Make builds smaller, leaner and faster */
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN

/* Not defined in windows */
#define SIGHUP 1

/* Not defined in windows, parameter to shutdown() */
#define SHUT_WR 2

/* They just have to be *different*, don't they. */
#define PATH_MAX MAX_PATH

/* Begone shitty 'safe STL' warnings */
#define _SCL_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define _AFX_SECURE_NO_WARNINGS
#define _ATL_SECURE_NO_WARNINGS

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
#define _SCL_SECURE_NO_DEPRECATE

#include <string>

/* Normal windows (platform-specific) includes */
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <algorithm>
#include <io.h>
#include <psapi.h>
#include "pipe.h"

#ifdef ENABLE_CRASHDUMPS
#include <DbgHelp.h>
#endif

/* strcasecmp is not defined on windows by default */
#define strcasecmp _stricmp

/* this standard function is nonstarard. go figure. */
#define popen _popen
#define pclose _pclose

/* Error macros need to be redirected to winsock error codes, apart from on VS2010 *SIGH* */
#if _MSC_VER < 1600
	#define ETIMEDOUT WSAETIMEDOUT
	#define ECONNREFUSED WSAECONNREFUSED
	#define EADDRINUSE WSAEADDRINUSE
	#define EINPROGRESS WSAEWOULDBLOCK
	#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#endif

/* Convert formatted (xxx.xxx.xxx.xxx) string to in_addr struct */
CoreExport int insp_inet_pton(int af, const char * src, void * dst);

/* Convert struct to formatted (xxx.xxx.xxx.xxx) string */
CoreExport const char * insp_inet_ntop(int af, const void * src, char * dst, socklen_t cnt);

/* we don't want to use windows' broken inet_pton and ntop */
#define inet_pton insp_inet_pton
#define inet_ntop insp_inet_ntop

/* Safe printf functions aren't defined in VC2003 */
#define snprintf _snprintf
#define vsnprintf _vsnprintf

/* Since when does the ISO C++ standard *remove* C functions?! */
#define mkdir(file,mode) _mkdir(file)

#define strncasecmp strnicmp

/* Unix-style sleep (argument is in seconds) */
__inline void sleep(int seconds) { Sleep(seconds * 1000); }

/* IPV4 only convert string to address struct */
CoreExport int inet_aton(const char *, struct in_addr *);

/* Unix-style get running user id */
CoreExport int geteuid();

/* Handles colors in printf */
CoreExport int printf_c(const char * format, ...);

/* getopt() wrapper */
# define no_argument            0
# define required_argument      1
# define optional_argument      2
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

/* Module Loading */
#define dlopen(path, state) (void*)LoadLibrary(path)
#define dlsym(handle, export) (void*)GetProcAddress((HMODULE)handle, export)
#define dlclose(handle) FreeLibrary((HMODULE)handle)
const char * dlerror();

/* Unix-style directory searching functions */
#define chmod(filename, mode)  

struct dirent
{
	char d_name[MAX_PATH];
};

struct DIR
{
	dirent dirent_pointer;
	HANDLE find_handle;
	WIN32_FIND_DATA find_data;
	bool first;
};

struct timespec
{
	time_t tv_sec;
	long tv_nsec;
};

CoreExport DIR * opendir(const char * path);
CoreExport dirent * readdir(DIR * handle);
CoreExport void closedir(DIR * handle);

const int CLOCK_REALTIME = 0;
CoreExport int clock_gettime(int clock, struct timespec * tv);

/* Disable these stupid warnings.. */
#pragma warning(disable:4800)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
#pragma warning(disable:4244)		// warning C4244: '=' : conversion from 'long' to 'short', possible loss of data
#pragma warning(disable:4267)		// warning C4267: 'argument' : conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable:4805)		// warning C4805: '!=' : unsafe mix of type 'char' and type 'bool' in operation
#pragma warning(disable:4311)		// warning C4311: 'type cast' : pointer truncation from 'accept_overlap *' to 'int'
#pragma warning(disable:4312)		// warning C4312: 'type cast' : conversion from 'int' to 'HANDLE' of greater size
#pragma warning(disable:4355)		// warning C4355: 'this' : used in base member initializer list
#pragma warning(disable:4996)		// warning C4996: 'std::_Traits_helper::move_s' was declared deprecated
#pragma warning(disable:4706)		// warning C4706: assignment within conditional expression
#pragma warning(disable:4201)		// mmsystem.h generates this warning

/* Mehhhh... typedefs. */

typedef unsigned char uint8_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef signed long int32_t;
typedef signed long long int64_t;
typedef signed long ssize_t;

/* Shared memory allocation functions */
void * ::operator new(size_t iSize);
void ::operator delete(void * ptr);

/* IPC Handlers */
class ValueItem;
class ServerConfig;

/* Look up the nameserver in use from the registry on windows */
CoreExport std::string FindNameServerWin();

#define DISABLE_WRITEV

/* Clear a windows console */
CoreExport void ClearConsole();

CoreExport DWORD WindowsForkStart();

CoreExport void WindowsForkKillOwner();

CoreExport void ChangeWindowsSpecificPointers();

CoreExport void FindDNS(std::string& server);

CoreExport bool initwmi();
CoreExport void donewmi();
CoreExport int getcpu();
CoreExport int random();
CoreExport void srandom(unsigned seed);
CoreExport int gettimeofday(timeval *tv, void *);

#endif

