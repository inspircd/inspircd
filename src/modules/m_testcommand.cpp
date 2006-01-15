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

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Povides a proof-of-concept test /WOOT command */

Server *Srv;
	 
class cmd_woot : public command_t
{
 public:
	cmd_woot () : command_t("WOOT", 0, 0)
	{
		this->source = "m_testcommand.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		Srv->Log(DEBUG,"woot handler");
		// Here is a sample of how to send servermodes. Note that unless remote
		// servers in your net are u:lined, they may reverse this, but its a
		// quick and effective modehack.
		
		// NOTE: DO NOT CODE LIKE THIS!!! This has no checks and can send
		// invalid or plain confusing mode changes, code some checking!
		char* modes[3];
		modes[0] = "#chatspike";
		modes[1] = "+o";
		modes[2] = user->nick;
		
		// run the mode change, send numerics (such as "no such channel") back
		// to "user".
		Srv->SendMode(modes,3,user);
	}
};

class ModuleTestCommand : public Module
{
	cmd_woot* newcommand;
 public:
	ModuleTestCommand(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		// Create a new command:
		// command will be called /WOOT, and will
		// call handle_woot when triggered, the
		// 0 in the modes parameter signifies that
		// anyone can issue the command, and the
		// command takes only one parameter.
		newcommand = new cmd_woot();
		Srv->AddCommand(newcommand);

		// Add a mode +Z for channels with no parameters		
		Srv->AddExtendedMode('Z',MT_CHANNEL,false,1,0);
	}

	void Implements(char* List)
	{
		List[I_OnExtendedMode] = List[I_OnUserJoin] = 1;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		
		if ((modechar != 'Z') || (type != MT_CHANNEL))
		{
			// this mode isn't ours, we have to bail and return 0 to not handle it.
			Srv->Log(DEBUG,"Extended mode event triggered, but this is not a mode i've claimed!");
			return 0;
		}
		
		chanrec* chan = (chanrec*)target;
		
		if (mode_on)
		{
			Srv->Log(DEBUG,"Custom mode is being added to channel");
			Srv->Log(DEBUG,chan->name);
		}
		else
		{
			Srv->Log(DEBUG,"Custom mode is being taken from a channel");
			Srv->Log(DEBUG,chan->name);
		}

		// must return 1 to handle the mode!
		return 1;
	}
	
	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
		Srv->Log(DEBUG,"OnUserJoin triggered");
		if (channel->IsCustomModeSet('Z'))
		{
			std::string param = channel->GetModeParameter('Z');
			Srv->Log(DEBUG,"Custom mode is set on this channel! Parameter="+param);
		}
	}
	
	virtual ~ModuleTestCommand()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleTestCommandFactory : public ModuleFactory
{
 public:
	ModuleTestCommandFactory()
	{
	}
	
	~ModuleTestCommandFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleTestCommand(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleTestCommandFactory;
}

