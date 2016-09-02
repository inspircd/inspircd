/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Provides support for the RPL_REDIR numeric and the /JUMPSERVER command. */

/** Handle /JUMPSERVER
 */
class CommandJumpserver : public Command
{
 public:
	bool redirect_new_users;
	std::string redirect_to;
	std::string reason;
	int port;

	CommandJumpserver(Module* Creator) : Command(Creator, "JUMPSERVER", 0, 4)
	{
		flags_needed = 'o'; syntax = "[<server> <port> <+/-an> <reason>]";
		port = 0;
		redirect_new_users = false;
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		int n_done = 0;
		reason = (parameters.size() < 4) ? "Please use this server/port instead" : parameters[3];
		bool redirect_all_immediately = false;
		redirect_new_users = true;
		bool direction = true;
		std::string n_done_s;

		/* No parameters: jumpserver disabled */
		if (!parameters.size())
		{
			if (port)
				user->WriteServ("NOTICE %s :*** Disabled jumpserver (previously set to '%s:%d')", user->nick.c_str(), redirect_to.c_str(), port);
			else
				user->WriteServ("NOTICE %s :*** Jumpserver was not enabled.", user->nick.c_str());

			port = 0;
			redirect_to.clear();
			return CMD_SUCCESS;
		}

		port = 0;
		redirect_to.clear();

		if (parameters.size() >= 3)
		{
			for (std::string::const_iterator n = parameters[2].begin(); n != parameters[2].end(); ++n)
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
				for (LocalUserList::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); ++i)
				{
					User* t = *i;
					if (!IS_OPER(t))
					{
						t->WriteNumeric(10, "%s %s %s :Please use this Server/Port instead", t->nick.c_str(), parameters[0].c_str(), parameters[1].c_str());
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

		return CMD_SUCCESS;
	}
};


class ModuleJumpServer : public Module
{
	CommandJumpserver js;
 public:
	ModuleJumpServer() : js(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(js);
		Implementation eventlist[] = { I_OnUserRegister, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleJumpServer()
	{
	}

	virtual ModResult OnUserRegister(LocalUser* user)
	{
		if (js.port && js.redirect_new_users)
		{
			user->WriteNumeric(10, "%s %s %d :Please use this Server/Port instead",
				user->nick.c_str(), js.redirect_to.c_str(), js.port);
			ServerInstance->Users->QuitUser(user, js.reason);
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void OnRehash(User* user)
	{
		// Emergency way to unlock
		if (!user) js.redirect_new_users = false;
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for the RPL_REDIR numeric and the /JUMPSERVER command.", VF_VENDOR);
	}

};

MODULE_INIT(ModuleJumpServer)
