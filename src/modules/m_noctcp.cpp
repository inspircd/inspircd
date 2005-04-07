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

/* $ModDesc: Provides support for unreal-style channel mode +c */

class ModuleNoCTCP : public Module
{
	Server *Srv;
	
 public:
 
	ModuleNoCTCP()
	{
		Srv = new Server;
		Srv->AddExtendedMode('C',MT_CHANNEL,false,0,0);
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
                                temp1 = "CHANMODES=" + temp1 + "C";
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
			if (c->IsCustomModeSet('C'))
			{
				if ((text.length()) && (text[0] == '\1'))
				{
					if (strncmp(text.c_str(),"\1ACTION ",8))
					{
						WriteServ(user->fd,"492 %s %s :Can't send CTCP to channel (+C set)",user->nick, c->name);
						return 1;
					}
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
			if (c->IsCustomModeSet('C'))
			{
				if ((text.length()) && (text[0] == '\1'))
				{
					if (strncmp(text.c_str(),"\1ACTION ",8))
					{
						WriteServ(user->fd,"492 %s %s :Can't send CTCP to channel (+C set)",user->nick, c->name);
						return 1;
					}
				}
			}
		}
		return 0;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'C') && (type == MT_CHANNEL))
  		{
  			log(DEBUG,"Allowing C change");
			return 1;
		}
		else
		{
			return 0;
		}
	}

	virtual ~ModuleNoCTCP()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleNoCTCPFactory : public ModuleFactory
{
 public:
	ModuleNoCTCPFactory()
	{
	}
	
	~ModuleNoCTCPFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleNoCTCP;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoCTCPFactory;
}

