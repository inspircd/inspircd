/* Written by Special (john@yarbbles.com) */

using namespace std;

#include <stdio.h>
#include <string>
#include "inspircd.h"
#include "modules.h"

/* $ModDesc: Display timestamps from all servers connected to the network */

class cmd_alltime : public command_t
{
 public:
	cmd_alltime(InspIRCd *Instance) : command_t(Instance, "ALLTIME", 'o', 0)
	{
		this->source = "m_alltime.so";
		syntax = "";
	}

	CmdResult Handle(const char **parameters, int pcnt, userrec *user)
	{
		char fmtdate[64];
		time_t now = ServerInstance->Time();
		strftime(fmtdate, sizeof(fmtdate), "%F %T", gmtime(&now));
		
		// I'm too lazy to add a function to fetch the delta, so lets just cheat..
		int delta = time(NULL) - now;
		
		string msg = ":" + string(ServerInstance->Config->ServerName) + " NOTICE " + user->nick + " :Time for " +
			ServerInstance->Config->ServerName + " is: " + fmtdate + " (delta " + ConvToStr(delta) + " seconds)";
		
		if (IS_LOCAL(user))
		{
			user->Write(msg);
		}
		else
		{
			deque<string> params;
			params.push_back(user->nick);
			params.push_back(msg);
			Event ev((char *) &params, NULL, "send_push");
			ev.Send(ServerInstance);
		}
		
		return CMD_SUCCESS;
	}
};

class Modulealltime : public Module
{
	cmd_alltime *mycommand;
 public:
	Modulealltime(InspIRCd *Me)
		: Module::Module(Me)
	{
		mycommand = new cmd_alltime(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~Modulealltime()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
};

class ModulealltimeFactory : public ModuleFactory
{
 public:
	ModulealltimeFactory()
	{
	}
	
	~ModulealltimeFactory()
	{
	}
	
	virtual Module *CreateModule(InspIRCd *Me)
	{
		return new Modulealltime(Me);
	}
};


extern "C" void *init_module(void)
{
	return new ModulealltimeFactory;
}
