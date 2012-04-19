/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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
#include "protocol.h"

/* $ModDesc: Logs snomask output to channel(s). */

class ModuleChanLog : public Module
{
 private:
	/*
	 * Multimap so people can redirect a snomask to multiple channels.
	 */
	std::multimap<char, std::string> logstreams;

 public:
	void init()
	{
		Implementation eventlist[] = { I_OnSendSnotice };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleChanLog()
	{
	}

	void ReadConfig(ConfigReadStatus& status)
	{
		std::string snomasks;
		std::string channel;

		logstreams.clear();

		ConfigTagList tags = ServerInstance->Config->GetTags("chanlog");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			channel = i->second->getString("channel");
			snomasks = i->second->getString("snomasks");

			if (channel.empty() || snomasks.empty())
			{
				status.ReportError(i->second, "Malformed chanlog tag, ignoring", false);
				continue;
			}

			for (std::string::const_iterator it = snomasks.begin(); it != snomasks.end(); it++)
			{
				logstreams.insert(std::make_pair(*it, channel));
				ServerInstance->Logs->Log("m_chanlog", DEFAULT, "Logging %c to %s", *it, channel.c_str());
			}
		}
	}

	virtual ModResult OnSendSnotice(char &sno, std::string &desc, const std::string &msg)
	{
		std::multimap<char, std::string>::const_iterator it = logstreams.find(sno);
		char buf[MAXBUF];

		if (it == logstreams.end())
			return MOD_RES_PASSTHRU;

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
				c->WriteChannelWithServ(ServerInstance->Config->ServerName.c_str(), "PRIVMSG %s :%s", c->name.c_str(), buf);
				ServerInstance->PI->SendChannelPrivmsg(c, 0, buf);
			}

			it++;
		}

		return MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Logs snomask output to channel(s).", VF_VENDOR);
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
	ChannelLogStream(int loglevel, const std::string &chan) : LogStream(loglevel), channel(chan)
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

			c->WriteChannelWithServ(ServerInstance->Config->ServerName.c_str(), "PRIVMSG %s :%s", c->name.c_str(), buf);
			ServerInstance->PI->SendChannelPrivmsg(c, 0, buf);
			Logging = false;
		}
	}
};
#endif

