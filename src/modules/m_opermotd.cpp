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
	return CMD_LOCALONLY;
}

/** Handle /OPERMOTD
 */
class CommandOpermotd : public Command
{
 public:
	CommandOpermotd (InspIRCd* Instance) : Command(Instance,"OPERMOTD", 'o', 0)
	{
		this->source = "m_opermotd.so";
		syntax = "[<servername>]";
	}

	CmdResult Handle (const char** parameters, int pcnt, User* user)
	{
		return ShowOperMOTD(user);
	}
};


class ModuleOpermotd : public Module
{
	CommandOpermotd* mycommand;
 public:

	void LoadOperMOTD()
	{
		ConfigReader* conf = new ConfigReader(ServerInstance);
		std::string filename;
		filename = conf->ReadValue("opermotd","file",0);
		if (opermotd)
		{
			delete opermotd;
			opermotd = NULL;
		}
		opermotd = new FileReader(ServerInstance, filename);
		delete conf;
	}
	
	ModuleOpermotd(InspIRCd* Me)
		: Module(Me)
	{
		opermotd = NULL;
		mycommand = new CommandOpermotd(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		opermotd = new FileReader(ServerInstance);
		LoadOperMOTD();
		Implementation eventlist[] = { I_OnRehash, I_OnOper };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleOpermotd()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}


	virtual void OnOper(User* user, const std::string &opertype)
	{
		ShowOperMOTD(user);
	}

	virtual void OnRehash(User* user, const std::string &parameter)
	{
		LoadOperMOTD();
	}
};

MODULE_INIT(ModuleOpermotd)
