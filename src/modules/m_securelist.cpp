/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "modules/account.h"

typedef std::vector<std::string> AllowList;

class ModuleSecureList : public Module
{
	AllowList allowlist;
	bool exemptregistered;
	unsigned int WaitTime;

 public:
	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Disallows /LIST for recently connected clients to hinder spam bots", VF_VENDOR);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		AllowList newallows;

		ConfigTagList tags = ServerInstance->Config->ConfTags("securehost");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			std::string host = i->second->getString("exception");
			if (host.empty())
				throw ModuleException("<securehost:exception> is a required field at " + i->second->getTagLocation());
			newallows.push_back(host);
		}

		ConfigTag* tag = ServerInstance->Config->ConfValue("securelist");

		exemptregistered = tag->getBool("exemptregistered");
		WaitTime = tag->getDuration("waittime", 60, 1);
		allowlist.swap(newallows);
	}


	/*
	 * OnPreCommand()
	 *   Intercept the LIST command.
	 */
	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) CXX11_OVERRIDE
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return MOD_RES_PASSTHRU;

		if ((command == "LIST") && (ServerInstance->Time() < (user->signon+WaitTime)) && (!user->IsOper()))
		{
			/* Normally wouldnt be allowed here, are they exempt? */
			for (std::vector<std::string>::iterator x = allowlist.begin(); x != allowlist.end(); x++)
				if (InspIRCd::Match(user->MakeHost(), *x, ascii_case_insensitive_map))
					return MOD_RES_PASSTHRU;

			const AccountExtItem* ext = GetAccountExtItem();
			if (exemptregistered && ext && ext->get(user))
				return MOD_RES_PASSTHRU;

			/* Not exempt, BOOK EM DANNO! */
			user->WriteNotice("*** You cannot list within the first " + ConvToStr(WaitTime) + " seconds of connecting. Please try again later.");
			/* Some clients (e.g. mIRC, various java chat applets) muck up if they don't
			 * receive these numerics whenever they send LIST, so give them an empty LIST to mull over.
			 */
			user->WriteNumeric(RPL_LISTSTART, "Channel", "Users Name");
			user->WriteNumeric(RPL_LISTEND, "End of channel list.");
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["SECURELIST"];
	}
};

MODULE_INIT(ModuleSecureList)
