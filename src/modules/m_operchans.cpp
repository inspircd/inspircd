/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
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

enum
{
	// From UnrealIRCd.
	ERR_CANTJOINOPERSONLY = 520
};

class ModuleOperChans : public Module
{
 private:
	SimpleChannelModeHandler oc;
	std::string space;
	std::string underscore;

 public:
	ModuleOperChans()
		: oc(this, "operonly", 'O', true)
		, space(" ")
		, underscore("_")
	{
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if (chan && chan->IsModeSet(oc) && !user->IsOper())
		{
			user->WriteNumeric(ERR_CANTJOINOPERSONLY, chan->name, InspIRCd::Format("Only server operators may join %s (+O is set)", chan->name.c_str()));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask) CXX11_OVERRIDE
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

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('O');
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for oper-only channels via channel mode +O and extban 'O'", VF_VENDOR);
	}
};

MODULE_INIT(ModuleOperChans)
