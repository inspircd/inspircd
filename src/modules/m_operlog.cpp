/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;
 
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "message.h"
#include <vector>

/* $ModDesc: A module which logs all oper commands to the ircd log at default loglevel. */

class ModuleOperLog : public Module
{
 private:
	 Server *Srv;
 public:
	ModuleOperLog(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
	}
 
	virtual ~ModuleOperLog()
	{
	}
 
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}
 
	void Implements(char* List)
	{
		List[I_OnPreCommand] = List[I_On005Numeric] = 1;
	}

	virtual int OnPreCommand(const std::string &command, char **parameters, int pcnt, userrec *user, bool validated)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return 0;
 
		if (*user->oper)
		{
			std::string plist = "";
			for (int j = 0; j < pcnt; j++)
			{
				plist.append(std::string(" ")+std::string(parameters[j]));
			}
			log(DEFAULT,"OPERLOG: [%s!%s@%s] %s%s",user->nick,user->ident,user->host,command.c_str(),plist.c_str());
		}

		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" OPERLOG");
	}

};
 
 
 
/******************************************************************************************************/
 
class ModuleOperLogFactory : public ModuleFactory
{
 public:
	ModuleOperLogFactory()
	{
	}
 
	~ModuleOperLogFactory()
	{
	}
 
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleOperLog(Me);
	}
 
};
 
extern "C" void * init_module( void )
{
	return new ModuleOperLogFactory;
}

