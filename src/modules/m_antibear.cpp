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
#include "xline.h"

/* $ModDesc: Sends a numeric on connect which cripples a common type of trojan/spambot */

class ModuleAntiBear : public Module
{
 private:

 public:
	ModuleAntiBear(InspIRCd* Me) : Module(Me)
	{
		
		Implementation eventlist[] = { I_OnUserRegister, I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}
	
	virtual ~ModuleAntiBear()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR,API_VERSION);
	}


	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, User *user, bool validated, const std::string &original_line)
	{
		if (command == "NOTICE" && !validated && pcnt > 1 && user->GetExt("antibear_timewait"))
		{
			if (!strncmp(parameters[1], "\1TIME Mon May 01 18:54:20 2006", 30))
			{
				ZLine* zl = new ZLine(ServerInstance, ServerInstance->Time(), 86400, ServerInstance->Config->ServerName,
						"Unless you're stuck in a time warp, you appear to be a bear bot!", user->GetIPString());
				if (ServerInstance->XLines->AddLine(zl,NULL))
				{
					ServerInstance->XLines->ApplyLines();
				}
				else
					delete zl;

				return 1;
			}
			
			user->Shrink("antibear_timewait");
			// Block the command, so the user doesn't receive a no such nick notice
			return 1;
		}
		
		return 0;
	}

	virtual int OnUserRegister(User* user)
	{
		user->WriteServ("439 %s :This server has anti-spambot mechanisms enabled.", user->nick);
		user->WriteServ("931 %s :Malicious bots, spammers, and other automated systems of dubious origin are NOT welcome here.", user->nick);
		user->WriteServ("PRIVMSG %s :\1TIME\1", user->nick);
		user->Extend("antibear_timewait");
		return 0;
	}
};

MODULE_INIT(ModuleAntiBear)
