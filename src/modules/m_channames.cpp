/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

static CharState allowedmap;

class NewIsChannelHandler final
{
public:
	static bool Call(const std::string_view&);
};

bool NewIsChannelHandler::Call(const std::string_view& channame)
{
	if (channame.empty() || channame.length() > ServerInstance->Config->Limits.MaxChannel || !ServerInstance->Channels.IsPrefix(channame[0]))
		return false;

	for (const auto chr : channame)
	{
		if (!allowedmap[static_cast<unsigned char>(chr)])
			return false;
	}

	return true;
}

class ModuleChannelNames final
	: public Module
{
	std::function<bool(const std::string_view&)> rememberer;
	bool badchan = false;
	ChanModeReference permchannelmode;

public:
	ModuleChannelNames()
		: Module(VF_VENDOR, "Allows the server administrator to define what characters are allowed in channel names.")
		, rememberer(ServerInstance->Channels.IsChannel)
		, permchannelmode(this, "permanent")
	{
	}

	void ValidateChans()
	{
		Modes::ChangeList removepermchan;

		badchan = true;
		const ChannelMap& chans = ServerInstance->Channels.GetChans();
		for (ChannelMap::const_iterator i = chans.begin(); i != chans.end(); )
		{
			Channel* c = i->second;
			// Move iterator before we begin kicking
			++i;
			if (ServerInstance->Channels.IsChannel(c->name))
				continue; // The name of this channel is still valid

			if (c->IsModeSet(permchannelmode) && !c->GetUsers().empty())
			{
				removepermchan.clear();
				removepermchan.push_remove(*permchannelmode);
				ServerInstance->Modes.Process(ServerInstance->FakeClient, c, nullptr, removepermchan);
			}

			Channel::MemberMap& users = c->userlist;
			for (Channel::MemberMap::iterator j = users.begin(); j != users.end(); )
			{
				if (IS_LOCAL(j->first))
				{
					// KickUser invalidates the iterator
					Channel::MemberMap::iterator it = j++;
					c->KickUser(ServerInstance->FakeClient, it, "Channel name no longer valid");
				}
				else
					++j;
			}
		}
		badchan = false;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("channames");
		std::string denyToken = tag->getString("denyrange");
		std::string allowToken = tag->getString("allowrange");

		if (!denyToken.compare(0, 2, "0-"))
			denyToken[0] = '1';
		if (!allowToken.compare(0, 2, "0-"))
			allowToken[0] = '1';

		allowedmap.set();

		irc::portparser denyrange(denyToken, false);
		long denyno = -1;
		while (0 != (denyno = denyrange.GetToken()))
			allowedmap[denyno & UCHAR_MAX] = false;

		irc::portparser allowrange(allowToken, false);
		long allowno = -1;
		while (0 != (allowno = allowrange.GetToken()))
			allowedmap[allowno & UCHAR_MAX] = true;

		allowedmap[0x07] = false; // BEL
		allowedmap[0x20] = false; // ' '
		allowedmap[0x2C] = false; // ','

		ServerInstance->Channels.IsChannel = NewIsChannelHandler::Call;
		ValidateChans();
	}

	void OnUserKick(User* source, Membership* memb, const std::string& reason, CUList& except_list) override
	{
		if (badchan)
		{
			for (const auto& [user, _] : memb->chan->GetUsers())
			{
				if (user != memb->user)
					except_list.insert(user);
			}
		}
	}

	Cullable::Result Cull() override
	{
		ServerInstance->Channels.IsChannel = rememberer;
		ValidateChans();
		return Module::Cull();
	}
};

MODULE_INIT(ModuleChannelNames)
