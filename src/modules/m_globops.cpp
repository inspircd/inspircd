/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

// Globops and +g support module by C.J.Edwards

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides support for GLOBOPS and user mode +g */

extern InspIRCd* ServerInstance;

class cmd_globops : public command_t
{
 public:
	cmd_globops () : command_t("GLOBOPS",'o',1)
	{
		this->source = "m_globops.so";
		syntax = "<any-text>";
	}
	
	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		std::string line = "*** GLOBOPS - From " + std::string(user->nick) + ": ";
		for (int i = 0; i < pcnt; i++)
		{
			line = line + std::string(parameters[i]) + " ";
		}
		ServerInstance->WriteMode("og",WM_AND,line.c_str());
	}
};

class ModeGlobops : public ModeHandler
{
 public:
	ModeGlobops(InspIRCd* Instance) : ModeHandler(Instance, 'g', 0, 0, false, MODETYPE_USER, true) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('g'))
			{
				dest->SetMode('P',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('g'))
			{
				dest->SetMode('P',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};


class ModuleGlobops : public Module
{
	cmd_globops* mycommand;
	ModeGlobops* mg;
 public:
	ModuleGlobops(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mg = new ModeGlobops(ServerInstance);
		ServerInstance->AddMode(mg, 'g');
		mycommand = new cmd_globops();
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleGlobops()
	{
		DELETE(mycommand);
		DELETE(mg);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_STATIC|VF_VENDOR);
	}

	void Implements(char* List)
	{
	}
};

class ModuleGlobopsFactory : public ModuleFactory
{
 public:
	ModuleGlobopsFactory()
	{
	}
	
	~ModuleGlobopsFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleGlobops(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleGlobopsFactory;
}

