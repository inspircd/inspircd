// opermotd module by typobox43

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "inspircd.h"

/* $ModDesc: Shows a message to opers after oper-up, adds /opermotd */

static FileReader* opermotd;

CmdResult ShowOperMOTD(userrec* user)
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

	return CMD_SUCCESS;
}

/** Handle /OPERMOTD
 */
class cmd_opermotd : public command_t
{
 public:
	cmd_opermotd (InspIRCd* Instance) : command_t(Instance,"OPERMOTD", 'o', 0)
	{
		this->source = "m_opermotd.so";
		syntax = "[<servername>]";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec* user)
	{
		return ShowOperMOTD(user);
	}
};


class ModuleOpermotd : public Module
{
	cmd_opermotd* mycommand;
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
		DELETE(conf);
	}
	
	ModuleOpermotd(InspIRCd* Me)
		: Module::Module(Me)
	{
		opermotd = NULL;
		mycommand = new cmd_opermotd(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		opermotd = new FileReader(ServerInstance);
		LoadOperMOTD();
	}

	virtual ~ModuleOpermotd()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR,API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnOper] = 1;
	}

	virtual void OnOper(userrec* user, const std::string &opertype)
	{
		ShowOperMOTD(user);
	}

	virtual void OnRehash(const std::string &parameter)
	{
		LoadOperMOTD();
	}
};

class ModuleOpermotdFactory : public ModuleFactory
{
 public:
	ModuleOpermotdFactory()
	{
	}

	~ModuleOpermotdFactory()
	{
	}

	virtual Module* CreateModule(InspIRCd* Me)
	{
		return new ModuleOpermotd(Me);
	}
};

extern "C" void* init_module(void)
{
	return new ModuleOpermotdFactory;
}
