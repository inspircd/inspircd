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
#include "m_cap.h"

/* $ModDesc: Provides the NAMESX (CAP multi-prefix) capability. */

class ModuleNamesX : public Module
{
 public:
	GenericCap cap;
	ModuleNamesX() : cap(this, "multi-prefix")
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_OnPreCommand, I_OnNamesListItem, I_On005Numeric, I_OnEvent, I_OnSendWhoLine };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}


	~ModuleNamesX()
	{
	}

	Version GetVersion()
	{
		return Version("Provides the NAMESX (CAP multi-prefix) capability.",VF_VENDOR);
	}

	void On005Numeric(std::string &output)
	{
		output.append(" NAMESX");
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
			if ((parameters.size()) && (!strcasecmp(parameters[0].c_str(),"NAMESX")))
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

		/* Some module hid this from being displayed, dont bother */
		if (nick.empty())
			return;

		prefixes = memb->chan->GetAllPrefixChars(memb->user);
	}

	void OnSendWhoLine(User* source, const std::vector<std::string>& params, User* user, std::string& line)
	{
		if (!cap.ext.get(source))
			return;

		// Channel names can contain ":", and ":" as a 'start-of-token' delimiter is
		// only ever valid after whitespace, so... find the actual delimiter first!
		// Thanks to FxChiP for pointing this out.
		std::string::size_type pos = line.find(" :");
		if (pos == std::string::npos || pos == 0)
			return;
		pos--;
		// Don't do anything if the user has no prefixes
		if ((line[pos] == 'H') || (line[pos] == 'G') || (line[pos] == '*'))
			return;

		// 352 21DAAAAAB #chan ident localhost insp21.test 21DAAAAAB H@ :0 a
		//              a     b                                       pos
		std::string::size_type a = 4 + source->nick.length() + 1;
		std::string::size_type b = line.find(' ', a);
		if (b == std::string::npos)
			return;

		// Try to find this channel
		std::string channame = line.substr(a, b-a);
		Channel* chan = ServerInstance->FindChan(channame);
		if (!chan)
			return;

		// Don't do anything if the user has only one prefix
		std::string prefixes = chan->GetAllPrefixChars(user);
		if (prefixes.length() <= 1)
			return;

		line.erase(pos, 1);
		line.insert(pos, prefixes);
	}

	void OnEvent(Event& ev)
	{
		cap.HandleEvent(ev);
	}
};

MODULE_INIT(ModuleNamesX)
