/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

static std::bitset<256> allowedmap;

class NewIsChannelHandler : public HandlerBase1<bool, const std::string&>
{
 public:
	bool Call(const std::string&);
};

bool NewIsChannelHandler::Call(const std::string& channame)
{
	if (channame.empty() || channame.length() > ServerInstance->Config->Limits.ChanMax || channame[0] != '#')
		return false;

	for (std::string::const_iterator c = channame.begin(); c != channame.end(); ++c)
	{
		unsigned int i = *c & 0xFF;
		if (!allowedmap[i])
			return false;
	}

	return true;
}

class ModuleChannelNames : public Module
{
	NewIsChannelHandler myhandler;
	caller1<bool, const std::string&> rememberer;
	bool badchan;
	ChanModeReference permchannelmode;

 public:
	ModuleChannelNames()
		: rememberer(ServerInstance->IsChannel)
		, badchan(false)
		, permchannelmode(this, "permanent")
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->IsChannel = &myhandler;
	}

	void ValidateChans()
	{
		Modes::ChangeList removepermchan;

		badchan = true;
		const chan_hash& chans = ServerInstance->GetChans();
		for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); )
		{
			Channel* c = i->second;
			// Move iterator before we begin kicking
			++i;
			if (ServerInstance->IsChannel(c->name))
				continue; // The name of this channel is still valid

			if (c->IsModeSet(permchannelmode) && c->GetUserCounter())
			{
				removepermchan.clear();
				removepermchan.push_remove(*permchannelmode);
				ServerInstance->Modes->Process(ServerInstance->FakeClient, c, NULL, removepermchan);
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

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("channames");
		std::string denyToken = tag->getString("denyrange");
		std::string allowToken = tag->getString("allowrange");

		if (!denyToken.compare(0, 2, "0-"))
			denyToken[0] = '1';
		if (!allowToken.compare(0, 2, "0-"))
			allowToken[0] = '1';

		allowedmap.set();

		irc::portparser denyrange(denyToken, false);
		int denyno = -1;
		while (0 != (denyno = denyrange.GetToken()))
			allowedmap[denyno & 0xFF] = false;

		irc::portparser allowrange(allowToken, false);
		int allowno = -1;
		while (0 != (allowno = allowrange.GetToken()))
			allowedmap[allowno & 0xFF] = true;

		allowedmap[0x07] = false; // BEL
		allowedmap[0x20] = false; // ' '
		allowedmap[0x2C] = false; // ','

		ValidateChans();
	}

	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& except_list) CXX11_OVERRIDE
	{
		if (badchan)
		{
			const Channel::MemberMap& users = memb->chan->GetUsers();
			for (Channel::MemberMap::const_iterator i = users.begin(); i != users.end(); ++i)
				if (i->first != memb->user)
					except_list.insert(i->first);
		}
	}

	CullResult cull() CXX11_OVERRIDE
	{
		ServerInstance->IsChannel = rememberer;
		ValidateChans();
		return Module::cull();
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements config tags which allow changing characters allowed in channel names", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChannelNames)
