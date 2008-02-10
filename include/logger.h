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

	virtual void OnLog(int loglevel, const std::string &type, const std::string &msg) { }
};

typedef std::map<FileWriter*, int> FileLogMap;

class CoreExport LogManager : public classbase
{
 private:
	bool Logging; // true when logging, avoids recursion
	InspIRCd *ServerInstance;
	std::map<std::string, std::vector<LogStream *> > LogStreams;
	std::vector<LogStream *> GlobalLogStreams; //holds all logstreams with a type of *
	FileLogMap FileLogs; /* Holds all file logs, refcounted */
 public:
	LogManager(InspIRCd *Instance)
	{
		ServerInstance = Instance;
		Logging = false;
	}

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

	void OpenSingleFile(FILE* f, const std::string& type, int loglevel);
	void OpenFileLogs();
	void CloseLogs();
	bool AddLogType(const std::string &type, LogStream *l);
	bool DelLogType(const std::string &type, LogStream *l);
	void Log(const std::string &type, int loglevel, const std::string &msg);
	void Log(const std::string &type, int loglevel, const char *fmt, ...);
};

#endif
