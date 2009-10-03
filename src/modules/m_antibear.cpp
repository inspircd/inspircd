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
#include "xline.h"

/* $ModDesc: Sends a numeric on connect which cripples a common type of trojan/spambot */

class ModuleAntiBear : public Module
{
	LocalIntExt bearExt;
 public:
	ModuleAntiBear() : bearExt("antibear_timewait", this)
	{
		Extensible::Register(&bearExt);
		Implementation eventlist[] = { I_OnUserRegister, I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleAntiBear()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Sends a numeric on connect which cripples a common type of trojan/spambot",VF_VENDOR,API_VERSION);
	}

	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		if (command == "NOTICE" && !validated && parameters.size() > 1 && bearExt.get(user))
		{
			if (!strncmp(parameters[1].c_str(), "\1TIME Mon May 01 18:54:20 2006", 30))
			{
				ZLine* zl = new ZLine(ServerInstance->Time(), 86400, ServerInstance->Config->ServerName.c_str(),
						"Unless you're stuck in a time warp, you appear to be a bear bot!", user->GetIPString());
				if (ServerInstance->XLines->AddLine(zl,NULL))
				{
					ServerInstance->XLines->ApplyLines();
				}
				else
					delete zl;

				return MOD_RES_DENY;
			}

			bearExt.set(user, 0);
			// Block the command, so the user doesn't receive a no such nick notice
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserRegister(User* user)
	{
		user->WriteNumeric(439, "%s :This server has anti-spambot mechanisms enabled.", user->nick.c_str());
		user->WriteNumeric(931, "%s :Malicious bots, spammers, and other automated systems of dubious origin are NOT welcome here.", user->nick.c_str());
		user->WriteServ("PRIVMSG %s :\1TIME\1", user->nick.c_str());
		bearExt.set(user, 1);
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleAntiBear)
