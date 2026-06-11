/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2015, 2018, 2021-2023 Sadie Powell <sadie@sadiepowell.dev>
 *   Copyright (C) 2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011, 2014, 2019 Adam <Adam@anope.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
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

/* Normal windows (platform-specific) includes */
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#include <windows.h>
#include <ws2tcpip.h>
#include <io.h>

using ssize_t = SSIZE_T;

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

inline tm* gmtime_r(const time_t* timep, struct tm* result)
{
	if (!timep || !result)
		return nullptr;

	if (gmtime_s(result, timep) != 0)
		return nullptr;

	return result;
}

inline tm* localtime_r(const time_t* timep, struct tm* result)
{
	if (!timep || !result)
		return nullptr;

	if (localtime_s(result, timep) != 0)
		return nullptr;

	return result;
}

inline std::string GetErrorMessage(DWORD dwErrorCode)
{
	char szErrorString[1024];
	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)szErrorString, _countof(szErrorString), nullptr) == 0)
		sprintf_s(szErrorString, _countof(szErrorString), "Error code: %u", dwErrorCode);
	return szErrorString;
}
