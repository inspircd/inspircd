/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

/* $ModDesc: Provides the TITLE command which allows setting of CUSTOM WHOIS TITLE line */

/** Handle /TITLE
 */
class CommandTitle : public Command
{
 public:
	StringExtItem ctitle;
	CommandTitle(Module* Creator) : Command(Creator,"TITLE", 2),
		ctitle("ctitle", Creator)
	{
		syntax = "<user> <password>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	bool OneOfMatches(const char* host, const char* ip, const char* hostlist)
	{
		std::stringstream hl(hostlist);
		std::string xhost;
		while (hl >> xhost)
		{
			if (InspIRCd::Match(host, xhost, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(ip, xhost, ascii_case_insensitive_map))
			{
				return true;
			}
		}
		return false;
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User* user)
	{
		char TheHost[MAXBUF];
		char TheIP[MAXBUF];

		snprintf(TheHost,MAXBUF,"%s@%s",user->ident.c_str(), user->host.c_str());
		snprintf(TheIP, MAXBUF,"%s@%s",user->ident.c_str(), user->GetIPString());

		ConfigReader Conf;
		for (int i=0; i<Conf.Enumerate("title"); i++)
		{
			std::string Name = Conf.ReadValue("title", "name", "", i);
			std::string pass = Conf.ReadValue("title", "password", "", i);
			std::string hash = Conf.ReadValue("title", "hash", "", i);
			std::string host = Conf.ReadValue("title", "host", "*@*", i);
			std::string title = Conf.ReadValue("title", "title", "", i);
			std::string vhost = Conf.ReadValue("title", "vhost", "", i);

			if (!strcmp(Name.c_str(),parameters[0].c_str()) && !ServerInstance->PassCompare(user, pass.c_str(), parameters[1].c_str(), hash.c_str()) && OneOfMatches(TheHost,TheIP,host.c_str()) && !title.empty())
			{
				ctitle.set(user, title);

				ServerInstance->PI->SendMetaData(user, "ctitle", title);

				if (!vhost.empty())
					user->ChangeDisplayedHost(vhost.c_str());

				user->WriteServ("NOTICE %s :Custom title set to '%s'",user->nick.c_str(), title.c_str());

				return CMD_SUCCESS;
			}
		}

		user->WriteServ("NOTICE %s :Invalid title credentials",user->nick.c_str());
		return CMD_SUCCESS;
	}

};

class ModuleCustomTitle : public Module
{
	CommandTitle cmd;

 public:
	ModuleCustomTitle() : cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
		ServerInstance->Extensions.Register(&cmd.ctitle);
		ServerInstance->Modules->Attach(I_OnWhoisLine, this);
	}

	// :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
	ModResult OnWhoisLine(User* user, User* dest, int &numeric, std::string &text)
	{
		/* We use this and not OnWhois because this triggers for remote, too */
		if (numeric == 312)
		{
			/* Insert our numeric before 312 */
			const std::string* ctitle = cmd.ctitle.get(dest);
			if (ctitle)
			{
				ServerInstance->SendWhoisLine(user, dest, 320, "%s %s :%s",user->nick.c_str(), dest->nick.c_str(), ctitle->c_str());
			}
		}
		/* Don't block anything */
		return MOD_RES_PASSTHRU;
	}

	~ModuleCustomTitle()
	{
	}

	Version GetVersion()
	{
		return Version("Custom Title for users", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleCustomTitle)
