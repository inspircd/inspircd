/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
#include <fstream>
#include "socketengine.h"
#include "inspircd_se_config.h"
#include "filelogger.h"

FileLogStream::FileLogStream(InspIRCd *Instance, int loglevel, FileWriter *fw)
	: LogStream(Instance, loglevel), f(fw)
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

	/* sanity check, just in case */
	if (!ServerInstance->Config)
		return;

	/* If we were given -debug we output all messages, regardless of configured loglevel */
	if ((loglevel < this->loglvl) && !ServerInstance->Config->forcedebug)
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
