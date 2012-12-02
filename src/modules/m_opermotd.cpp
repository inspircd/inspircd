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

/** Handle /OPERMOTD
 */
class CommandOpermotd : public Command
{
 public:
	file_cache opermotd;

	CommandOpermotd(Module* Creator) : Command(Creator,"OPERMOTD", 0, 1)
	{
		flags_needed = 'o'; syntax = "[<servername>]";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User* user)
	{
		if ((parameters.empty()) || (parameters[0] == ServerInstance->Config->ServerName))
			ShowOperMOTD(user);
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (!parameters.empty())
			return ROUTE_OPT_UCAST(parameters[0]);
		return ROUTE_LOCALONLY;
	}

	void ShowOperMOTD(User* user)
	{
		const std::string& servername = ServerInstance->Config->ServerName;
		if (opermotd.empty())
		{
			user->SendText(":%s 455 %s :OPERMOTD file is missing", servername.c_str(), user->nick.c_str());
			return;
		}

		user->SendText(":%s 375 %s :- IRC Operators Message of the Day", servername.c_str(), user->nick.c_str());

		for (file_cache::const_iterator i = opermotd.begin(); i != opermotd.end(); ++i)
		{
			user->SendText(":%s 372 %s :- %s", servername.c_str(), user->nick.c_str(), i->c_str());
		}

		user->SendText(":%s 376 %s :- End of OPERMOTD", servername.c_str(), user->nick.c_str());
	}
};


class ModuleOpermotd : public Module
{
	CommandOpermotd cmd;
	bool onoper;
 public:

	ModuleOpermotd()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnOper };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual Version GetVersion()
	{
		return Version("Shows a message to opers after oper-up, adds /opermotd", VF_VENDOR | VF_OPTCOMMON);
	}

	virtual void OnOper(User* user, const std::string &opertype)
	{
		if (onoper && IS_LOCAL(user))
			cmd.ShowOperMOTD(user);
	}

	virtual void OnRehash(User* user)
	{
		cmd.opermotd.clear();
		ConfigTag* conf = ServerInstance->Config->ConfValue("opermotd");
		onoper = conf->getBool("onoper", true);

		FileReader f(conf->getString("file", "opermotd"));
		for (int i=0, filesize = f.FileSize(); i < filesize; i++)
			cmd.opermotd.push_back(f.GetLine(i));

		if (conf->getBool("processcolors"))
			InspIRCd::ProcessColors(cmd.opermotd);
	}
};

MODULE_INIT(ModuleOpermotd)
