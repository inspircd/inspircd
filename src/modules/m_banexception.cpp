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
#include "listmode.h"

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
	BanException(Module* Creator)
		: ListModeBase(Creator, "banexception", 'e', "End of Channel Exception List", 348, 349, true)
	{
		syntax = "<mask>";
	}
};


class ModuleBanException : public Module
{
	BanException be;

 public:
	ModuleBanException() : be(this)
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXCEPTS"] = ConvToStr(be.GetModeChar());
	}

	ModResult OnExtBanCheck(User *user, Channel *chan, char type) CXX11_OVERRIDE
	{
		ListModeBase::ModeList* list = be.GetList(chan);
		if (!list)
			return MOD_RES_PASSTHRU;

		for (ListModeBase::ModeList::iterator it = list->begin(); it != list->end(); it++)
		{
			if (it->mask.length() <= 2 || it->mask[0] != type || it->mask[1] != ':')
				continue;

			if (chan->CheckBan(user, it->mask.substr(2)))
			{
				// They match an entry on the list, so let them pass this.
				return MOD_RES_ALLOW;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckChannelBan(User* user, Channel* chan) CXX11_OVERRIDE
	{
		ListModeBase::ModeList* list = be.GetList(chan);
		if (!list)
		{
			// No list, proceed normally
			return MOD_RES_PASSTHRU;
		}

		for (ListModeBase::ModeList::iterator it = list->begin(); it != list->end(); it++)
		{
			if (chan->CheckBan(user, it->mask))
			{
				// They match an entry on the list, so let them in.
				return MOD_RES_ALLOW;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		be.DoRehash();
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +e, ban exceptions", VF_VENDOR);
	}
};

MODULE_INIT(ModuleBanException)
