/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/cap.h"
#include "modules/names.h"
#include "modules/who.h"

class ModuleNamesX
	: public Module
	, public Names::EventListener
	, public Who::EventListener
{
 private:
	Cap::Capability cap;

 public:
	ModuleNamesX()
		: Names::EventListener(this)
		, Who::EventListener(this)
		, cap(this, "multi-prefix")
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the NAMESX (CAP multi-prefix) capability", VF_VENDOR);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		// The legacy PROTOCTL system is a wrapper around the cap.
		dynamic_reference_nocheck<Cap::Manager> capmanager(this, "capmanager");
		if (capmanager)
			tokens["NAMESX"];
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) CXX11_OVERRIDE
	{
		/* We don't actually create a proper command handler class for PROTOCTL,
		 * because other modules might want to have PROTOCTL hooks too.
		 * Therefore, we just hook its as an unvalidated command therefore we
		 * can capture it even if it doesnt exist! :-)
		 */
		if (command == "PROTOCTL")
		{
			if ((parameters.size()) && (!strcasecmp(parameters[0].c_str(),"NAMESX")))
			{
				cap.set(user, true);
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnNamesListItem(LocalUser* issuer, Membership* memb, std::string& prefixes, std::string& nick) CXX11_OVERRIDE
	{
		if (cap.get(issuer))
			prefixes = memb->GetAllPrefixChars();

		return MOD_RES_PASSTHRU;
	}

	ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric) CXX11_OVERRIDE
	{
		if ((!memb) || (!cap.get(source)))
			return MOD_RES_PASSTHRU;

		// Don't do anything if the user has only one prefix
		std::string prefixes = memb->GetAllPrefixChars();
		if (prefixes.length() <= 1)
			return MOD_RES_PASSTHRU;

		size_t flag_index;
		if (!request.GetFieldIndex('f', flag_index))
			return MOD_RES_PASSTHRU;

		// #chan ident localhost insp22.test nick H@ :0 Attila
		if (numeric.GetParams().size() <= flag_index)
			return MOD_RES_PASSTHRU;

		numeric.GetParams()[flag_index].append(prefixes, 1, std::string::npos);
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleNamesX)
