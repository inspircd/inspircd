/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Logs snomask output to channel(s). */

class ModuleChanLog : public Module
{
 private:
	/*
	 * Multimap so people can redirect a snomask to multiple channels.
	 */
	std::multimap<char, std::string> logstreams;

 public:
	ModuleChanLog(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnRehash, I_OnSendSnotice };
		ServerInstance->Modules->Attach(eventlist, this, 2);

		OnRehash(NULL);
	}

	virtual ~ModuleChanLog()
	{
	}

	virtual void OnRehash(User *user)
	{
		ConfigReader MyConf(ServerInstance);
		std::string snomasks;
		std::string channel;

		logstreams.clear();

		for (int i = 0; i < MyConf.Enumerate("chanlog"); i++)
		{
			channel = MyConf.ReadValue("chanlog", "channel", i);
			snomasks = MyConf.ReadValue("chanlog", "snomasks", i);

			if (channel.empty() || snomasks.empty())
			{
				ServerInstance->Logs->Log("m_chanlog", DEFAULT, "Malformed chanlog tag, ignoring");
				continue;
			}

			for (std::string::const_iterator it = snomasks.begin(); it != snomasks.end(); it++)
			{
				logstreams.insert(std::make_pair(*it, channel));
				ServerInstance->Logs->Log("m_chanlog", DEFAULT, "Logging %c to %s", *it, channel.c_str());
			}
		}

	}

	virtual int OnSendSnotice(char &sno, std::string &desc, const std::string &msg)
	{
		std::multimap<char, std::string>::const_iterator it = logstreams.find(sno);
		char buf[MAXBUF];

		if (it == logstreams.end())
			return 0;

		snprintf(buf, MAXBUF, "\2%s\2: %s", desc.c_str(), msg.c_str());

		while (it != logstreams.end())
		{
			if (it->first != sno)
			{
				it++;
				continue;
			}

			Channel *c = ServerInstance->FindChan(it->second);
			if (c)
			{
				c->WriteChannelWithServ(ServerInstance->Config->ServerName, "PRIVMSG %s :%s", c->name.c_str(), buf);
				ServerInstance->PI->SendChannelPrivmsg(c, 0, buf);
			}

			it++;
		}

		return 0;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR,API_VERSION);
	}
};


MODULE_INIT(ModuleChanLog)









/*
 * This is for the "old" chanlog module which intercepted messages going to the logfile..
 * I don't consider it all that useful, and it's quite dangerous if setup incorrectly, so
 * this is defined out but left intact in case someone wants to develop it further someday.
 *
 * -- w00t (aug 23rd, 2008)
 */
#define OLD_CHANLOG 0

#if OLD_CHANLOG
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
		static bool Logging = false;

		if (loglevel < this->loglvl)
			return;

		if (Logging)
			return;

		if (c)
		{
			Logging = true; // this avoids (rare chance) loops with logging server IO on networks
			char buf[MAXBUF];
			snprintf(buf, MAXBUF, "\2%s\2: %s", type.c_str(), msg.c_str());

			c->WriteChannelWithServ(ServerInstance->Config->ServerName, "PRIVMSG %s :%s", c->name.c_str(), buf);
			ServerInstance->PI->SendChannelPrivmsg(c, 0, buf);
			Logging = false;
		}
	}
};
#endif

