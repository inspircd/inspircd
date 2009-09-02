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

/* $ModDesc: Provides support for the RPL_REDIR numeric */

/** Handle /JUMPSERVER
 */
class CommandJumpserver : public Command
{
 public:
	bool redirect_all_immediately;
	bool redirect_new_users;
	bool direction;
	std::string redirect_to;
	std::string reason;
	int port;

	CommandJumpserver (InspIRCd* Instance) : Command(Instance, "JUMPSERVER", "o", 0, 4)
	{
		this->source = "m_jumpserver.so";
		syntax = "[<server> <port> <+/-an> <reason>]";
		redirect_to.clear();
		reason.clear();
		port = 0;
		redirect_all_immediately = redirect_new_users = false;
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		int n_done = 0;
		reason = (parameters.size() < 4) ? "Please use this server/port instead" : parameters[3];
		redirect_all_immediately = false;
		redirect_new_users = true;
		direction = true;
		std::string n_done_s;

		/* No parameters: jumpserver disabled */
		if (!parameters.size())
		{
			if (port)
				user->WriteServ("NOTICE %s :*** Disabled jumpserver (previously set to '%s:%d')", user->nick.c_str(), redirect_to.c_str(), port);
			else
				user->WriteServ("NOTICE %s :*** jumpserver was not enabled.", user->nick.c_str());

			port = 0;
			redirect_to.clear();
			return CMD_LOCALONLY;
		}

		port = 0;
		redirect_to.clear();

		if (parameters.size() >= 3)
		{
			for (const char* n = parameters[2].c_str(); *n; n++)
			{
				switch (*n)
				{
					case '+':
						direction = true;
					break;
					case '-':
						direction = false;
					break;
					case 'a':
						redirect_all_immediately = direction;
					break;
					case 'n':
						redirect_new_users = direction;
					break;
					default:
						user->WriteServ("NOTICE %s :*** Invalid JUMPSERVER flag: %c", user->nick.c_str(), *n);
						return CMD_FAILURE;
					break;
				}
			}

			if (!atoi(parameters[1].c_str()))
			{
				user->WriteServ("NOTICE %s :*** Invalid port number", user->nick.c_str());
				return CMD_FAILURE;
			}

			if (redirect_all_immediately)
			{
				/* Redirect everyone but the oper sending the command */
				for (std::vector<User*>::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); i++)
				{
					User* t = *i;
					if (!IS_OPER(t))
					{
						t->WriteNumeric(10, "%s %s %s :Please use this Server/Port instead", user->nick.c_str(), parameters[0].c_str(), parameters[1].c_str());
						ServerInstance->Users->QuitUser(t, reason);
						n_done++;
					}
				}
				if (n_done)
				{
					n_done_s = ConvToStr(n_done);
				}
			}

			if (redirect_new_users)
			{
				redirect_to = parameters[0];
				port = atoi(parameters[1].c_str());
			}

			user->WriteServ("NOTICE %s :*** Set jumpserver to server '%s' port '%s', flags '+%s%s'%s%s%s: %s", user->nick.c_str(), parameters[0].c_str(), parameters[1].c_str(),
					redirect_all_immediately ? "a" : "",
					redirect_new_users ? "n" : "",
					n_done ? " (" : "",
					n_done ? n_done_s.c_str() : "",
					n_done ? " user(s) redirected)" : "",
					reason.c_str());
		}

		return CMD_LOCALONLY;
	}
};


class ModuleJumpServer : public Module
{
	CommandJumpserver js;
 public:
	ModuleJumpServer(InspIRCd* Me)
		: Module(Me), js(Me)
	{
		ServerInstance->AddCommand(&js);
		Implementation eventlist[] = { I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModuleJumpServer()
	{
	}

	virtual int OnUserRegister(User* user)
	{
		if (js.port && js.redirect_new_users)
		{
			user->WriteNumeric(10, "%s %s %d :Please use this Server/Port instead",
				user->nick.c_str(), js.redirect_to.c_str(), js.port);
			ServerInstance->Users->QuitUser(user, js.reason);
			return 0;
		}
		return 0;
	}


	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleJumpServer)
