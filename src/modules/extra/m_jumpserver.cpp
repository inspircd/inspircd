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

/// $ModAuthor: InspIRCd Developers
/// $ModAuthorMail: noreply@inspircd.org
/// $ModDepends: core 3
/// $ModDesc: Provides support for the RPL_REDIR numeric and the /JUMPSERVER command.


#include "inspircd.h"
#include "modules/ssl.h"

enum
{
	// From ircd-ratbox.
	RPL_REDIR = 10
};

/** Handle /JUMPSERVER
 */
class CommandJumpserver : public Command
{
 public:
	bool redirect_new_users;
	std::string redirect_to;
	std::string reason;
	int port;
	int sslport;
	UserCertificateAPI sslapi;

	CommandJumpserver(Module* Creator)
		: Command(Creator, "JUMPSERVER", 0, 4)
		, sslapi(Creator)
	{
		allow_empty_last_param = false;
		flags_needed = 'o';
		syntax = "[<server> <port>[:<sslport>] <+/-an> <reason>]";
		port = 0;
		sslport = 0;
		redirect_new_users = false;
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		int n_done = 0;
		reason = (parameters.size() < 4) ? "Please use this server/port instead" : parameters[3];
		bool redirect_all_immediately = false;
		redirect_new_users = true;
		bool direction = true;
		std::string n_done_s;

		/* No parameters: jumpserver disabled */
		if (parameters.empty())
		{
			if (port)
				user->WriteNotice("*** Disabled jumpserver (previously set to '" + redirect_to + ":" + ConvToStr(port) + "')");
			else
				user->WriteNotice("*** Jumpserver was not enabled.");

			port = 0;
			sslport = 0;
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
						user->WriteNotice("*** Invalid JUMPSERVER flag: " + ConvToStr(*n));
						return CMD_FAILURE;
					break;
				}
			}

			size_t delimpos = parameters[1].find(':');
			port = ConvToNum<int>(parameters[1].substr(0, delimpos ? delimpos : std::string::npos));
			sslport = (delimpos == std::string::npos ? 0 : ConvToNum<int>(parameters[1].substr(delimpos + 1)));

			if (parameters[1].find_first_not_of("0123456789:") != std::string::npos
				|| parameters[1].rfind(':') != delimpos
				|| port > 65535 || sslport > 65535)
			{
				user->WriteNotice("*** Invalid port number");
				return CMD_FAILURE;
			}

			if (redirect_all_immediately)
			{
				/* Redirect everyone but the oper sending the command */
				const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
				for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); )
				{
					// Quitting the user removes it from the list
					LocalUser* t = *i;
					++i;
					if (!t->IsOper())
					{
						t->WriteNumeric(RPL_REDIR, parameters[0], GetPort(t), "Please use this Server/Port instead");
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
				redirect_to = parameters[0];

			user->WriteNotice("*** Set jumpserver to server '" + parameters[0] + "' port '" + (port ? ConvToStr(port) : "Auto") + ", SSL " + (sslport ? ConvToStr(sslport) : "Auto") + "', flags '+" +
				(redirect_all_immediately ? "a" : "") + (redirect_new_users ? "n'" : "'") +
				(n_done ? " (" + n_done_s + "user(s) redirected): " : ": ") + reason);
		}

		return CMD_SUCCESS;
	}

	int GetPort(LocalUser* user)
	{
		int p = (sslapi && sslapi->GetCertificate(user) ? sslport : port);
		if (p == 0)
			p = user->server_sa.port();
		return p;
	}
};

class ModuleJumpServer : public Module
{
	CommandJumpserver js;
 public:
	ModuleJumpServer() : js(this)
	{
	}

	void OnModuleRehash(User* user, const std::string& param) CXX11_OVERRIDE
	{
		if (irc::equals(param, "jumpserver") && js.redirect_new_users)
			js.redirect_new_users = false;
	}

	ModResult OnUserRegister(LocalUser* user) CXX11_OVERRIDE
	{
		if (js.redirect_new_users)
		{
			int port = js.GetPort(user);
			user->WriteNumeric(RPL_REDIR, js.redirect_to, port, "Please use this Server/Port instead");
			ServerInstance->Users->QuitUser(user, js.reason);
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		// Emergency way to unlock
		if (!status.srcuser)
			js.redirect_new_users = false;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for the RPL_REDIR numeric and the /JUMPSERVER command.", VF_NONE);
	}
};

MODULE_INIT(ModuleJumpServer)
