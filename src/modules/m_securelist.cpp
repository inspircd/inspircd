/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: A module overriding /list, and making it safe - stop those sendq problems. */

class ModuleSecureList : public Module
{
 private:
	std::vector<std::string> allowlist;
	time_t WaitTime;
 public:
	ModuleSecureList() 	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnPreCommand, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleSecureList()
	{
	}

	virtual Version GetVersion()
	{
		return Version("A module overriding /list, and making it safe - stop those sendq problems.",VF_VENDOR);
	}

	void OnRehash(User* user)
	{
		ConfigReader* MyConf = new ConfigReader;
		allowlist.clear();

		for (int i = 0; i < MyConf->Enumerate("securehost"); i++)
			allowlist.push_back(MyConf->ReadValue("securehost", "exception", i));

		WaitTime = MyConf->ReadInteger("securelist", "waittime", "60", 0, true);
		delete MyConf;
	}


	/*
	 * OnPreCommand()
	 *   Intercept the LIST command.
	 */
	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
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
			/* Some crap clients (read: mIRC, various java chat applets) muck up if they don't
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
