/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDfilelogger */

#include "inspircd.h"
#include <fstream>
#include "socketengine.h"
#include "inspircd_se_config.h"
#include "filelogger.h"

FileLogger::FileLogger(InspIRCd* Instance, FILE* logfile)
: ServerInstance(Instance), log(logfile), writeops(0)
{
	if (log)
	{
		Instance->SE->NonBlocking(fileno(log));
		SetFd(fileno(log));
		buffer.clear();
	}
}

bool FileLogger::Readable()
{
	return false;
}
    
void FileLogger::HandleEvent(EventType, int)
{
	WriteLogLine("");
	if (log)
		ServerInstance->SE->DelFd(this);
}

void FileLogger::WriteLogLine(const std::string &line)
{
	if (line.length())
		buffer.append(line);

	if (log)
	{
		int written = fprintf(log,"%s",buffer.c_str());
#ifdef WINDOWS
		buffer.clear();
#else
		if ((written >= 0) && (written < (int)buffer.length()))
		{
			buffer.erase(0, buffer.length());
			ServerInstance->SE->AddFd(this);
		}
		else if (written == -1)
		{
			if (errno == EAGAIN)
				ServerInstance->SE->AddFd(this);
		}
		else
		{
			/* Wrote the whole buffer, and no need for write callback */
			buffer.clear();
		}
#endif
		if (writeops++ % 20)
		{
			fflush(log);
		}
	}
}

void FileLogger::Close()
{
	if (log)
	{
		ServerInstance->SE->Blocking(fileno(log));

		if (buffer.size())
			fprintf(log,"%s",buffer.c_str());

#ifndef WINDOWS
		ServerInstance->SE->DelFd(this);
#endif

		fflush(log);
		fclose(log);
	}

	buffer.clear();
}

FileLogger::~FileLogger()
{
	this->Close();
}


void FileLogStream::OnLog(int loglevel, const std::string &text)
{
	static char TIMESTR[26];
	static time_t LAST = 0;

	/* sanity check, just in case */
	if (!ServerInstance->Config)
		return;

	/* If we were given -debug we output all messages, regardless of configured loglevel */
	if ((loglevel < ServerInstance->Config->LogLevel) && !ServerInstance->Config->forcedebug)
		return;

	if (ServerInstance->Time() != LAST)
	{
		time_t local = ServerInstance->Time();
		struct tm *timeinfo = localtime(&local);

		strlcpy(TIMESTR,asctime(timeinfo),26);
		TIMESTR[24] = ':';
		LAST = ServerInstance->Time();
	}

	if (ServerInstance->Config->log_file && ServerInstance->Config->writelog)
	{
		std::string out = std::string(TIMESTR) + " " + text.c_str() + "\n";
		this->f->WriteLogLine(out);
	}

	if (ServerInstance->Config->nofork)
	{
		printf("%s %s\n", TIMESTR, text.c_str());
	}
}
