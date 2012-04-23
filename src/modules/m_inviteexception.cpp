/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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
#include "u_listmode.h"

/* $ModDesc: Provides support for the +I channel mode */
/* $ModDep: ../../include/u_listmode.h */

/*
 * Written by Om <om@inspircd.org>, April 2005.
 * Based on m_exception, which was originally based on m_chanprotect and m_silence
 *
 * The +I channel mode takes a nick!ident@host, glob patterns allowed,
 * and if a user matches an entry on the +I list then they can join the channel,
 * ignoring if +i is set on the channel
 * Now supports CIDR and IP addresses -- Brain
 */

class InspIRCd* ServerInstance;

/** Handles channel mode +I
 */
class InviteException : public ListModeBase
{
 public:
	InviteException(InspIRCd* Instance) : ListModeBase(Instance, 'I', "End of Channel Invite Exception List", 346, 347, true) { }
};

class ModuleInviteException : public Module
{
	InviteException* ie;
public:
	ModuleInviteException(InspIRCd* Me) : Module(Me)
	{
		ie = new InviteException(ServerInstance);
		if (!ServerInstance->Modes->AddMode(ie))
			throw ModuleException("Could not add new modes!");
		ServerInstance->Modules->PublishInterface("ChannelBanList", this);

		ie->DoImplements(this);
		Implementation eventlist[] = { I_OnRequest, I_On005Numeric, I_OnCheckInvite };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" INVEX=I");
	}

	virtual int OnCheckInvite(User* user, Channel* chan)
	{
		if(chan != NULL)
		{
			modelist* list;
			chan->GetExt(ie->GetInfoKey(), list);
			if (list)
			{
				std::string mask = std::string(user->nick) + "!" + user->ident + "@" + user->GetIPString();
				for (modelist::iterator it = list->begin(); it != list->end(); it++)
				{
					if(InspIRCd::Match(user->GetFullRealHost(), it->mask) || InspIRCd::Match(user->GetFullHost(), it->mask) || (InspIRCd::MatchCIDR(mask, it->mask)))
					{
						// They match an entry on the list, so let them in.
						return 1;
					}
				}
			}
			// or if there wasn't a list, there can't be anyone on it, so we don't need to do anything.
		}

		return 0;
	}

	virtual const char* OnRequest(Request* request)
	{
		return ie->DoOnRequest(request);
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		ie->DoCleanup(target_type, item);
	}

	virtual void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		ie->DoSyncChannel(chan, proto, opaque);
	}

	virtual void OnChannelDelete(Channel* chan)
	{
		ie->DoChannelDelete(chan);
	}

	virtual void OnRehash(User* user)
	{
		ie->DoRehash();
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR | VF_COMMON, API_VERSION);
	}

	~ModuleInviteException()
	{
		ServerInstance->Modes->DelMode(ie);
		delete ie;
		ServerInstance->Modules->UnpublishInterface("ChannelBanList", this);
	}
};

MODULE_INIT(ModuleInviteException)
