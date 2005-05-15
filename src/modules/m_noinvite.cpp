/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for unreal-style channel mode +V */

class ModuleNoInvite : public Module
{
	Server *Srv;
	
 public:
 
	ModuleNoInvite()
	{
		Srv = new Server;
		Srv->AddExtendedMode('V',MT_CHANNEL,false,0,0);
	}

        virtual void On005Numeric(std::string &output)
        {
                std::stringstream line(output);
                std::string temp1, temp2;
                while (!line.eof())
                {
                        line >> temp1;
                        if (temp1.substr(0,10) == "CHANMODES=")
                        {
                                // append the chanmode to the end
                                temp1 = temp1.substr(10,temp1.length());
                                temp1 = "CHANMODES=" + temp1 + "V";
                        }
                        temp2 = temp2 + temp1 + " ";
                }
		if (temp2.length())
	                output = temp2.substr(0,temp2.length()-1);
        }


	virtual int OnUserPreInvite(userrec* user,userrec* dest,chanrec* channel)
	{
		if (channel->IsCustomModeSet('V'))
		{
			WriteServ(user->fd,"492 %s %s :Can't invite %s to channel (+V set)",user->nick, channel->name, dest->nick);
			return 1;
		}
		return 0;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'V') && (type == MT_CHANNEL))
  		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	virtual ~ModuleNoInvite()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleNoInviteFactory : public ModuleFactory
{
 public:
	ModuleNoInviteFactory()
	{
	}
	
	~ModuleNoInviteFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleNoInvite;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoInviteFactory;
}

