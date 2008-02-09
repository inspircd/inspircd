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
 private:
	InspIRCd *ServerInstance;
	std::string type;
 public:
	LogStream(InspIRCd *Instance, const std::string &type)
	{
		this->ServerInstance = Instance;
		this->type = type;
	}

	virtual void OnLog(int loglevel, const std::string &msg);
};

class CoreExport LogManager : public classbase
{
 private:
	InspIRCd *ServerInstance;
	std::map<std::string, std::vector<LogStream *> > LogStreams;
 public:
	LogManager(InspIRCd *Instance)
	{
		ServerInstance = Instance;
	}

	bool AddLogType(const std::string &type, LogStream *l);
	bool DelLogType(const std::string &type, LogStream *l);
	void Log(const std::string &type, int loglevel, const std::string &msg);
};

#endif
