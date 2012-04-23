/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

LogManager::LogManager()
{
	Logging = false;
}

LogManager::~LogManager()
{
}

void LogManager::OpenFileLogs()
{
	if (ServerInstance->Config->cmdline.forcedebug)
	{
		ServerInstance->Config->RawLog = true;
		return;
	}
	/* Skip rest of logfile opening if we are running -nolog. */
	if (!ServerInstance->Config->cmdline.writelog)
		return;
	std::map<std::string, FileWriter*> logmap;
	ConfigTagList tags = ServerInstance->Config->ConfTags("log");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string method = tag->getString("method");
		if (method != "file")
		{
			continue;
		}
		std::string type = tag->getString("type");
		std::string level = tag->getString("level");
		int loglevel = DEFAULT;
		if (level == "rawio")
		{
			loglevel = RAWIO;
			ServerInstance->Config->RawLog = true;
		}
		else if (level == "debug")
		{
			loglevel = DEBUG;
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
		std::string target = tag->getString("target");
		std::map<std::string, FileWriter*>::iterator fwi = logmap.find(target);
		if (fwi == logmap.end())
		{
			char realtarget[MAXBUF];
			time_t time = ServerInstance->Time();
			struct tm *mytime = gmtime(&time);
			strftime(realtarget, MAXBUF, target.c_str(), mytime);
			FILE* f = fopen(realtarget, "a");
			fw = new FileWriter(f);
			logmap.insert(std::make_pair(target, fw));
		}
		else
		{
			fw = fwi->second;
		}
		FileLogStream* fls = new FileLogStream(loglevel, fw);
		fls->OnLog(SPARSE, "HEADER", InspIRCd::LogHeader);
		AddLogTypes(type, fls, true);
	}
}

void LogManager::CloseLogs()
{
	if (ServerInstance->Config && ServerInstance->Config->cmdline.forcedebug)
		return;
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

FileWriter::~FileWriter()
{
	if (log)
	{
		fflush(log);
		fclose(log);
		log = NULL;
	}
}
