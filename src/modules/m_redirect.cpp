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

/* $ModDesc: Provides channel mode +L (limit redirection) */


class ModuleRedirect : public Module
{
	Server *Srv;
	
 public:
 
	ModuleRedirect()
	{
		Srv = new Server;
		Srv->AddExtendedMode('L',MT_CHANNEL,false,1,0);
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		if ((modechar == 'L') && (type == MT_CHANNEL))
		{
			if (mode_on)
			{
				std::string ChanToJoin = params[0];
				chanrec* c = Srv->FindChannel(ChanToJoin);
				if (c)
				{
					if (c->IsCustomModeSet('L'))
					{
						WriteServ(user->fd,"690 %s :Circular redirection, mode +L to %s not allowed.",user->nick,params[0].c_str());
     						return 0;
					}
				}
			}
			return 1;
		}
		return 0;
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
				// By doing this we're *assuming* no other module has fucked up the CHANMODES=
				// section of the 005 numeric. If they have, we're going DOWN in a blaze of glory,
				// with a honking great EXCEPTION :)
				temp1.insert(temp1.find(",")+1,"L");
                        }
                        temp2 = temp2 + temp1 + " ";
                }
		if (temp2.length())
	                output = temp2.substr(0,temp2.length()-1);
        }
	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (chan)
		{
			if (chan->IsCustomModeSet('L'))
			{
				if (chan->limit >= Srv->CountUsers(chan))
				{
					std::string channel = chan->GetModeParameter('L');
					WriteServ(user->fd,"470 %s :%s has become full, so you are automatically being transferred to the linked channel %s",user->nick,cname,channel.c_str());
					Srv->JoinUserToChannel(user,channel.c_str(),"");
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleRedirect()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
	}

};


class ModuleRedirectFactory : public ModuleFactory
{
 public:
	ModuleRedirectFactory()
	{
	}
	
	~ModuleRedirectFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleRedirect;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleRedirectFactory;
}

