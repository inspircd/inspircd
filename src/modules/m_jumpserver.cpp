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

/* $ModDesc: Provides support for unreal-style SAPART command */

/** Handle /SAPART
 */
class cmd_jumpserver : public command_t
{
 public:
	bool redirect_all_immediately;
	bool redirect_new_users;
	bool direction;
	std::string redirect_to;
	std::string reason;
	int port;

	cmd_jumpserver (InspIRCd* Instance) : command_t(Instance, "JUMPSERVER", 'o', 0)
	{
		this->source = "m_jumpserver.so";
		syntax = "[<server> <port> <+/-a> :<reason>]";
		redirect_to.clear();
		reason.clear();
		port = 0;
		redirect_all_immediately = redirect_new_users = false;
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		int n_done = 0;
		reason = (pcnt < 4) ? "Please use this server/port instead" : parameters[3];
		redirect_all_immediately = false;
		redirect_new_users = true;
		direction = true;
		std::string n_done_s;

		/* No parameters: jumpserver disabled */
		if (!pcnt)
		{
			if (port)
				user->WriteServ("NOTICE %s :*** Disabled jumpserver (previously set to '%s:%d')", user->nick, redirect_to.c_str(), port);
			else
				user->WriteServ("NOTICE %s :*** jumpserver was not enabled.", user->nick);

			port = 0;
			redirect_to.clear();
			return CMD_LOCALONLY;
		}

		port = 0;
		redirect_to.clear();

		for (const char* n = parameters[2]; *n; n++)
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
			}
		}

		if (redirect_all_immediately)
		{
			/* Redirect everyone but the oper sending the command */
			for (std::vector<userrec*>::const_iterator i = ServerInstance->local_users.begin(); i != ServerInstance->local_users.end(); i++)
			{
				userrec* t = *i;
				if (!IS_OPER(t))
				{
					t->WriteServ("010 %s %s %s :Please use this Server/Port instead", user->nick, parameters[0], parameters[1]);
					userrec::QuitUser(ServerInstance, t, reason);
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
			port = atoi(parameters[1]);
		}

		user->WriteServ("NOTICE %s :*** Set jumpserver to server '%s' port '%s', flags '+%s%s'%s%s%s: %s", user->nick, parameters[0], parameters[1],
				redirect_all_immediately ? "a" : "",
				redirect_new_users ? "n" : "",
				n_done ? " (" : "",
				n_done ? n_done_s.c_str() : "",
				n_done ? " user(s) redirected)" : "",
				reason.c_str());

		return CMD_LOCALONLY;
	}
};


class ModuleJumpServer : public Module
{
	cmd_jumpserver*	js;
 public:
	ModuleJumpServer(InspIRCd* Me)
		: Module(Me)
	{
		
		js = new cmd_jumpserver(ServerInstance);
		ServerInstance->AddCommand(js);
	}
	
	virtual ~ModuleJumpServer()
	{
	}

	virtual int OnUserRegister(userrec* user)
	{
		if (js->port && js->redirect_new_users)
		{
			user->WriteServ("010 %s %s %d :Please use this Server/Port instead", user->nick, js->redirect_to.c_str(), js->port);
			userrec::QuitUser(ServerInstance, user, js->reason);
			return 0;
		}
		return 0;
	}

	virtual void Implements(char* List)
	{
		List[I_OnUserRegister] = 1;
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleJumpServer)
