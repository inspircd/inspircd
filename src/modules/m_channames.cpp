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

/* $ModDesc: Implements config tags which allow changing characters allowed in channel names */

static std::bitset<256> allowedmap;

class NewIsChannelHandler : public HandlerBase2<bool, const char*, size_t>
{
 public:
	NewIsChannelHandler() { }
	virtual ~NewIsChannelHandler() { }
	virtual bool Call(const char*, size_t);
};

bool NewIsChannelHandler::Call(const char* c, size_t max)
{
		/* check for no name - don't check for !*chname, as if it is empty, it won't be '#'! */
		if (!c || *c++ != '#')
			return false;

		while (*c && --max)
		{
			unsigned int i = *c++ & 0xFF;
			if (!allowedmap[i])
				return false;
		}
		// a name of exactly max length will have max = 1 here; the null does not trigger --max
		return max;
}

class ModuleChannelNames : public Module
{
 private:
	NewIsChannelHandler myhandler;
	caller2<bool, const char*, size_t> rememberer;
	bool badchan;

 public:
	ModuleChannelNames() : rememberer(ServerInstance->IsChannel)
	{
		ServerInstance->IsChannel = &myhandler;
		badchan = false;
		Implementation eventlist[] = { I_OnRehash, I_OnUserKick };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		OnRehash(NULL);
	}

	void ValidateChans()
	{
		badchan = true;
		std::vector<Channel*> chanvec;
		for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); ++i)
		{
			if (!ServerInstance->IsChannel(i->second->name.c_str(), MAXBUF))
				chanvec.push_back(i->second);
		}
		std::vector<Channel*>::reverse_iterator c2 = chanvec.rbegin();
		while (c2 != chanvec.rend())
		{
			Channel* c = *c2++;
			if (c->IsModeSet('P') && c->GetUserCounter())
			{
				std::vector<std::string> modes;
				modes.push_back(c->name);
				modes.push_back("-P");

				ServerInstance->SendGlobalMode(modes, ServerInstance->FakeClient);
			}
			const UserMembList* users = c->GetUsers();
			for(UserMembCIter j = users->begin(); j != users->end(); ++j)
				if (IS_LOCAL(j->first))
					c->KickUser(ServerInstance->FakeClient, j->first, "Channel name no longer valid");
		}
		badchan = false;
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf;
		std::string denyToken = Conf.ReadValue("channames", "denyrange", 0);
		std::string allowToken = Conf.ReadValue("channames", "allowrange", 0);
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

	virtual void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& except_list)
	{
		if (badchan)
		{
			const UserMembList* users = memb->chan->GetUsers();
			for(UserMembCIter i = users->begin(); i != users->end(); i++)
				if (i->first != memb->user)
					except_list.insert(i->first);
		}
	}

	virtual ~ModuleChannelNames()
	{
		ServerInstance->IsChannel = rememberer;
		ValidateChans();
	}

	virtual Version GetVersion()
	{
		return Version("Implements config tags which allow changing characters allowed in channel names", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChannelNames)
