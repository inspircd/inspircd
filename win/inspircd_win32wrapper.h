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

/* Windows Port
   Wrapper Functions/Definitions
   By Burlex */

#ifndef INSPIRCD_WIN32WRAPPER_H
#define INSPIRCD_WIN32WRAPPER_H

/* Define the WINDOWS macro. This means we're building on windows to the rest of the server.
   I think this is more reasonable than using WIN32, especially if we're gonna be doing 64-bit compiles */
#define WINDOWS 1

/* Make builds smaller, leaner and faster */
#define VC_EXTRALEAN

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

/* Disable the deprecation warnings.. it spams :P */
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_DEPRECATE

#include <string>

/* Say we're building on windows 2000. Anyone running something older than this
 * reeeeeeeally needs to upgrade! */

#define _WIN32_WINNT 0x500

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

/* strcasecmp is not defined on windows by default */
#define strcasecmp _stricmp

/* Error macros need to be redirected to winsock error codes */
#define ETIMEDOUT WSAETIMEDOUT
#define ECONNREFUSED WSAECONNREFUSED
#define EADDRINUSE WSAEADDRINUSE
#define EINPROGRESS WSAEWOULDBLOCK

/* Remember file descriptors are treated differently on windows ;) */
__inline int close(int socket) { return closesocket(socket); }

/* Convert formatted (xxx.xxx.xxx.xxx) string to in_addr struct */
CoreExport int inet_pton(int af, const char * src, void * dst);

/* Convert struct to formatted (xxx.xxx.xxx.xxx) string */
CoreExport const char * inet_ntop(int af, const void * src, char * dst, socklen_t cnt);

/* Safe printf functions aren't defined in VC2003 */
#define snprintf _snprintf
#define vsnprintf _vsnprintf

/* Recursive token function doesn't exist in VC++ */
CoreExport char * strtok_r(char *_String, const char *_Control, char **_Context);

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
extern char optarg[514];
int getopt_long_only (int ___argc, char *const *___argv, const char *__shortopts, const struct option *__longopts, int *__longind);

/* Accept Handlers */
struct udp_overlap;
CoreExport int __accept_socket(SOCKET s, sockaddr * addr, int * addrlen, void * acceptevent);
CoreExport int __getsockname(SOCKET s, sockaddr * name, int * namelen, void * acceptevent);
CoreExport int __recvfrom(SOCKET s, char * buf, int len, int flags, struct sockaddr * from, int * fromlen, udp_overlap * ov);

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

CoreExport DIR * opendir(const char * path);
CoreExport dirent * readdir(DIR * handle);
CoreExport void closedir(DIR * handle);

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

/* Mehhhh... typedefs. */

typedef unsigned char uint8_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef signed long int32_t;
typedef signed long long int64_t;

/* Shared memory allocation functions */
void * ::operator new(size_t iSize);
void ::operator delete(void * ptr);

/* IPC Handlers */
class InspIRCd;

void InitIPC();
void CheckIPC(InspIRCd * Instance);
void CloseIPC();

/* Look up the nameserver in use from the registry on windows */
std::string FindNameServerWin();

/* Clear a windows console */
void ClearConsole();

#endif

