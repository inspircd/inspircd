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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

/* $ModDesc: Disallows /LIST for recently connected clients to hinder spam bots */

class ModuleSecureList : public Module
{
 private:
	std::vector<std::string> allowlist;
	time_t WaitTime;
 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnPreCommand, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleSecureList()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Disallows /LIST for recently connected clients to hinder spam bots", VF_VENDOR);
	}

	void OnRehash(User* user)
	{
		allowlist.clear();

		ConfigTagList tags = ServerInstance->Config->ConfTags("securehost");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
			allowlist.push_back(i->second->getString("exception"));

		WaitTime = ServerInstance->Config->ConfValue("securelist")->getInt("waittime", 60);
	}


	/*
	 * OnPreCommand()
	 *   Intercept the LIST command.
	 */
	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return MOD_RES_PASSTHRU;

		if ((command == "LIST") && (ServerInstance->Time() < (user->signon+WaitTime)) && (!IS_OPER(user)))
		{
			/* Normally wouldnt be allowed here, are they exempt? */
			for (std::vector<std::string>::iterator x = allowlist.begin(); x != allowlist.end(); x++)
				if (InspIRCd::Match(user->MakeHost(), *x, ascii_case_insensitive_map))
					return MOD_RES_PASSTHRU;

			/* Not exempt, BOOK EM DANNO! */
			user->WriteServ("NOTICE %s :*** You cannot list within the first %lu seconds of connecting. Please try again later.",user->nick.c_str(), (unsigned long) WaitTime);
			/* Some clients (e.g. mIRC, various java chat applets) muck up if they don't
			 * receive these numerics whenever they send LIST, so give them an empty LIST to mull over.
			 */
			user->WriteNumeric(321, "%s Channel :Users Name",user->nick.c_str());
			user->WriteNumeric(323, "%s :End of channel list.",user->nick.c_str());
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" SECURELIST");
	}
};

MODULE_INIT(ModuleSecureList)
