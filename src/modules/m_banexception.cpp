/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
	BanException(Module* Creator) : ListModeBase(Creator, "banexception", 'e', "End of Channel Exception List", 348, 349, true) { }
};


class ModuleBanException : public Module
{
	BanException be;

public:
	ModuleBanException() : be(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(be);

		be.DoImplements(this);
		Implementation list[] = { I_OnRehash, I_On005Numeric, I_OnExtBanCheck, I_OnCheckChannelBan };
		ServerInstance->Modules->Attach(list, this, sizeof(list)/sizeof(Implementation));
	}

	void On005Numeric(std::string &output)
	{
		output.append(" EXCEPTS=e");
	}

	ModResult OnExtBanCheck(User *user, Channel *chan, char type)
	{
		if (chan != NULL)
		{
			modelist *list = be.extItem.get(chan);

			if (!list)
				return MOD_RES_PASSTHRU;

			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				if (it->mask.length() <= 2 || it->mask[0] != type || it->mask[1] != ':')
					continue;

				if (chan->CheckBan(user, it->mask.substr(2)))
				{
					// They match an entry on the list, so let them pass this.
					return MOD_RES_ALLOW;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckChannelBan(User* user, Channel* chan)
	{
		if (chan)
		{
			modelist *list = be.extItem.get(chan);

			if (!list)
			{
				// No list, proceed normally
				return MOD_RES_PASSTHRU;
			}

			for (modelist::iterator it = list->begin(); it != list->end(); it++)
			{
				if (chan->CheckBan(user, it->mask))
				{
					// They match an entry on the list, so let them in.
					return MOD_RES_ALLOW;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		be.DoSyncChannel(chan, proto, opaque);
	}

	void OnRehash(User* user)
	{
		be.DoRehash();
	}

	Version GetVersion()
	{
		return Version("Provides support for the +e channel mode", VF_VENDOR);
	}
};

MODULE_INIT(ModuleBanException)
