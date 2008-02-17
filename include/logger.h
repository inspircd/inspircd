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

#ifndef __LOGMANAGER_H
#define __LOGMANAGER_H

/** This class implements a nonblocking writer.
 * Most people writing an ircd give little thought to their disk
 * i/o. On a congested system, disk writes can block for long
 * periods of time (e.g. if the system is busy and/or swapping
 * a lot). If we just use a blocking fprintf() call, this could
 * block for undesirable amounts of time (half of a second through
 * to whole seconds). We DO NOT want this, so we make our logfile
 * nonblocking and hook it into the SocketEngine.
 * NB: If the operating system does not support nonblocking file
 * I/O (linux seems to, as does freebsd) this will default to
 * blocking behaviour.
 */
class CoreExport FileWriter : public EventHandler
{
 protected:
	/** The creator/owner of this object
	 */
	InspIRCd* ServerInstance;

	/** The log file (fd is inside this somewhere,
	 * we get it out with fileno())
	 */
	FILE* log;

	/** Buffer of pending log lines to be written
	 */
	std::string buffer;

	/** Number of write operations that have occured
	 */
	int writeops;

 public:
	/** The constructor takes an already opened logfile.
	 */
	FileWriter(InspIRCd* Instance, FILE* logfile);

	/** This returns false, logfiles are writeable.
	 */
	virtual bool Readable();

	/** Handle pending write events.
	 * This will flush any waiting data to disk.
	 * If any data remains after the fprintf call,
	 * another write event is scheduled to write
	 * the rest of the data when possible.
	 */
	virtual void HandleEvent(EventType et, int errornum = 0);

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
	virtual void Close();

	/** Close the log file and cancel any events.
	 * (indirectly call Close()
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
 *   LogManager handles all instances of LogStreams, LogStreams (or more likely, derived classes) are instantiated and passed to it.
 */

/** LogStream base class. Modules (and other stuff) inherit from this to decide what logging they are interested in, and what to do with it.
 */
class CoreExport LogStream : public classbase
{
 protected:
	InspIRCd *ServerInstance;
	int loglvl;
 public:
	LogStream(InspIRCd *Instance, int loglevel) : loglvl(loglevel)
	{
		this->ServerInstance = Instance;
	}

	virtual ~LogStream() { }

	/** XXX document me properly.
	 * Used for on the fly changing of loglevel.
	 */
	void ChangeLevel(int lvl) { this->loglvl = lvl; }

	/** Called when there is stuff to log for this particular logstream. The derived class may take no action with it, or do what it
	 * wants with the output, basically. loglevel and type are primarily for informational purposes (the level and type of the event triggered)
	 * and msg is, of course, the actual message to log.
	 */
	virtual void OnLog(int loglevel, const std::string &type, const std::string &msg) = 0;
};

typedef std::map<FileWriter*, int> FileLogMap;

class CoreExport LogManager : public classbase
{
 private:
	bool Logging;														// true when logging, avoids recursion
	LogStream* noforkstream;											// LogStream for nofork.
	InspIRCd *ServerInstance;
	std::map<std::string, std::vector<LogStream *> > LogStreams;
	std::map<LogStream *, int> AllLogStreams;							// holds all logstreams
	std::vector<LogStream *> GlobalLogStreams;							//holds all logstreams with a type of *
	FileLogMap FileLogs;												// Holds all file logs, refcounted
 public:
	LogManager(InspIRCd *Instance)
	{
		ServerInstance = Instance;
		Logging = false;
	}

	void SetupNoFork();

	/** XXX document me properly. */
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

	/** XXX document me properly. */
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

	/** XXX document me properly. */
	void OpenSingleFile(FILE* f, const std::string& type, int loglevel);
	
	/** XXX document me properly. */
	void OpenFileLogs();
	
	/** Gives all logstreams a chance to clear up (in destructors) while it deletes them.
	 */
	void CloseLogs();
	
	/** Registers a new logstream into the logging core, so it can be called for future events
	 * XXX document me properly.
	 */
	bool AddLogType(const std::string &type, LogStream *l, bool autoclose);
	
	/** Removes a logstream from the core. After removal, it will not recieve further events.
	 */
	void DelLogStream(LogStream* l);
	
	/** XXX document me properly. */
	bool DelLogType(const std::string &type, LogStream *l);
	
	/** Pretty self explanatory.
	 */
	void Log(const std::string &type, int loglevel, const std::string &msg);
	
	/** Duh.
	 */
	void Log(const std::string &type, int loglevel, const char *fmt, ...);
};

#endif
