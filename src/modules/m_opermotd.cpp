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
		ConfigReader conf;
		std::string filename;
		filename = conf.ReadValue("opermotd","file",0);
		delete opermotd;
		opermotd = new FileReader(filename);
		onoper = conf.ReadFlag("opermotd","onoper","yes",0);
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
