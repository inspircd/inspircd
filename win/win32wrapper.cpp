/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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


#include "inspircd.h"

CWin32Exception::CWin32Exception()
{
	dwErrorCode = GetLastError();
	szErrorString = GetErrorMessage(dwErrorCode);

	for (size_t pos = 0; ((pos = szErrorString.find_first_of("\r\n", pos)) != std::string::npos); )
		szErrorString[pos] = ' ';
}

CWin32Exception::CWin32Exception(const CWin32Exception& other)
	: szErrorString(other.szErrorString)
{
}

const char* CWin32Exception::what() const noexcept
{
	return szErrorString.c_str();
}

DWORD CWin32Exception::GetErrorCode()
{
	return dwErrorCode;
}
