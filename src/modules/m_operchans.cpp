/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004, 2006 Craig Edwards <craigedwards@brainbox.cc>
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
#include "modules/isupport.h"

enum
{
	// From UnrealIRCd.
	ERR_CANTJOINOPERSONLY = 520
};

class OperChans : public SimpleChannelModeHandler
{
 public:
	/* This is an oper-only mode */
	OperChans(Module* Creator) : SimpleChannelModeHandler(Creator, "operonly", 'O')
	{
		oper = true;
	}
};

class ModuleOperChans
	: public Module
	, public ISupport::EventListener
{
 private:
	OperChans oc;
	std::string space;
	std::string underscore;

 public:
	ModuleOperChans()
		: ISupport::EventListener(this)
		, oc(this)
		, space(" ")
		, underscore("_")
	{
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) override
	{
		if (chan && chan->IsModeSet(oc) && !user->IsOper())
		{
			user->WriteNumeric(ERR_CANTJOINOPERSONLY, chan->name, InspIRCd::Format("Only server operators may join %s (+O is set)", chan->name.c_str()));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask) override
	{
		// Check whether the entry is an extban.
		if (mask.length() <= 2 || mask[0] != 'O' || mask[1] != ':')
			return MOD_RES_PASSTHRU;

		// If the user is not an oper they can't match this.
		if (!user->IsOper())
			return MOD_RES_PASSTHRU;

		// Check whether the oper's type matches the ban.
		const std::string submask = mask.substr(2);
		if (InspIRCd::Match(user->oper->name, submask))
			return MOD_RES_DENY;

		// If the oper's type contains spaces recheck with underscores.
		std::string opername(user->oper->name);
		stdalgo::string::replace_all(opername, space, underscore);
		if (InspIRCd::Match(opername, submask))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["EXTBAN"].push_back('O');
	}

	Version GetVersion() override
	{
		return Version("Provides support for oper-only channels via channel mode +O and extban 'O'", VF_VENDOR);
	}
};

MODULE_INIT(ModuleOperChans)
