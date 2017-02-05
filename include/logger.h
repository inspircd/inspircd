/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


#pragma once

/** Levels at which messages can be logged. */
enum LogLevel
{
	LOG_RAWIO   = 5,
	LOG_DEBUG   = 10,
	LOG_VERBOSE = 20,
	LOG_DEFAULT = 30,
	LOG_SPARSE  = 40,
	LOG_NONE    = 50
};

/** Simple wrapper providing periodic flushing to a disk-backed file.
 */
class CoreExport FileWriter
{
 protected:
	/** The log file (fd is inside this somewhere,
	 * we get it out with fileno())
	 */
	FILE* log;

	/** The number of write operations after which we should flush.
	 */
	unsigned int flush;

	/** Number of write operations that have occured
	 */
	unsigned int writeops;

 public:
	/** The constructor takes an already opened logfile.
	 */
	FileWriter(FILE* logfile, unsigned int flushcount);

	/** Write one or more preformatted log lines.
	 * If the data cannot be written immediately,
	 * this class will insert itself into the
	 * SocketEngine, and register a write event,
	 * and when the write event occurs it will
	 * attempt again to write the data.
	 */
	void WriteLogLine(const std::string &line);

	/** Close the log file and cancel any events.
	 */
	virtual ~FileWriter();
};



/*
 * New world logging!
 * The brief summary:
 *  Logging used to be a simple affair, a FILE * handled by a nonblocking logging class inheriting from EventHandler, that was inserted
 *  into the socket engine, and wrote lines. If nofork was on, it was printf()'d.
 *
 *  We decided to horribly overcomplicate matters, and create vastly customisable logging. LogManager and LogStream form the visible basis
 *  of the new interface. Basically, a LogStream can be inherited to do different things with logging output. We inherit from it once in core
 *  to create a FileLogStream, that writes to a file, for example. Different LogStreams can hook different types of log messages, and different
 *  levels of output too, for extreme customisation. Multiple LogStreams can hook the same message/levels of output, meaning that e.g. output
 *  can go to a channel as well as a file.
 *
 *  HOW THIS WORKS
 *   LogManager handles all instances of LogStreams, classes derived from LogStream are instantiated and passed to it.
 */

/** LogStream base class. Modules (and other stuff) inherit from this to decide what logging they are interested in, and what to do with it.
 */
class CoreExport LogStream : public classbase
{
 protected:
	LogLevel loglvl;
 public:
	static const char LogHeader[];

	LogStream(LogLevel loglevel) : loglvl(loglevel)
	{
	}

	/* A LogStream's destructor should do whatever it needs to close any resources it was using (or indicate that it is no longer using a resource
	 * in the event that the resource is shared, see for example FileLogStream).
	 */
	virtual ~LogStream() { }

	/** Changes the loglevel for this LogStream on-the-fly.
	 * This is needed for -nofork. But other LogStreams could use it to change loglevels.
	 */
	void ChangeLevel(LogLevel lvl) { this->loglvl = lvl; }

	/** Called when there is stuff to log for this particular logstream. The derived class may take no action with it, or do what it
	 * wants with the output, basically. loglevel and type are primarily for informational purposes (the level and type of the event triggered)
	 * and msg is, of course, the actual message to log.
	 */
	virtual void OnLog(LogLevel loglevel, const std::string &type, const std::string &msg) = 0;
};

typedef std::map<FileWriter*, int> FileLogMap;

class CoreExport LogManager : public fakederef<LogManager>
{
 private:
	/** Lock variable, set to true when a log is in progress, which prevents further loggging from happening and creating a loop.
	 */
	bool Logging;

	/** Map of active log types and what LogStreams will receive them.
	 */
	std::map<std::string, std::vector<LogStream *> > LogStreams;

	/** Refcount map of all LogStreams managed by LogManager.
	 * If a logstream is not listed here, it won't be automatically closed by LogManager, even if it's loaded in one of the other lists.
	 */
	std::map<LogStream *, int> AllLogStreams;

	/** LogStreams with type * (which means everything), and a list a logtypes they are excluded from (eg for "* -USERINPUT -USEROUTPUT").
	 */
	std::map<LogStream *, std::vector<std::string> > GlobalLogStreams;

	/** Refcounted map of all FileWriters in use by FileLogStreams, for file stream sharing.
	 */
	FileLogMap FileLogs;

 public:
	LogManager();
	~LogManager();

	/** Adds a FileWriter instance to LogManager, or increments the reference count of an existing instance.
	 * Used for file-stream sharing for FileLogStreams.
	 */
	void AddLoggerRef(FileWriter* fw)
	{
		FileLogMap::iterator i = FileLogs.find(fw);
		if (i == FileLogs.end())
		{
			FileLogs.insert(std::make_pair(fw, 1));
		}
		else
		{
			++i->second;
		}
	}

	/** Indicates that a FileWriter reference has been removed. Reference count is decreased, and if zeroed, the FileWriter is closed.
	 */
	void DelLoggerRef(FileWriter* fw)
	{
		FileLogMap::iterator i = FileLogs.find(fw);
		if (i == FileLogs.end()) return; /* Maybe should log this? */
		if (--i->second < 1)
		{
			delete i->first;
			FileLogs.erase(i);
		}
	}

	/** Opens all logfiles defined in the configuration file using \<log method="file">.
	 */
	void OpenFileLogs();

	/** Removes all LogStreams, meaning they have to be readded for logging to continue.
	 * Only LogStreams that were listed in AllLogStreams are actually closed.
	 */
	void CloseLogs();

	/** Adds a single LogStream to multiple logtypes.
	 * This automatically handles things like "* -USERINPUT -USEROUTPUT" to mean all but USERINPUT and USEROUTPUT types.
	 * It is not a good idea to mix values of autoclose for the same LogStream.
	 * @param type The type string (from configuration, or whatever) to parse.
	 * @param l The LogStream to add.
	 * @param autoclose True to have the LogStream automatically closed when all references to it are removed from LogManager. False to leave it open.
	 */
	void AddLogTypes(const std::string &type, LogStream *l, bool autoclose);

	/** Registers a new logstream into the logging core, so it can be called for future events
	 * It is not a good idea to mix values of autoclose for the same LogStream.
	 * @param type The type to add this LogStream to.
	 * @param l The LogStream to add.
	 * @param autoclose True to have the LogStream automatically closed when all references to it are removed from LogManager. False to leave it open.
	 * @return True if the LogStream was added successfully, False otherwise.
	 */
	bool AddLogType(const std::string &type, LogStream *l, bool autoclose);

	/** Removes a logstream from the core. After removal, it will not recieve further events.
	 * If the LogStream was ever added with autoclose, it will be closed after this call (this means the pointer won't be valid anymore).
	 */
	void DelLogStream(LogStream* l);

	/** Removes a LogStream from a single type. If the LogStream has been registered for "*" it will still receive the type unless you remove it from "*" specifically.
	 * If the LogStream was added with autoclose set to true, then when the last occurrence of the stream is removed it will automatically be closed (freed).
	 */
	bool DelLogType(const std::string &type, LogStream *l);

	/** Logs an event, sending it to all LogStreams registered for the type.
	 * @param type Log message type (ex: "USERINPUT", "MODULE", ...)
	 * @param loglevel Log message level (LOG_DEBUG, LOG_VERBOSE, LOG_DEFAULT, LOG_SPARSE, LOG_NONE)
	 * @param msg The message to be logged (literal).
	 */
	void Log(const std::string &type, LogLevel loglevel, const std::string &msg);

	/** Logs an event, sending it to all LogStreams registered for the type.
	 * @param type Log message type (ex: "USERINPUT", "MODULE", ...)
	 * @param loglevel Log message level (LOG_DEBUG, LOG_VERBOSE, LOG_DEFAULT, LOG_SPARSE, LOG_NONE)
	 * @param fmt The format of the message to be logged. See your C manual on printf() for details.
	 */
	void Log(const std::string &type, LogLevel loglevel, const char *fmt, ...) CUSTOM_PRINTF(4, 5);
};
