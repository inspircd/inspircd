/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd_win32wrapper.h"
#include "inspircd.h"
#include "configreader.h"
#include <string>
#include <errno.h>
#include <assert.h>
#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "comsuppwd.lib")
#pragma comment(lib, "winmm.lib")
using namespace std;

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#include <mmsystem.h>

IWbemLocator *pLoc = NULL;
IWbemServices *pSvc = NULL;

/* This MUST remain static and delcared outside the class, so that WriteProcessMemory can reference it properly */
static DWORD owner_processid = 0;


int inet_aton(const char *cp, struct in_addr *addr)
{
	unsigned long ip = inet_addr(cp);
	addr->s_addr = ip;
	return (addr->s_addr == INADDR_NONE) ? 0 : 1;
}

const char *insp_inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{

	if (af == AF_INET)
	{
		struct sockaddr_in in;
		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		memcpy(&in.sin_addr, src, sizeof(struct in_addr));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	}
	else if (af == AF_INET6)
	{
		struct sockaddr_in6 in;
		memset(&in, 0, sizeof(in));
		in.sin6_family = AF_INET6;
		memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	}
	return NULL;
}

int geteuid()
{
	return 1;
}

int insp_inet_pton(int af, const char *src, void *dst)
{
	sockaddr_in sa;
	int len = sizeof(SOCKADDR);
	int rv = WSAStringToAddress((LPSTR)src, af, NULL, (LPSOCKADDR)&sa, &len);
	if(rv >= 0)
	{
		if(WSAGetLastError() == 10022)			// Invalid Argument
			rv = 0;
		else
			rv = 1;
	}
	memcpy(dst, &sa.sin_addr, sizeof(struct in_addr));
	return rv;
}

void setcolor(int color_code)
{
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color_code);
}

DIR * opendir(const char * path)
{
	std::string search_path = string(path) + "\\*.*";
	WIN32_FIND_DATA fd;
	HANDLE f = FindFirstFile(search_path.c_str(), &fd);
	if (f != INVALID_HANDLE_VALUE)
	{
		DIR * d = new DIR;
		memcpy(&d->find_data, &fd, sizeof(WIN32_FIND_DATA));
		d->find_handle = f;
		d->first = true;
		return d;
	}
	else
	{
		return 0;
	}
}

dirent * readdir(DIR * handle)
{
	if (handle->first)
		handle->first = false;
	else
	{
		if (!FindNextFile(handle->find_handle, &handle->find_data))
			return 0;
	}

	strncpy(handle->dirent_pointer.d_name, handle->find_data.cFileName, MAX_PATH);
	return &handle->dirent_pointer;
}

void closedir(DIR * handle)
{
	FindClose(handle->find_handle);
	delete handle;
}

const char * dlerror()
{
	static char errormessage[500];
	DWORD error = GetLastError();
	SetLastError(0);
	if (error == 0)
		return 0;

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)errormessage, 500, 0);
	return errormessage;
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt)
{
    ssize_t ret;
    size_t tot = 0;
    int i;
    char *buf, *p;

    for(i = 0; i < iovcnt; ++i)
        tot += iov[i].iov_len;
    buf = (char*)malloc(tot);
    if (tot != 0 && buf == NULL) {
        errno = ENOMEM;
        return -1;
    }
    p = buf;
    for (i = 0; i < iovcnt; ++i) {
        memcpy (p, iov[i].iov_base, iov[i].iov_len);
        p += iov[i].iov_len;
    }
    ret = send((SOCKET)fd, buf, tot, 0);
    free (buf);
    return ret;
}

#define TRED FOREGROUND_RED | FOREGROUND_INTENSITY
#define TGREEN FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define TYELLOW FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY
#define TNORMAL FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE
#define TWHITE TNORMAL | FOREGROUND_INTENSITY
#define TBLUE FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY

/* Handles colors in printf */
int printf_c(const char * format, ...)
{
	// Better hope we're not multithreaded, otherwise we'll have chickens crossing the road other side to get the to :P
	static char message[MAXBUF];
	static char temp[MAXBUF];
	int color1, color2;

	/* parse arguments */
	va_list ap;
	va_start(ap, format);
	vsnprintf(message, 500, format, ap);
	va_end(ap);

	/* search for unix-style escape sequences */
	int t;
	int c = 0;
	const char * p = message;
	while (*p != 0)
	{
		if (*p == '\033')
		{
			// Escape sequence -> copy into the temp buffer, and parse the color.
			p++;
			t = 0;
			while ((*p) && (*p != 'm'))
			{
				temp[t++] = *p;
				++p;
			}

			temp[t] = 0;
			p++;

			if (*temp == '[')
			{
				if (sscanf(temp, "[%u;%u", &color1, &color2) == 2)
				{
					switch(color2)
					{
					case 32:		// Green
						SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_INTENSITY);		// Yellow
						break;

					default:		// Unknown
						// White
						SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
						break;
					}
				}
				else
				{
					switch (*(temp+1))
					{
						case '0':
							// Returning to normal colour.
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
							break;

						case '1':
							// White
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), TWHITE);
							break;

						default:
							char message[50];
							sprintf(message, "Unknown color code: %s", temp);
							MessageBox(0, message, message, MB_OK);
							break;
					}
				}
			}
		}

		putchar(*p);
		++c;
		++p;
	}

	return c;
}

int optind = 1;
char optarg[514];
int getopt_long_only(int ___argc, char *const *___argv, const char *__shortopts, const struct option *__longopts, int *__longind)
{
	// burlex todo: handle the shortops, at the moment it only works with longopts.

	if (___argc == 1 || optind == ___argc)			// No arguments (apart from filename)
		return -1;

	const char * opt = ___argv[optind];
	optind++;

	// if we're not an option, return an error.
	if (strnicmp(opt, "--", 2) != 0)
		return 1;
	else
		opt += 2;


	// parse argument list
	int i = 0;
	for (; __longopts[i].name != 0; ++i)
	{
		if (!strnicmp(__longopts[i].name, opt, strlen(__longopts[i].name)))
		{
			// woot, found a valid argument =)
			char * par = 0;
			if ((optind) != ___argc)
			{
				// grab the parameter from the next argument (if its not another argument)
				if (strnicmp(___argv[optind], "--", 2) != 0)
				{
//					optind++;		// Trash this next argument, we won't be needing it.
					par = ___argv[optind-1];
				}
			}			

			// increment the argument for next time
//			optind++;

			// determine action based on type
			if (__longopts[i].has_arg == required_argument && !par)
			{
				// parameter missing and its a required parameter option
				return 1;
			}

			// store argument in optarg
			if (par)
				strncpy(optarg, par, 514);

			if (__longopts[i].flag != 0)
			{
				// this is a variable, we have to set it if this argument is found.
				*__longopts[i].flag = 1;
				return 0;
			}
			else
			{
				if (__longopts[i].val == -1 || par == 0)
					return 1;
				
				return __longopts[i].val;
			}			
			break;
		}
	}

	// return 1 (invalid argument)
	return 1;
}

/* These three functions were created from looking at how ares does it
 * (...and they look far tidier in C++)
 */

/* Get active nameserver */
bool GetNameServer(HKEY regkey, const char *key, char* &output)
{
	DWORD size = 0;
	DWORD result = RegQueryValueEx(regkey, key, 0, NULL, NULL, &size);
	if (((result != ERROR_SUCCESS) && (result != ERROR_MORE_DATA)) || (!size))
		return false;

	output = new char[size+1];

	if ((RegQueryValueEx(regkey, key, 0, NULL, (LPBYTE)output, &size) != ERROR_SUCCESS) || (!*output))
	{
		delete output;
		return false;
	}
	return true;
}

/* Check a network interface for its nameserver */
bool GetInterface(HKEY regkey, const char *key, char* &output)
{
	char buf[39];
	DWORD size = 39;
	int idx = 0;
	HKEY top;

	while (RegEnumKeyEx(regkey, idx++, buf, &size, 0, NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS)
	{
		size = 39;
		if (RegOpenKeyEx(regkey, buf, 0, KEY_QUERY_VALUE, &top) != ERROR_SUCCESS)
			continue;
		int rc = GetNameServer(top, key, output);
		RegCloseKey(top);
		if (rc)
			return true;
	}
	return false;
}


std::string FindNameServerWin()
{
	std::string returnval = "127.0.0.1";
	HKEY top, key;
	char* dns = NULL;

	/* Lets see if the correct registry hive and tree exist */
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\Tcpip\\Parameters", 0, KEY_READ, &top) == ERROR_SUCCESS)
	{
		/* If they do, attempt to get the nameserver name */
		RegOpenKeyEx(top, "Interfaces", 0, KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS, &key);
		if ((GetNameServer(top, "NameServer", dns)) || (GetNameServer(top, "DhcpNameServer", dns))
			|| (GetInterface(key, "NameServer", dns)) || (GetInterface(key, "DhcpNameServer", dns)))
		{
			if (dns)
			{
				returnval = dns;
				delete dns;
			}
		}
		RegCloseKey(key);
		RegCloseKey(top);
	}
	return returnval;
}


void ClearConsole()
{
	COORD coordScreen = { 0, 0 };    /* here's where we'll home the cursor */
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD cCharsWritten;
	CONSOLE_SCREEN_BUFFER_INFO csbi; /* to get buffer info */ 
	DWORD dwConSize;                 /* number of character cells in the current buffer */ 

	/* get the number of character cells in the current buffer */ 

	if (GetConsoleScreenBufferInfo( hConsole, &csbi ))
	{
		dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
		/* fill the entire screen with blanks */ 
		if (FillConsoleOutputCharacter( hConsole, (TCHAR) ' ', dwConSize, coordScreen, &cCharsWritten ))
		{
			/* get the current text attribute */ 
			if (GetConsoleScreenBufferInfo( hConsole, &csbi ))
			{
				/* now set the buffer's attributes accordingly */
				if (FillConsoleOutputAttribute( hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten ))
				{
					/* put the cursor at (0, 0) */
					SetConsoleCursorPosition( hConsole, coordScreen );
				}
			}
		}
	}
	return;
}

/* Many inspircd classes contain function pointers/functors which can be changed to point at platform specific implementations
 * of code. This function repoints these pointers and functors so that calls are windows specific.
 */
void ChangeWindowsSpecificPointers()
{
	ServerInstance->Logs->Log("win32",DEBUG,"Changing to windows specific pointer and functor set");
}

DWORD WindowsForkStart()
{
        /* Windows implementation of fork() :P */
	if (owner_processid)
		return 0;

        char module[MAX_PATH];
        if(!GetModuleFileName(NULL, module, MAX_PATH))
        {
                printf("GetModuleFileName() failed.\n");
                return false;
        }

        STARTUPINFO startupinfo;
        PROCESS_INFORMATION procinfo;
        ZeroMemory(&startupinfo, sizeof(STARTUPINFO));
        ZeroMemory(&procinfo, sizeof(PROCESS_INFORMATION));

        // Fill in the startup info struct
        GetStartupInfo(&startupinfo);

        /* Default creation flags create the processes suspended */
        DWORD startupflags = CREATE_SUSPENDED;

        /* On windows 2003/XP and above, we can use the value
         * CREATE_PRESERVE_CODE_AUTHZ_LEVEL which gives more access
         * to the process which we may require on these operating systems.
         */
        OSVERSIONINFO vi;
        vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&vi);
        if ((vi.dwMajorVersion >= 5) && (vi.dwMinorVersion > 0))
                startupflags |= CREATE_PRESERVE_CODE_AUTHZ_LEVEL;

        // Launch our "forked" process.
        BOOL bSuccess = CreateProcess ( module, // Module (exe) filename
                strdup(GetCommandLine()),       // Command line (exe plus parameters from the OS)
                                                // NOTE: We cannot return the direct value of the
                                                // GetCommandLine function here, as the pointer is
                                                // passed straight to the child process, and will be
                                                // invalid once we exit as it goes out of context.
                                                // strdup() seems ok, though.
                0,                              // PROCESS_SECURITY_ATTRIBUTES
                0,                              // THREAD_SECURITY_ATTRIBUTES
                TRUE,                           // We went to inherit handles.
                startupflags,                   // Allow us full access to the process and suspend it.
                0,                              // ENVIRONMENT
                0,                              // CURRENT_DIRECTORY
                &startupinfo,                   // startup info
                &procinfo);                     // process info

        if(!bSuccess)
        {
                printf("CreateProcess() error: %s\n", dlerror());
                return false;
        }

        // Set the owner process id in the target process.
        SIZE_T written = 0;
        DWORD pid = GetCurrentProcessId();
        if(!WriteProcessMemory(procinfo.hProcess, &owner_processid, &pid, sizeof(DWORD), &written) || written != sizeof(DWORD))
        {
                printf("WriteProcessMemory() failed: %s\n", dlerror());
                return false;
        }

        // Resume the other thread (let it start)
        ResumeThread(procinfo.hThread);

        // Wait for the new process to kill us. If there is some error, the new process will end and we will end up at the next line.
        WaitForSingleObject(procinfo.hProcess, INFINITE);

        // If we hit this it means startup failed, default to 14 if this fails.
        DWORD ExitCode = 14;
        GetExitCodeProcess(procinfo.hProcess, &ExitCode);
        CloseHandle(procinfo.hThread);
        CloseHandle(procinfo.hProcess);
        return ExitCode;
}

void WindowsForkKillOwner()
{
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, owner_processid);
        if(!hProcess || !owner_processid)
        {
                printf("Could not open process id %u: %s.\n", owner_processid, dlerror());
                ServerInstance->Exit(14);
        }

        // die die die
        if(!TerminateProcess(hProcess, 0))
        {
                printf("Could not TerminateProcess(): %s\n", dlerror());
                ServerInstance->Exit(14);
        }

        CloseHandle(hProcess);
}

bool ValidateDnsServer(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if (!*(data.GetString()))
	{
		std::string nameserver;
		ServerInstance->Logs->Log("win32",DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in the registry...");
		nameserver = FindNameServerWin();
		/* Windows stacks multiple nameservers in one registry key, seperated by commas.
		 * Spotted by Cataclysm.
		 */
		if (nameserver.find(',') != std::string::npos)
			nameserver = nameserver.substr(0, nameserver.find(','));
		/* Just to be FUCKING AKWARD, windows fister... err i mean vista...
		 * seperates the nameservers with spaces instead.
		 */
		if (nameserver.find(' ') != std::string::npos)
			nameserver = nameserver.substr(0, nameserver.find(' '));
		data.Set(nameserver.c_str());
		ServerInstance->Logs->Log("win32",DEFAULT,"<dns:server> set to '%s' as first active resolver in registry.", nameserver.c_str());
	}
	return true;
}

int gettimeofday(struct timeval * tv, void * tz)
{
	if(tv == NULL)
		return -1;

	DWORD mstime = timeGetTime();
	tv->tv_sec   = time(NULL);
	tv->tv_usec  = (mstime - (tv->tv_sec * 1000)) * 1000;
	return 0;	
}

/* Initialise WMI. Microsoft have the silliest ideas about easy ways to
 * obtain the CPU percentage of a running process!
 * The whole API for this uses evil DCOM and is entirely unicode, giving
 * all results and accepting queries as wide strings.
 */
bool initwmi()
{
	HRESULT hres;

	/* Initialise COM. This can kill babies. */
	hres =  CoInitializeEx(0, COINIT_MULTITHREADED); 
	if (FAILED(hres))
		return false;

	/* COM security. This stuff kills kittens */
	hres =  CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
		RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

	if (FAILED(hres))
	{
		CoUninitialize();
		return false;
	}
    
	/* Instance to COM object */
	pLoc = NULL;
	hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
 
	if (FAILED(hres))
	{
		CoUninitialize();
		return false;
	}

	pSvc = NULL;

	/* Connect to DCOM server */
	hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    
	/* That didn't work, maybe no kittens found to kill? */
	if (FAILED(hres))
	{
		pLoc->Release();
		CoUninitialize();
		return false;
	}

	/* Don't even ASK what this does. I'm still not too sure myself. */
	hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL,
		RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

	if (FAILED(hres))
	{
		pSvc->Release();
		pLoc->Release();     
		CoUninitialize();
		return false;
	}
	return true;
}

void donewmi()
{
	pSvc->Release();
	pLoc->Release();
	CoUninitialize();
}

/* Return the CPU usage in percent of this process */
int getcpu()
{
	HRESULT hres;
	int cpu = -1;

	/* Use WQL, similar to SQL, to construct a query that lists the cpu usage and pid of all processes */
	IEnumWbemClassObject* pEnumerator = NULL;

	BSTR Language = SysAllocString(L"WQL");
	BSTR Query    = SysAllocString(L"Select PercentProcessorTime,IDProcess from Win32_PerfFormattedData_PerfProc_Process");

	hres = pSvc->ExecQuery(Language, Query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

	/* Query didn't work */
	if (!FAILED(hres))
	{
		IWbemClassObject *pclsObj = NULL;
		ULONG uReturn = 0;

		/* Iterate the query results */
		while (pEnumerator)
		{
			VARIANT vtProp;
			/* Next item */
			HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

			/* No more items left */
			if (uReturn == 0)
				break;

			/* Find process ID */
			hr = pclsObj->Get(L"IDProcess", 0, &vtProp, 0, 0);
			if (!FAILED(hr))
			{
				/* Matches our process ID? */
				if (vtProp.uintVal == GetCurrentProcessId())
				{
					VariantClear(&vtProp);
					/* Get CPU percentage for this process */
					hr = pclsObj->Get(L"PercentProcessorTime", 0, &vtProp, 0, 0);
					if (!FAILED(hr))
					{
						/* Deal with wide string ickyness. Who in their right
						 * mind puts a number in a bstrVal wide string item?!
						 */
						VariantClear(&vtProp);
						cpu = 0;
						std::wstringstream out(vtProp.bstrVal);
						out >> cpu;
						break;
					}
				}
			}
		}

		pEnumerator->Release();
		pclsObj->Release();
	}

	SysFreeString(Language);
	SysFreeString(Query);

	return cpu;
}
