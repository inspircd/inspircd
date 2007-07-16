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
#include "xline.h"

/* $ModDesc: Sends a numeric on connect which cripples a common type of trojan/spambot */

class ModuleAntiBear : public Module
{
 private:

 public:
	ModuleAntiBear(InspIRCd* Me) : Module(Me)
	{
		
	}
	
	virtual ~ModuleAntiBear()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR,API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnUserRegister] = List[I_OnPreCommand] = 1;
	}

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
	{
		if (command == "NOTICE" && !validated && pcnt > 1 && user->GetExt("antibear_timewait"))
		{
			if (!strncmp(parameters[1], "\1TIME Mon May 01 18:54:20 2006", 30))
			{
				if (ServerInstance->XLines->add_zline(86400, ServerInstance->Config->ServerName, "Unless you're stuck in a time warp, you appear to be a bear bot!", user->MakeHostIP()))
				{
					ServerInstance->XLines->apply_lines(APPLY_ZLINES);
					FOREACH_MOD(I_OnAddGLine,OnAddZLine(86400, NULL, "Unless you're stuck in a time warp, you appear to be a bear bot!", user->MakeHostIP()));
					return 1;
				}
			}
			
			user->Shrink("antibear_timewait");
			// Block the command, so the user doesn't receive a no such nick notice
			return 1;
		}
		
		return 0;
	}

	virtual int OnUserRegister(userrec* user)
	{
		user->WriteServ("439 %s :This server has anti-spambot mechanisms enabled.", user->nick);
		user->WriteServ("931 %s :Malicious bots, spammers, and other automated systems of dubious origin are NOT welcome here.", user->nick);
		user->WriteServ("PRIVMSG %s :\1TIME\1", user->nick);
		user->Extend("antibear_timewait");
		return 0;
	}
};

MODULE_INIT(ModuleAntiBear)
