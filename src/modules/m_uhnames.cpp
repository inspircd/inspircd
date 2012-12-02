/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "m_cap.h"

/* $ModDesc: Provides the UHNAMES facility. */

class ModuleUHNames : public Module
{
 public:
	GenericCap cap;

	ModuleUHNames() : cap(this, "userhost-in-names")
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_OnEvent, I_OnPreCommand, I_OnNamesListItem, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	~ModuleUHNames()
	{
	}

	Version GetVersion()
	{
		return Version("Provides the UHNAMES facility.",VF_VENDOR);
	}

	void On005Numeric(std::string &output)
	{
		output.append(" UHNAMES");
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		/* We don't actually create a proper command handler class for PROTOCTL,
		 * because other modules might want to have PROTOCTL hooks too.
		 * Therefore, we just hook its as an unvalidated command therefore we
		 * can capture it even if it doesnt exist! :-)
		 */
		if (command == "PROTOCTL")
		{
			if ((parameters.size()) && (!strcasecmp(parameters[0].c_str(),"UHNAMES")))
			{
				cap.ext.set(user, 1);
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnNamesListItem(User* issuer, Membership* memb, std::string &prefixes, std::string &nick)
	{
		if (!cap.ext.get(issuer))
			return;

		if (nick.empty())
			return;

		nick = memb->user->GetFullHost();
	}

	void OnEvent(Event& ev)
	{
		cap.HandleEvent(ev);
	}
};

MODULE_INIT(ModuleUHNames)
