/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/isupport.h"

enum
{
	// From RFC 2812.
	RPL_INVEXLIST = 346,
	RPL_ENDOFINVEXLIST = 347
};

class InviteException : public ListModeBase
{
 public:
	InviteException(Module* Creator)
		: ListModeBase(Creator, "invex", 'I', "End of Channel Invite Exception List", RPL_INVEXLIST, RPL_ENDOFINVEXLIST, true)
	{
		syntax = "<mask>";
	}
};

class ModuleInviteException
	: public Module
	, public ISupport::EventListener
{
 private:
	bool invite_bypass_key;
	InviteException ie;

 public:
	ModuleInviteException()
		: ISupport::EventListener(this)
		, ie(this)
	{
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["INVEX"] = ConvToStr(ie.GetModeChar());
	}

	ModResult OnCheckInvite(User* user, Channel* chan) override
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

	ModResult OnCheckKey(User* user, Channel* chan, const std::string& key) override
	{
		if (invite_bypass_key)
			return OnCheckInvite(user, chan);
		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		ie.DoRehash();
		invite_bypass_key = ServerInstance->Config->ConfValue("inviteexception")->getBool("bypasskey", true);
	}

	Version GetVersion() override
	{
		return Version("Provides channel mode +I, invite exceptions", VF_VENDOR);
	}
};

MODULE_INIT(ModuleInviteException)
