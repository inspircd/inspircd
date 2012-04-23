/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2004 Christopher Hall <typobox43@gmail.com>
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

/* $ModDesc: Shows a message to opers after oper-up, adds /opermotd */

static FileReader* opermotd;

CmdResult ShowOperMOTD(User* user)
{
	if(!opermotd->FileSize())
	{
		user->WriteServ(std::string("425 ") + user->nick + std::string(" :OPERMOTD file is missing"));
		return CMD_FAILURE;
	}

	user->WriteServ(std::string("375 ") + user->nick + std::string(" :- IRC Operators Message of the Day"));

	for(int i=0; i != opermotd->FileSize(); i++)
	{
		user->WriteServ(std::string("372 ") + user->nick + std::string(" :- ") + opermotd->GetLine(i));
	}

	user->WriteServ(std::string("376 ") + user->nick + std::string(" :- End of OPERMOTD"));

	/* don't route me */
	return CMD_SUCCESS;
}

/** Handle /OPERMOTD
 */
class CommandOpermotd : public Command
{
 public:
	CommandOpermotd(Module* Creator) : Command(Creator,"OPERMOTD", 0)
	{
		flags_needed = 'o'; syntax = "[<servername>]";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User* user)
	{
		return ShowOperMOTD(user);
	}
};


class ModuleOpermotd : public Module
{
	CommandOpermotd cmd;
	bool onoper;
 public:

	void LoadOperMOTD()
	{
		ConfigTag* conf = ServerInstance->Config->ConfValue("opermotd");
		opermotd->LoadFile(conf->getString("file","opermotd"));
		onoper = conf->getBool("onoper", true);
	}

	ModuleOpermotd()
		: cmd(this)
	{
		opermotd = NULL;
		ServerInstance->AddCommand(&cmd);
		opermotd = new FileReader;
		LoadOperMOTD();
		Implementation eventlist[] = { I_OnRehash, I_OnOper };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleOpermotd()
	{
		delete opermotd;
		opermotd = NULL;
	}

	virtual Version GetVersion()
	{
		return Version("Shows a message to opers after oper-up, adds /opermotd", VF_VENDOR);
	}

	virtual void OnOper(User* user, const std::string &opertype)
	{
		if (onoper)
			ShowOperMOTD(user);
	}

	virtual void OnRehash(User* user)
	{
		LoadOperMOTD();
	}
};

MODULE_INIT(ModuleOpermotd)
