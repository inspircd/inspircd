/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

/* $ModDesc: Provides support for the +e channel mode */
/* $ModDep: ../../include/u_listmode.h */

/* Written by Om<om@inspircd.org>, April 2005. */
/* Rewritten to use the listmode utility by Om, December 2005 */
/* Adapted from m_exception, which was originally based on m_chanprotect and m_silence */

// The +e channel mode takes a nick!ident@host, glob patterns allowed,
// and if a user matches an entry on the +e list then they can join the channel, overriding any (+b) bans set on them
// Now supports CIDR and IP addresses -- Brain


/** Handles +e channel mode
 */
class BanException : public ListModeBase
{
 public:
	BanException(InspIRCd* Instance) : ListModeBase(Instance, 'e', "End of Channel Exception List", 348, 349, true) { }
};


class ModuleBanException : public Module
{
	BanException* be;


public:
	ModuleBanException(InspIRCd* Me) : Module(Me)
	{
		be = new BanException(ServerInstance);
		if (!ServerInstance->Modes->AddMode(be))
			throw ModuleException("Could not add new modes!");
		ServerInstance->Modules->PublishInterface("ChannelBanList", this);

		be->DoImplements(this);
		Implementation list[] = { I_OnRehash, I_OnRequest, I_On005Numeric, I_OnCheckBan, I_OnCheckExtBan, I_OnCheckStringExtBan };
		Me->Modules->Attach(list, this, 6);

	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" EXCEPTS=e");
	}

	virtual int OnCheckExtBan(User *user, Channel *chan, char type)
	{
		if (chan != NULL)
		{
			modelist *list;
			chan->GetExt(be->GetInfoKey(), list);

			if (!list)
				return 0;

			std::string mask = std::string(user->nick) + "!" + user->ident + "@" + user->GetIPString();
			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				if (it->mask[0] != type || it->mask[1] != ':')
					continue;

				std::string maskptr = it->mask.substr(2);

				if (InspIRCd::Match(user->GetFullRealHost(), maskptr) || InspIRCd::Match(user->GetFullHost(), maskptr) || (InspIRCd::MatchCIDR(mask, maskptr)))
				{
					// They match an entry on the list, so let them pass this.
					return 1;
				}
			}
		}

		return 0;
	}

	virtual int OnCheckStringExtBan(const std::string &str, Channel *chan, char type)
	{
		if (chan != NULL)
		{
			modelist *list;
			chan->GetExt(be->GetInfoKey(), list);

			if (!list)
				return 0;
			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				if (it->mask[0] != type || it->mask[1] != ':')
					continue;

				std::string maskptr = it->mask.substr(2);
				if (InspIRCd::Match(str, maskptr))
					// They match an entry on the list, so let them in.
					return 1;
			}
		}

		return 0;
	}

	virtual int OnCheckBan(User* user, Channel* chan)
	{
		if (chan != NULL)
		{
			modelist* list;
			chan->GetExt(be->GetInfoKey(), list);

			if (!list)
			{
				// No list, proceed normally
				return 0;
			}

			std::string mask = std::string(user->nick) + "!" + user->ident + "@" + user->GetIPString();
			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				if (InspIRCd::Match(user->GetFullRealHost(), it->mask) || InspIRCd::Match(user->GetFullHost(), it->mask) || (InspIRCd::MatchCIDR(mask, it->mask)))
				{
					// They match an entry on the list, so let them in.
					return 1;
				}
			}
		}
		return 0;
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		be->DoCleanup(target_type, item);
	}

	virtual void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		be->DoSyncChannel(chan, proto, opaque);
	}

	virtual void OnChannelDelete(Channel* chan)
	{
		be->DoChannelDelete(chan);
	}

	virtual void OnRehash(User* user)
	{
		be->DoRehash();
	}

	virtual const char* OnRequest(Request* request)
	{
		return be->DoOnRequest(request);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

	virtual ~ModuleBanException()
	{
		ServerInstance->Modes->DelMode(be);
		delete be;
		ServerInstance->Modules->UnpublishInterface("ChannelBanList", this);
	}
};

MODULE_INIT(ModuleBanException)
