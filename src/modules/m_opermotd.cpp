// opermotd module by typobox43

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Shows a message to opers after oper-up, adds /opermotd */

static FileReader* opermotd;
static Server* Srv;

void LoadOperMOTD()
{
	ConfigReader* conf = new ConfigReader;
	std::string filename;

	filename = conf->ReadValue("opermotd","file",0);

	opermotd->LoadFile(filename);
	DELETE(conf);
}

void ShowOperMOTD(userrec* user)
{
	if(!opermotd->FileSize())
	{
		Srv->SendServ(user->fd,std::string("425 ") + user->nick + std::string(" :OPERMOTD file is missing"));
		return;
	}

	Srv->SendServ(user->fd,std::string("375 ") + user->nick + std::string(" :- IRC Operators Message of the Day"));

	for(int i=0; i != opermotd->FileSize(); i++)
	{
		Srv->SendServ(user->fd,std::string("372 ") + user->nick + std::string(" :- ") + opermotd->GetLine(i));
	}

	Srv->SendServ(user->fd,std::string("376 ") + user->nick + std::string(" :- End of OPERMOTD"));

}

class cmd_opermotd : public command_t
{
 public:
	cmd_opermotd () : command_t("OPERMOTD", 'o', 0)
	{
		this->source = "m_opermotd.so";
	}

	void Handle (const char** parameters, int pcnt, userrec* user)
	{
		ShowOperMOTD(user);
	}
};

class ModuleOpermotd : public Module
{
		cmd_opermotd* mycommand;
	public:
		ModuleOpermotd(Server* Me)
			: Module::Module(Me)
		{
			Srv = Me;
			mycommand = new cmd_opermotd();
			Srv->AddCommand(mycommand);
			opermotd = new FileReader();
			LoadOperMOTD();
		}

		virtual ~ModuleOpermotd()
		{
		}

		virtual Version GetVersion()
		{
			return Version(1,0,0,1,VF_VENDOR);
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

		virtual Module* CreateModule(Server* Me)
		{
			return new ModuleOpermotd(Me);
		}

};

extern "C" void* init_module(void)
{
	return new ModuleOpermotdFactory;
}
