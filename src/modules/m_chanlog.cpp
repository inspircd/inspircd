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

#include "inspircd.h"

class ChannelLogStream : public LogStream
{
 private:
	std::string channel;

 public:
	ChannelLogStream(InspIRCd *Instance, int loglevel, const std::string &chan) : LogStream(Instance, loglevel), channel(chan)
	{
	}

	virtual void OnLog(int loglevel, const std::string &type, const std::string &msg)
	{
		Channel *c = ServerInstance->FindChan(channel);

		if (loglevel < this->loglvl) return;

		if (c)
		{
			// So this won't work remotely. Oh well.
			c->WriteChannelWithServ(ServerInstance->Config->ServerName, "PRIVMSG %s :\2%s\2: %s", c->name, type.c_str(), msg.c_str());
		}
	}
};

/* $ModDesc: Logs output to a channel instead of / as well as a file. */

class ModuleChanLog : public Module
{
 private:
	ChannelLogStream *l;
 public:
	ModuleChanLog(InspIRCd* Me) : Module(Me)
	{
		l = new ChannelLogStream(Me, ServerInstance->Config->LogLevel, "#services");
		Me->Logs->AddLogType("*", l);
	}

	virtual ~ModuleChanLog()
	{
		delete l;
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};


MODULE_INIT(ModuleChanLog)

