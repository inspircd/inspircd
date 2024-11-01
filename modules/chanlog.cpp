/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2012-2014, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
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


#include "inspircd.h"
#include "clientprotocolmsg.h"

class ModuleChanLog final
	: public Module
{
	/*
	 * Multimap so people can redirect a snomask to multiple channels.
	 */
	typedef insp::flat_multimap<char, std::string> ChanLogTargets;
	ChanLogTargets logstreams;

public:
	ModuleChanLog()
		: Module(VF_VENDOR, "Allows messages sent to snomasks to be logged to a channel.")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		ChanLogTargets newlogs;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("chanlog"))
		{
			const std::string channel = tag->getString("channel");
			if (!ServerInstance->Channels.IsChannel(channel))
				throw ModuleException(this, "<chanlog:channel> must be set to a channel name, at " + tag->source.str());

			const std::string snomasks = tag->getString("snomasks");
			if (snomasks.empty())
				throw ModuleException(this, "<chanlog:snomasks> must not be empty, at " + tag->source.str());

			for (const auto snomask : snomasks)
			{
				newlogs.emplace(snomask, channel);
				ServerInstance->Logs.Normal(MODNAME, "Logging {} to {}", snomask, channel);
			}
		}
		logstreams.swap(newlogs);
	}

	ModResult OnSendSnotice(char& sno, std::string& desc, const std::string& msg) override
	{
		auto channels = insp::equal_range(logstreams, sno);
		if (channels.empty())
			return MOD_RES_PASSTHRU;

		const std::string snotice = "\002" + desc + "\002: " + msg;
		for (const auto& [_, channel] : channels)
		{
			auto* c = ServerInstance->Channels.Find(channel);
			if (c)
			{
				ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, c, snotice);
				c->Write(ServerInstance->GetRFCEvents().privmsg, privmsg);
				ServerInstance->PI->SendMessage(c, 0, snotice);
			}
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleChanLog)
