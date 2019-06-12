/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "listmode.h"

/*
 * Written by Om <om@inspircd.org>, April 2005.
 * Based on m_exception, which was originally based on m_chanprotect and m_silence
 *
 * The +I channel mode takes a nick!ident@host, glob patterns allowed,
 * and if a user matches an entry on the +I list then they can join the channel,
 * ignoring if +i is set on the channel
 * Now supports CIDR and IP addresses -- Brain
 */

/** Handles channel mode +I
 */
class InviteException : public ListModeBase
{
 public:
	InviteException(Module* Creator)
		: ListModeBase(Creator, "invex", 'I', "End of Channel Invite Exception List", 346, 347, true)
	{
		syntax = "<mask>";
	}
};

class ModuleInviteException : public Module
{
	bool invite_bypass_key;
	InviteException ie;
public:
	ModuleInviteException() : ie(this)
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["INVEX"] = ConvToStr(ie.GetModeChar());
	}

	ModResult OnCheckInvite(User* user, Channel* chan) CXX11_OVERRIDE
	{
		ListModeBase::ModeList* list = ie.GetList(chan);
		if (list)
		{
			for (ListModeBase::ModeList::iterator it = list->begin(); it != list->end(); it++)
			{
				if (chan->CheckBan(user, it->mask))
				{
					return MOD_RES_ALLOW;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckKey(User* user, Channel* chan, const std::string& key) CXX11_OVERRIDE
	{
		if (invite_bypass_key)
			return OnCheckInvite(user, chan);
		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ie.DoRehash();
		invite_bypass_key = ServerInstance->Config->ConfValue("inviteexception")->getBool("bypasskey", true);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +I, invite exceptions", VF_VENDOR);
	}
};

MODULE_INIT(ModuleInviteException)
