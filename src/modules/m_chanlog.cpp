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

class ModuleChanLog : public Module
{
	/*
	 * Multimap so people can redirect a snomask to multiple channels.
	 */
	typedef insp::flat_multimap<char, std::string> ChanLogTargets;
	ChanLogTargets logstreams;

 public:
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		std::string snomasks;
		std::string channel;

		logstreams.clear();

		ConfigTagList tags = ServerInstance->Config->ConfTags("chanlog");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			channel = i->second->getString("channel");
			snomasks = i->second->getString("snomasks");

			if (channel.empty() || snomasks.empty())
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Malformed chanlog tag, ignoring");
				continue;
			}

			for (std::string::const_iterator it = snomasks.begin(); it != snomasks.end(); it++)
			{
				logstreams.insert(std::make_pair(*it, channel));
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Logging %c to %s", *it, channel.c_str());
			}
		}

	}

	ModResult OnSendSnotice(char &sno, std::string &desc, const std::string &msg) CXX11_OVERRIDE
	{
		std::pair<ChanLogTargets::const_iterator, ChanLogTargets::const_iterator> itpair = logstreams.equal_range(sno);
		if (itpair.first == itpair.second)
			return MOD_RES_PASSTHRU;

		const std::string snotice = "\2" + desc + "\2: " + msg;

		for (ChanLogTargets::const_iterator it = itpair.first; it != itpair.second; ++it)
		{
			Channel *c = ServerInstance->FindChan(it->second);
			if (c)
			{
				c->WriteChannelWithServ(ServerInstance->Config->ServerName, "PRIVMSG %s :%s", c->name.c_str(), snotice.c_str());
				ServerInstance->PI->SendMessage(c, 0, snotice);
			}
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Logs snomask output to channel(s).", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanLog)
