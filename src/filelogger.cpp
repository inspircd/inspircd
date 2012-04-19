/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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


/* $Core */

#include "inspircd.h"
#include <fstream>
#include "socketengine.h"
#include "filelogger.h"

FileLogStream::FileLogStream(int loglevel, FileWriter *fw)
	: LogStream(loglevel), f(fw)
{
	ServerInstance->Logs->AddLoggerRef(f);
}

FileLogStream::~FileLogStream()
{
	/* FileWriter is managed externally now */
	ServerInstance->Logs->DelLoggerRef(f);
}

void FileLogStream::OnLog(int loglevel, const std::string &type, const std::string &text)
{
	static char TIMESTR[26];
	static time_t LAST = 0;

	if (loglevel < this->loglvl)
	{
		return;
	}

	if (ServerInstance->Time() != LAST)
	{
		time_t local = ServerInstance->Time();
		struct tm *timeinfo = localtime(&local);

		strlcpy(TIMESTR,asctime(timeinfo),26);
		TIMESTR[24] = ':';
		LAST = ServerInstance->Time();
	}

	std::string out = std::string(TIMESTR) + " " + text.c_str() + "\n";
	this->f->WriteLogLine(out);
}
