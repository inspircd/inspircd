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

class CoreExport LogStream : public classbase
{
 protected:
	InspIRCd *ServerInstance;
 public:
	LogStream(InspIRCd *Instance)
	{
		this->ServerInstance = Instance;
	}

	virtual void OnLog(int loglevel, const std::string &type, const std::string &msg) { }
};

class CoreExport LogManager : public classbase
{
 private:
	bool Logging; // true when logging, avoids recursion
	InspIRCd *ServerInstance;
	std::map<std::string, std::vector<LogStream *> > LogStreams;
	std::vector<LogStream *> GlobalLogStreams; //holds all logstreams with a type of *
 public:
	LogManager(InspIRCd *Instance)
	{
		ServerInstance = Instance;
		Logging = false;
	}

	void CloseLogs();
	bool AddLogType(const std::string &type, LogStream *l);
	bool DelLogType(const std::string &type, LogStream *l);
	void Log(const std::string &type, int loglevel, const std::string &msg);
	void Log(const std::string &type, int loglevel, const char *fmt, ...);
};

#endif
