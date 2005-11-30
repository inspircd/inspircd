// opermotd module by typobox43

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Shows a message to opers after oper-up, adds /opermotd */

FileReader* opermotd;
Server* Srv;

void do_opermotd(char** parameters, int pcnt, userrec* user);

void LoadOperMOTD()
{

	ConfigReader* conf = new ConfigReader;
	std::string filename;

	filename = conf->ReadValue("opermotd","file",0);

	opermotd->LoadFile(filename);
	
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

void do_opermotd(char** parameters, int pcnt, userrec* user)
{
	ShowOperMOTD(user);
}

class ModuleOpermotd : public Module
{
	public:
		ModuleOpermotd()
		{
			Srv = new Server;
			
			Srv->AddCommand("OPERMOTD",do_opermotd,'o',0,"m_opermotd.so");
			opermotd = new FileReader();
			LoadOperMOTD();
		}

		virtual ~ModuleOpermotd()
		{
			delete Srv;
		}

		virtual Version GetVersion()
		{
			return Version(1,0,0,1,VF_VENDOR);
		}

		virtual void OnOper(userrec* user, std::string opertype)
		{
			ShowOperMOTD(user);
		}

		virtual void OnRehash()
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

		virtual Module* CreateModule()
		{
			return new ModuleOpermotd;
		}

};

extern "C" void* init_module(void)
{
	return new ModuleOpermotdFactory;
}
