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
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides channel +S mode (strip ansi colour) */

class ModuleStripColor : public Module
{
 Server *Srv;
 ConfigReader *Conf, *MyConf;
 
 public:
	ModuleStripColor()
	{
		Srv = new Server;

		Srv->AddExtendedMode('S',MT_CHANNEL,false,0,0);
		Srv->AddExtendedMode('S',MT_CLIENT,false,0,0);
	}

	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if (modechar == 'S')
  		{
			return 1;
		}
		else
		{
			return 0;
		}
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
                                temp1 = "CHANMODES=" + temp1 + "S";
                        }
                        temp2 = temp2 + temp1 + " ";
                }
		if (temp2.length())
	                output = temp2.substr(0,temp2.length()-1);
        }
 	
	virtual ~ModuleStripColor()
	{
		delete Srv;
	}
	
	// ANSI colour stripping by Doc (Peter Wood)
	virtual void ReplaceLine(std::string &text)
	{
		int i, a, len, remove;
		char sentence[MAXBUF];
		strncpy(sentence,text.c_str(),MAXBUF);
  
		len = strlen (sentence);

		for (i = 0; i < len; i++)
  		{
			remove = 0;

			switch (sentence[i])
			{
				case 2:
				case 15:
				case 22:
				case 21:
				case 31:
					remove++;
				break;

				case 3:
					remove = 1;

					if (isdigit (sentence[i + remove]))
						remove++;

					if (isdigit (sentence[i + remove]))
						remove++;

					if (sentence[i + remove] == ',')
     					{
						remove += 2;

						if (isdigit (sentence[i + remove]))
						remove++;
					}
				break;
			}

			if (remove != 0) {
				len -= remove;

				for (a = i; a <= len; a++)
					sentence[a] = sentence[a + remove];
				i--;
			}
		}
		
		text = sentence;
	}
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text)
	{
		bool active = false;
		if (target_type == TYPE_USER)
		{
			userrec* t = (userrec*)dest;
			active = (strchr(t->modes,'S') > 0);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			chanrec* t = (chanrec*)dest;
			active = (t->IsCustomModeSet('S'));
		}
		if (active)
		{
			this->ReplaceLine(text);
		}
		return 0;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text)
	{
		bool active = false;
		if (target_type == TYPE_USER)
		{
			userrec* t = (userrec*)dest;
			active = (strchr(t->modes,'S') > 0);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			chanrec* t = (chanrec*)dest;
			active = (t->IsCustomModeSet('S'));
		}
		if (active)
		{
			this->ReplaceLine(text);
		}
		return 0;
	}
	
	virtual Version GetVersion()
	{
		// This is version 2 because version 1.x is the unreleased unrealircd module
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleStripColorFactory : public ModuleFactory
{
 public:
	ModuleStripColorFactory()
	{
	}
	
	~ModuleStripColorFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleStripColor;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleStripColorFactory;
}

