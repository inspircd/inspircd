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
#include "modules/who.h"

class ModuleNamesX
	: public Module
	, public Who::EventListener
{
 private:
	Cap::Capability cap;

 public:
	ModuleNamesX()
		: Who::EventListener(this)
		, cap(this, "multi-prefix")
	{
	}

	Version GetVersion() override
	{
		return Version("Provides the NAMESX (CAP multi-prefix) capability.",VF_VENDOR);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) override
	{
		tokens["NAMESX"];
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
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

	ModResult OnNamesListItem(User* issuer, Membership* memb, std::string& prefixes, std::string& nick) override
	{
		if (cap.get(issuer))
			prefixes = memb->GetAllPrefixChars();

		return MOD_RES_PASSTHRU;
	}

	ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric) override
	{
		if ((!memb) || (!cap.get(source)))
			return MOD_RES_PASSTHRU;

		// Don't do anything if the user has only one prefix
		std::string prefixes = memb->GetAllPrefixChars();
		if (prefixes.length() <= 1)
			return MOD_RES_PASSTHRU;

		size_t flag_index = 5;
		if (request.whox)
		{
			// We only need to fiddle with the flags if they are present.
			if (!request.whox_fields['f'])
				return MOD_RES_PASSTHRU;

			// WHOX makes this a bit tricky as we need to work out the parameter which the flags are in.
			flag_index = 0;
			static const char* flags = "tcuihsn";
			for (size_t i = 0; i < strlen(flags); ++i)
			{
				if (request.whox_fields[flags[i]])
					flag_index += 1;
			}
		}

		// #chan ident localhost insp22.test nick H@ :0 Attila
		if (numeric.GetParams().size() <= flag_index)
			return MOD_RES_PASSTHRU;

		numeric.GetParams()[flag_index].append(prefixes, 1, std::string::npos);
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleNamesX)
