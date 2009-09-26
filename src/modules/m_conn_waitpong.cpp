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

/* $ModDesc: Forces connecting clients to send a PONG message back to the server before they can complete their connection */

class ModuleWaitPong : public Module
{
	bool sendsnotice;
	bool killonbadreply;
	LocalStringExt ext;

 public:
	ModuleWaitPong()
	 : ext("waitpong_pingstr", this)
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnUserRegister, I_OnCheckReady, I_OnPreCommand, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	void OnRehash(User* user)
	{
		ConfigReader Conf;

		sendsnotice = Conf.ReadFlag("waitpong", "sendsnotice", 0);

		if(Conf.GetError() == CONF_VALUE_NOT_FOUND)
			sendsnotice = true;

		killonbadreply = Conf.ReadFlag("waitpong", "killonbadreply", 0);

		if(Conf.GetError() == CONF_VALUE_NOT_FOUND)
			killonbadreply = true;
	}

	std::string RandString()
	{
		char out[11];
		for(unsigned int i = 0; i < 10; i++)
			out[i] = ((rand() % 26) + 65);
		out[10] = '\0';

		return out;
	}

	ModResult OnUserRegister(User* user)
	{
		std::string pingrpl = RandString();

		user->Write("PING :%s", pingrpl.c_str());

		if(sendsnotice)
			user->WriteServ("NOTICE %s :*** If you are having problems connecting due to ping timeouts, please type /quote PONG %s or /raw PONG %s now.", user->nick.c_str(), pingrpl.c_str(), pingrpl.c_str());

		ext.set(user, pingrpl);
		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, User* user, bool validated, const std::string &original_line)
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

	ModResult OnCheckReady(User* user)
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
