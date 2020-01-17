/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011 Adam <Adam@anope.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
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


#include "inspircd_win32wrapper.h"
#include "inspircd.h"
#include "configreader.h"
#include <string>
#include "ya_getopt.c"

CWin32Exception::CWin32Exception() : exception()
{
	dwErrorCode = GetLastError();
	if( FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)szErrorString, _countof(szErrorString), NULL) == 0 )
		sprintf_s(szErrorString, _countof(szErrorString), "Error code: %u", dwErrorCode);
	for (size_t i = 0; i < _countof(szErrorString); i++)
	{
		if ((szErrorString[i] == '\r') || (szErrorString[i] == '\n'))
			szErrorString[i] = 0;
	}
}

CWin32Exception::CWin32Exception(const CWin32Exception& other)
{
	strcpy_s(szErrorString, _countof(szErrorString), other.szErrorString);
}

const char* CWin32Exception::what() const throw()
{
	return szErrorString;
}

DWORD CWin32Exception::GetErrorCode()
{
	return dwErrorCode;
}
