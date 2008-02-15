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
	std::vector<ChannelLogStream*> cls;
 public:
	ModuleChanLog(InspIRCd* Me) : Module(Me)
	{
	}

	virtual ~ModuleChanLog()
	{
		std::vector<ChannelLogStream*>::iterator i;
		while ((i = cls.begin()) != cls.end())
		{
			ServerInstance->Logs->DelLogStream(*i);
			cls.erase(i);
		}
	}

	virtual void OnReadConfig(ServerConfig* sc, ConfigReader* Conf)
	{
		/* Since the CloseLogs prior to this hook just wiped out our logstreams for us, we just need to wipe the vector. */
		std::vector<ChannelLogStream*>().swap(cls);
		int index, max = Conf->Enumerate("log");
		cls.reserve(max);
		for (index = 0; index < max; ++index)
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
			std::string target = Conf->ReadValue("log", "target", index);
			ChannelLogStream* c = new ChannelLogStream(ServerInstance, loglevel, target);
			irc::commasepstream css(type);
			std::string tok;
			while (css.GetToken(tok))
			{
				ServerInstance->Logs->AddLogType(tok, c, true);
			}
			cls.push_back(c);
		}
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};


MODULE_INIT(ModuleChanLog)

