/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2015, 2018, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011, 2014, 2019 Adam <Adam@anope.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007, 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
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

#include "win32service.h"

/* Disable the deprecation warnings.. it spams :P */
#define _CRT_SECURE_NO_DEPRECATE
#define _WINSOCK_DEPRECATED_NO_WARNINGS

/* Normal windows (platform-specific) includes */
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#include <windows.h>
#include <ws2tcpip.h>
#include <io.h>

/* strcasecmp is not defined on windows by default */
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

typedef SSIZE_T ssize_t;

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

class CWin32Exception final
	: public std::exception
{
public:
	CWin32Exception();
	CWin32Exception(const CWin32Exception& other);
	virtual const char* what() const noexcept;
	DWORD GetErrorCode();

private:
	std::string szErrorString;
	DWORD dwErrorCode;
};

// POSIX iovec
struct iovec final
{
	void* iov_base; // Starting address
	size_t iov_len; // Number of bytes to transfer
};

// Windows WSABUF with POSIX field names
struct WindowsIOVec final
{
	// POSIX iovec has iov_base then iov_len, WSABUF in Windows has the fields in reverse order
	u_long iov_len; // Number of bytes to transfer
	char FAR* iov_base; // Starting address
};

inline ssize_t writev(int fd, const WindowsIOVec* iov, int count)
{
	DWORD sent;
	int ret = WSASend(fd, reinterpret_cast<LPWSABUF>(const_cast<WindowsIOVec*>(iov)), count, &sent, 0, nullptr, nullptr);
	if (ret == 0)
		return sent;
	return -1;
}

inline std::string GetErrorMessage(DWORD dwErrorCode)
{
	char szErrorString[1024];
	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)szErrorString, _countof(szErrorString), nullptr) == 0)
		sprintf_s(szErrorString, _countof(szErrorString), "Error code: %u", dwErrorCode);
	return szErrorString;
}
