/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

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

void LogManager::SetupNoFork()
{
	if (!noforkstream)
	{
		FileWriter* fw = new FileWriter(stdout);
		noforkstream = new FileLogStream(ServerInstance->Config->forcedebug ? DEBUG : DEFAULT, fw);
	}
	else
	{
		noforkstream->ChangeLevel(ServerInstance->Config->forcedebug ? DEBUG : DEFAULT);
	}
	AddLogType("*", noforkstream, false);
}

void LogManager::OpenFileLogs()
{
	/* Re-register the nofork stream if necessary. */
	if (ServerInstance->Config->nofork)
	{
		SetupNoFork();
	}
	/* Skip rest of logfile opening if we are running -nolog. */
	if (!ServerInstance->Config->writelog)
	{
		return;
	}
	ConfigReader Conf;
	std::map<std::string, FileWriter*> logmap;
	std::map<std::string, FileWriter*>::iterator i;
	for (int index = 0;; ++index)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("log", index);
		if (!tag)
			break;
		std::string method = tag->getString("method");
		if (method != "file")
		{
			continue;
		}
		std::string type = tag->getString("type");
		std::string level = tag->getString("level");
		int loglevel = DEFAULT;
		if (level == "debug" || ServerInstance->Config->forcedebug)
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
		std::string target = Conf.ReadValue("log", "target", index);
		if ((i = logmap.find(target)) == logmap.end())
		{
			FILE* f = fopen(target.c_str(), "a");
			fw = new FileWriter(f);
			logmap.insert(std::make_pair(target, fw));
		}
		else
		{
			fw = i->second;
		}
		FileLogStream* fls = new FileLogStream(loglevel, fw);
		AddLogTypes(type, fls, true);
	}
}

void LogManager::CloseLogs()
{
	std::map<std::string, std::vector<LogStream*> >().swap(LogStreams); /* Clear it */
	std::map<LogStream*, std::vector<std::string> >().swap(GlobalLogStreams); /* Clear it */
	for (std::map<LogStream*, int>::iterator i = AllLogStreams.begin(); i != AllLogStreams.end(); ++i)
	{
		delete i->first;
	}
	std::map<LogStream*, int>().swap(AllLogStreams); /* And clear it */
}

void LogManager::AddLogTypes(const std::string &types, LogStream* l, bool autoclose)
{
	irc::spacesepstream css(types);
	std::string tok;
	std::vector<std::string> excludes;
	while (css.GetToken(tok))
	{
		if (tok.empty())
		{
			continue;
		}
		if (tok.at(0) == '-')
		{
			/* Exclude! */
			excludes.push_back(tok.substr(1));
		}
		else
		{
			AddLogType(tok, l, autoclose);
		}
	}
	// Handle doing things like: USERINPUT USEROUTPUT -USERINPUT should be the same as saying just USEROUTPUT.
	// (This is so modules could, for example, inject exclusions for logtypes they can't handle.)
	for (std::vector<std::string>::iterator i = excludes.begin(); i != excludes.end(); ++i)
	{
		if (*i == "*")
		{
			/* -* == Exclude all. Why someone would do this, I dunno. */
			DelLogStream(l);
			return;
		}
		DelLogType(*i, l);
	}
	// Now if it's registered as a global, add the exclusions there too.
	std::map<LogStream *, std::vector<std::string> >::iterator gi = GlobalLogStreams.find(l);
	if (gi != GlobalLogStreams.end())
	{
		gi->second.swap(excludes); // Swap with the vector in the hash.
	}
}

bool LogManager::AddLogType(const std::string &type, LogStream *l, bool autoclose)
{
	std::map<std::string, std::vector<LogStream *> >::iterator i = LogStreams.find(type);

	if (i != LogStreams.end())
	{
		i->second.push_back(l);
	}
	else
	{
		std::vector<LogStream *> v;
		v.push_back(l);
		LogStreams[type] = v;
	}

	if (type == "*")
	{
		GlobalLogStreams.insert(std::make_pair(l, std::vector<std::string>()));
	}

	if (autoclose)
	{
		std::map<LogStream*, int>::iterator ai = AllLogStreams.find(l);
		if (ai == AllLogStreams.end())
		{
			AllLogStreams.insert(std::make_pair(l, 1));
		}
		else
		{
			++ai->second;
		}
	}

	return true;
}

void LogManager::DelLogStream(LogStream* l)
{
	for (std::map<std::string, std::vector<LogStream*> >::iterator i = LogStreams.begin(); i != LogStreams.end(); ++i)
	{
		std::vector<LogStream*>::iterator it;
		while ((it = std::find(i->second.begin(), i->second.end(), l)) != i->second.end())
		{
			if (it == i->second.end())
				continue;
			i->second.erase(it);
		}
	}
	std::map<LogStream *, std::vector<std::string> >::iterator gi = GlobalLogStreams.find(l);
	if (gi != GlobalLogStreams.end())
	{
		GlobalLogStreams.erase(gi);
	}
	std::map<LogStream*, int>::iterator ai = AllLogStreams.begin();
	if (ai == AllLogStreams.end())
	{
		return; /* Done. */
	}
	delete ai->first;
	AllLogStreams.erase(ai);
}

bool LogManager::DelLogType(const std::string &type, LogStream *l)
{
	std::map<std::string, std::vector<LogStream *> >::iterator i = LogStreams.find(type);
	if (type == "*")
	{
		std::map<LogStream *, std::vector<std::string> >::iterator gi = GlobalLogStreams.find(l);
		if (gi != GlobalLogStreams.end()) GlobalLogStreams.erase(gi);
	}

	if (i != LogStreams.end())
	{
		std::vector<LogStream *>::iterator it = std::find(i->second.begin(), i->second.end(), l);

		if (it != i->second.end())
		{
			i->second.erase(it);
			if (i->second.size() == 0)
			{
				LogStreams.erase(i);
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	std::map<LogStream*, int>::iterator ai = AllLogStreams.find(l);
	if (ai == AllLogStreams.end())
	{
		return true;
	}

	if ((--ai->second) < 1)
	{
		AllLogStreams.erase(ai);
		delete l;
	}

	return true;
}

void LogManager::Log(const std::string &type, int loglevel, const char *fmt, ...)
{
	if (Logging)
	{
		return;
	}

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
	{
		return;
	}

	Logging = true;

	for (std::map<LogStream *, std::vector<std::string> >::iterator gi = GlobalLogStreams.begin(); gi != GlobalLogStreams.end(); ++gi)
	{
		if (std::find(gi->second.begin(), gi->second.end(), type) != gi->second.end())
		{
			continue;
		}
		gi->first->OnLog(loglevel, type, msg);
	}

	std::map<std::string, std::vector<LogStream *> >::iterator i = LogStreams.find(type);

	if (i != LogStreams.end())
	{
		for (std::vector<LogStream *>::iterator it = i->second.begin(); it != i->second.end(); ++it)
		{
			(*it)->OnLog(loglevel, type, msg);
		}
	}

	Logging = false;
}


FileWriter::FileWriter(FILE* logfile)
: log(logfile), writeops(0)
{
}

void FileWriter::HandleEvent(EventType ev, int)
{
}

void FileWriter::WriteLogLine(const std::string &line)
{
	if (log == NULL)
		return;
// XXX: For now, just return. Don't throw an exception. It'd be nice to find out if this is happening, but I'm terrified of breaking so close to final release. -- w00t
//		throw CoreException("FileWriter::WriteLogLine called with a closed logfile");

	fprintf(log,"%s",line.c_str());
	if (writeops++ % 20)
	{
		fflush(log);
	}
}

void FileWriter::Close()
{
	if (log)
	{
		fflush(log);
		fclose(log);
		log = NULL;
	}
}

FileWriter::~FileWriter()
{
	this->Close();
}
