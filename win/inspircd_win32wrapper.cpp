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
#include "inspircd.h"
#include <string>
#include <errno.h>
#include <assert.h>
using namespace std;

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

HANDLE hIPCPipe;

int inet_aton(const char *cp, struct in_addr *addr)
{
	unsigned long ip = inet_addr(cp);
	addr->s_addr = ip;
	return (addr->s_addr == INADDR_NONE) ? 0 : 1;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
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

int inet_pton(int af, const char *src, void *dst)
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

char * strtok_r(char *_String, const char *_Control, char **_Context)
{
	unsigned char *str;
	const unsigned char *ctl = (const unsigned char*)_Control;
	unsigned char map[32];

	if (_Context == 0 || !_Control)
		return 0;

	if (!(_String != NULL || *_Context != NULL))
		return 0;

	memset(map, 0, 32);

	do {
		map[*ctl >> 3] |= (1 << (*ctl & 7));
	} while (*ctl++);

	/* If string is NULL, set str to the saved
	* pointer (i.e., continue breaking tokens out of the string
	* from the last strtok call) */
	if (_String != NULL)
	{
		str = (unsigned char*)_String;
	}
	else
	{
		str = (unsigned char*)*_Context;
	}

	/* Find beginning of token (skip over leading delimiters). Note that
	* there is no token iff this loop sets str to point to the terminal
	* null (*str == 0) */
	while ((map[*str >> 3] & (1 << (*str & 7))) && *str != 0)
	{
		str++;
	}

	_String = (char*)str;

	/* Find the end of the token. If it is not the end of the string,
	* put a null there. */
	for ( ; *str != 0 ; str++ )
	{
		if (map[*str >> 3] & (1 << (*str & 7)))
		{
			*str++ = 0;
			break;
		}
	}

	/* Update context */
	*_Context = (char*)str;

	/* Determine if a token has been found. */
	if (_String == (char*)str)
	{
		return NULL;
	}
	else
	{
		return _String;
	}
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

int arg_counter = 1;
char optarg[514];
int getopt_long_only(int ___argc, char *const *___argv, const char *__shortopts, const struct option *__longopts, int *__longind)
{
	// burlex todo: handle the shortops, at the moment it only works with longopts.

	if (___argc == 1 || arg_counter == ___argc)			// No arguments (apart from filename)
		return -1;

	const char * opt = ___argv[arg_counter];
	int return_val = 0;

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
			if ((arg_counter + 1) != ___argc)
			{
				// grab the parameter from the next argument (if its not another argument)
				if (strnicmp(___argv[arg_counter+1], "--", 2) != 0)
				{
					arg_counter++;		// Trash this next argument, we won't be needing it.
					par = ___argv[arg_counter];
				}
			}			

			// increment the argument for next time
			arg_counter++;

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

/* IPC Messages */
#define IPC_MESSAGE_REHASH	1
#define IPC_MESSAGE_DIE		2
#define IPC_MESSAGE_RESTART	3

void InitIPC()
{
	static DWORD buflen = 1024;
	static const char * pipename = "\\\\.\\mailslot\\Inspircd";
	hIPCPipe = CreateMailslot(pipename, buflen, 0, 0);
	if (hIPCPipe == INVALID_HANDLE_VALUE)
		printf("IPC Pipe could not be created. Are you sure you didn't start InspIRCd twice?\n");
}

void CheckIPC(InspIRCd * Instance)
{
	if (hIPCPipe == INVALID_HANDLE_VALUE)
		return;

	DWORD bytes;
	DWORD action;

	BOOL res = ReadFile(hIPCPipe, &action, sizeof(DWORD), &bytes, 0);
	if (!res)
	{
		if (GetLastError() != ERROR_SEM_TIMEOUT)
			Instance->Log(DEFAULT, "IPC Pipe Error %u: %s", GetLastError(), dlerror());
		return;
	}

	switch (action)
	{
		case IPC_MESSAGE_REHASH:
			InspIRCd::Rehash(0);
		break;
		
		case IPC_MESSAGE_DIE:
			InspIRCd::Exit(0);
		break;

		case IPC_MESSAGE_RESTART:
			Instance->Restart("IPC_MESSAGE_RESTART received by mailslot.");
		break;
	}
}

void CloseIPC()
{
	CloseHandle(hIPCPipe);
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
