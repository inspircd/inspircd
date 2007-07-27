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

#include "inspircd.h"
#include <sstream>
#include <fstream>
#include "socketengine.h"
#include "inspircd_se_config.h"
#include "filelogger.h"

FileLogger::FileLogger(InspIRCd* Instance, FILE* logfile)
: ServerInstance(Instance), log(logfile), writeops(0)
{
	if (log)
	{
		irc::sockets::NonBlocking(fileno(log));
		SetFd(fileno(log));
		buffer.clear();
	}
}

bool FileLogger::Readable()
{
	return false;
}
    
void FileLogger::HandleEvent(EventType et, int errornum)
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
		/* Burlex: Windows assumes nonblocking on FILE* pointers anyway, and also "file" fd's aren't the same
		 * as socket fd's.
		 */
#ifndef WIN32
		int flags = fcntl(fileno(log), F_GETFL, 0);
		fcntl(fileno(log), F_SETFL, flags ^ O_NONBLOCK);
#endif
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

