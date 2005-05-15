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
#include <sstream>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for unreal-style channel mode +c */

class ModuleBlockColor : public Module
{
	Server *Srv;
 public:
 
	ModuleBlockColor()
	{
		Srv = new Server;
		Srv->AddExtendedMode('c',MT_CHANNEL,false,0,0);
	}

        virtual void On005Numeric(std::string &output)
        {
                // we don't really have a limit...
		std::stringstream line(output);
		std::string temp1, temp2;
		while (!line.eof())
		{
			line >> temp1;
			if (temp1.substr(0,10) == "CHANMODES=")
			{
				// append the chanmode to the end
				temp1 = temp1.substr(10,temp1.length());
				temp1 = "CHANMODES=" + temp1 + "c";
			}
			temp2 = temp2 + temp1 + " ";
		}
		if (temp2.length())
			output = temp2.substr(0,temp2.length()-1);
        }
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			char ctext[MAXBUF];
			snprintf(ctext,MAXBUF,"%s",text.c_str());
			if (c->IsCustomModeSet('c'))
			{
				if ((strchr(ctext,'\2')) || (strchr(ctext,'\3')) || (strchr(ctext,31)))
				{
					WriteServ(user->fd,"404 %s %s :Can't send colors to channel (+c set)",user->nick, c->name);
					return 1;
				}
			}
		}
		return 0;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			char ctext[MAXBUF];
			snprintf(ctext,MAXBUF,"%s",text.c_str());
			if (c->IsCustomModeSet('c'))
			{
				if ((strchr(ctext,'\2')) || (strchr(ctext,'\3')) || (strchr(ctext,31)))
				{
					WriteServ(user->fd,"404 %s %s :Can't send colors to channel (+c set)",user->nick, c->name);
					return 1;
				}
			}
		}
		return 0;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'c') && (type == MT_CHANNEL))
  		{
  			log(DEBUG,"Allowing c change");
			return 1;
		}
		else
		{
			return 0;
		}
	}

	virtual ~ModuleBlockColor()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleBlockColorFactory : public ModuleFactory
{
 public:
	ModuleBlockColorFactory()
	{
	}
	
	~ModuleBlockColorFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleBlockColor;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleBlockColorFactory;
}

