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

/** Handles channel mode +I
 */
class InviteException : public ListModeBase
{
 public:
	InviteException(Module* Creator) : ListModeBase(Creator, "invex", 'I', "End of Channel Invite Exception List", 346, 347, true) { }
};

class ModuleInviteException : public Module
{
	InviteException ie;
public:
	ModuleInviteException() : ie(this)
	{
		if (!ServerInstance->Modes->AddMode(&ie))
			throw ModuleException("Could not add new modes!");

		ie.DoImplements(this);
		Implementation eventlist[] = { I_On005Numeric, I_OnCheckInvite, I_OnCheckKey };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	void On005Numeric(std::string &output)
	{
		output.append(" INVEX=I");
	}

	ModResult OnCheckInvite(User* user, Channel* chan)
	{
		if(chan != NULL)
		{
			modelist* list = ie.extItem.get(chan);
			if (list)
			{
				for (modelist::iterator it = list->begin(); it != list->end(); it++)
				{
					if (chan->CheckBan(user, it->mask))
					{
						return MOD_RES_ALLOW;
					}
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckKey(User* user, Channel* chan, const std::string& key)
	{
		if (ServerInstance->Config->ConfValue("inviteexception")->getBool("bypasskey", true))
			return OnCheckInvite(user, chan);
		return MOD_RES_PASSTHRU;
	}

	void OnCleanup(int target_type, void* item)
	{
		ie.DoCleanup(target_type, item);
	}

	void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		ie.DoSyncChannel(chan, proto, opaque);
	}

	void OnRehash(User* user)
	{
		ie.DoRehash();
	}

	Version GetVersion()
	{
		return Version("Provides support for the +I channel mode", VF_VENDOR);
	}
};

MODULE_INIT(ModuleInviteException)
