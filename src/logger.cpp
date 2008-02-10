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

/* $Core: libIRCDlogger */

#include "inspircd.h"

#include "filelogger.h"

/*
 * Suggested implementation...
 *	class LogManager
 *		bool AddLogType(const std::string &type, enum loglevel, LogStream *)
 *		bool DelLogType(const std::string &type, LogStream *)
 *		Log(const std::string &type, enum loglevel, const std::string &msg)
 *		std::map<std::string, std::vector<LogStream *> > logstreams (holds a 'chain' of logstreams for each type that are all notified when a log happens)
 *
 *  class LogStream
 *		std::string type
 *      virtual void OnLog(enum loglevel, const std::string &msg)
 *
 * How it works:
 *  Modules create their own logstream types (core will create one for 'file logging' for example) and create instances of these logstream types
 *  and register interest in a certain logtype. Globbing is not here, with the exception of * - for all events.. loglevel is used to drop 
 *  events that are of no interest to a logstream.
 *
 *  When Log is called, the vector of logstreams for that type is iterated (along with the special vector for "*"), and all registered logstreams
 *  are called back ("OnLog" or whatever) to do whatever they like with the message. In the case of the core, this will write to a file.
 *  In the case of the module I plan to write (m_logtochannel or something), it will log to the channel(s) for that logstream, etc.
 *
 * NOTE: Somehow we have to let LogManager manage the non-blocking file streams and provide an interface to share them with various LogStreams,
 *       as, for example, a user may want to let 'KILL' and 'XLINE' snotices go to /home/ircd/inspircd/logs/operactions.log, or whatever. How
 *       can we accomplish this easily? I guess with a map of pre-loved logpaths, and a pointer of FILE *..
 * 
 */

void LogManager::OpenFileLogs()
{
	ConfigReader* Conf = new ConfigReader(ServerInstance);
	std::map<std::string, FileWriter*> logmap;
	std::map<std::string, FileWriter*>::iterator i;
	for (int index = 0; index < Conf->Enumerate("log"); ++index)
	{
		std::string method = Conf->ReadValue("log", "method", index);
		if (method != "file") continue;
		std::string type = Conf->ReadValue("log", "type", index);
		std::string level = Conf->ReadValue("log", "level", index);
		int loglevel = DEFAULT;
		if (level == "debug")
		{
			loglevel = DEBUG;
			ServerInstance->Config->debugging = true;
		}
		else if (level == "verbose")
		{
			loglevel = VERBOSE;
		}
		else if (level == "default")
		{
			loglevel = DEFAULT;
		}
		else if (level == "sparse")
		{
			loglevel = SPARSE;
		}
		else if (level == "none")
		{
			loglevel = NONE;
		}
		FileWriter* fw;
		std::string target = Conf->ReadValue("log", "target", index);
		if ((i = logmap.find(target)) == logmap.end())
		{
			FILE* f = fopen(target.c_str(), "a");
			fw = new FileWriter(ServerInstance, f);
			logmap.insert(std::make_pair(target, fw));
		}
		else
		{
			fw = i->second;
		}
		FileLogStream* fls = new FileLogStream(ServerInstance, loglevel, fw);
		irc::commasepstream css(type);
		std::string tok;
		while (css.GetToken(tok))
		{
			AddLogType(tok, fls);
		}
	}
}

void LogManager::CloseLogs()
{
	/*
	 * This doesn't remove logstreams from the map/vector etc, because if this is called, shit is hitting the fan
	 * and we're going down anyway - this just provides a "nice" way for logstreams to clean up. -- w
	 */
	std::map<std::string, std::vector<LogStream *> >::iterator i;

	while (LogStreams.begin() != LogStreams.end())
	{
		i = LogStreams.begin();

		while (i->second.begin() != i->second.end())
		{
			std::vector<LogStream *>::iterator it = i->second.begin();

			delete (*it);
			i->second.erase(it);
		}

		LogStreams.erase(i);
	}
	/* Now close FileLoggers, for those logstreams that neglected to properly free their stuff. */
	for (FileLogMap::iterator i = FileLogs.begin(); i != FileLogs.end(); ++i)
	{
		delete i->first;
	}
	FileLogMap().swap(FileLogs); /* Swap with empty map to clear */
}

bool LogManager::AddLogType(const std::string &type, LogStream *l)
{
	std::map<std::string, std::vector<LogStream *> >::iterator i = LogStreams.find(type);

	if (i != LogStreams.end())
		i->second.push_back(l);
	else
	{
		std::vector<LogStream *> v;
		v.push_back(l);
		LogStreams[type] = v;
	}

	if (type == "*")
		GlobalLogStreams.push_back(l);

	return true;
}

bool LogManager::DelLogType(const std::string &type, LogStream *l)
{
	std::map<std::string, std::vector<LogStream *> >::iterator i = LogStreams.find(type);
	std::vector<LogStream *>::iterator gi = GlobalLogStreams.begin();

	while (gi != GlobalLogStreams.end())
	{
		if ((*gi) == l)
		{
			GlobalLogStreams.erase(gi);
			break;
		}
	}

	if (i != LogStreams.end())
	{
		std::vector<LogStream *>::iterator it = i->second.begin();

		while (it != i->second.end())
		{
			if (*it == l)
			{
				i->second.erase(it);

				if (i->second.size() == 0)
				{
					LogStreams.erase(i);
				}

				delete l;
				return true;
			}

			it++;
		}
	}

	return false;
}

void LogManager::Log(const std::string &type, int loglevel, const char *fmt, ...)
{
	if (Logging)
		return;

	va_list a;
	static char buf[65536];

	va_start(a, fmt);
	vsnprintf(buf, 65536, fmt, a);
	va_end(a);

	this->Log(type, loglevel, std::string(buf));
}

void LogManager::Log(const std::string &type, int loglevel, const std::string &msg)
{
	if (Logging)
		return;

	Logging = true;

	std::vector<LogStream *>::iterator gi = GlobalLogStreams.begin();

	while (gi != GlobalLogStreams.end())
	{
		(*gi)->OnLog(loglevel, type, msg);
		gi++;
	}

	std::map<std::string, std::vector<LogStream *> >::iterator i = LogStreams.find(type);

	if (i != LogStreams.end())
	{
		std::vector<LogStream *>::iterator it = i->second.begin();

		while (it != i->second.end())
		{
			(*it)->OnLog(loglevel, type, msg);
			it++;
		}
	}

	Logging = false;
}


FileWriter::FileWriter(InspIRCd* Instance, FILE* logfile)
: ServerInstance(Instance), log(logfile), writeops(0)
{
	if (log)
	{
		Instance->SE->NonBlocking(fileno(log));
		SetFd(fileno(log));
		buffer.clear();
	}
}

bool FileWriter::Readable()
{
	return false;
}
    
void FileWriter::HandleEvent(EventType, int)
{
	WriteLogLine("");
	if (log)
		ServerInstance->SE->DelFd(this);
}

void FileWriter::WriteLogLine(const std::string &line)
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

void FileWriter::Close()
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

FileWriter::~FileWriter()
{
	this->Close();
}


