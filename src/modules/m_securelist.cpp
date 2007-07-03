/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h" 
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: A module overriding /list, and making it safe - stop those sendq problems. */

class ModuleSecureList : public Module
{
 private:
	std::vector<std::string> allowlist;
	time_t WaitTime;
 public:
	ModuleSecureList(InspIRCd* Me) : Module(Me)
	{
		OnRehash(NULL,"");
	}
 
	virtual ~ModuleSecureList()
	{
	}
 
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR,API_VERSION);
	}

	void OnRehash(userrec* user, const std::string &parameter)
	{
		ConfigReader* MyConf = new ConfigReader(ServerInstance);
		allowlist.clear();
		for (int i = 0; i < MyConf->Enumerate("securehost"); i++)
			allowlist.push_back(MyConf->ReadValue("securehost", "exception", i));
		WaitTime = MyConf->ReadInteger("securelist", "waittime", "60", 0, true);
		DELETE(MyConf);
	}
 
	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnPreCommand] = List[I_On005Numeric] = 1;
	}

	/*
	 * OnPreCommand()
	 *   Intercept the LIST command.
	 */ 
	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return 0;
 
		if ((command == "LIST") && (ServerInstance->Time() < (user->signon+WaitTime)) && (!IS_OPER(user)))
		{
			/* Normally wouldnt be allowed here, are they exempt? */
			for (std::vector<std::string>::iterator x = allowlist.begin(); x != allowlist.end(); x++)
				if (ServerInstance->MatchText(user->MakeHost(), *x))
					return 0;

			/* Not exempt, BOOK EM DANNO! */
			user->WriteServ("NOTICE %s :*** You cannot list within the first %d seconds of connecting. Please try again later.",user->nick, WaitTime);
			/* Some crap clients (read: mIRC, various java chat applets) muck up if they don't
			 * receive these numerics whenever they send LIST, so give them an empty LIST to mull over.
			 */
			user->WriteServ("321 %s Channel :Users Name",user->nick);
			user->WriteServ("323 %s :End of channel list.",user->nick);
			return 1;
		}
		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" SECURELIST");
	}

	virtual Priority Prioritize()
	{
		return (Priority)ServerInstance->PriorityBefore("m_safelist.so");
	}

};
 
MODULE_INIT(ModuleSecureList)
