/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Forces connecting clients to send a PONG message back to the server before they can complete their connection */

class ModuleWaitPong : public Module
{
	bool sendsnotice;
	bool killonbadreply;
	LocalStringExt ext;

 public:
	ModuleWaitPong()
	 : ext(EXTENSIBLE_USER, "waitpong_pingstr", this)
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_OnUserRegister, I_OnCheckReady, I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		sendsnotice = ServerInstance->Config->GetTag("waitpong")->getBool("sendsnotice", true);
		killonbadreply = ServerInstance->Config->GetTag("waitpong")->getBool("killonbadreply", true);
	}

	void OnUserRegister(LocalUser* user)
	{
		std::string pingrpl = ServerInstance->GenRandomStr(10);

		user->Write("PING :%s", pingrpl.c_str());

		if(sendsnotice)
			user->WriteServ("NOTICE %s :*** If you are having problems connecting due to ping timeouts, please type /quote PONG %s or /raw PONG %s now.", user->nick.c_str(), pingrpl.c_str(), pingrpl.c_str());

		ext.set(user, pingrpl);
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser* user, bool validated, const std::string &original_line)
	{
		if (command == "PONG")
		{
			std::string* pingrpl = ext.get(user);

			if (pingrpl)
			{
				if (!parameters.empty() && *pingrpl == parameters[0])
				{
					ext.unset(user);
					return MOD_RES_DENY;
				}
				else
				{
					if(killonbadreply)
						ServerInstance->Users->QuitUser(user, "Incorrect ping reply for registration");
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		return ext.get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	~ModuleWaitPong()
	{
	}

	Version GetVersion()
	{
		return Version("Require pong prior to registration", VF_VENDOR);
	}

};

MODULE_INIT(ModuleWaitPong)
