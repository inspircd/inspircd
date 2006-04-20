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
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides channel +S mode (strip ansi colour) */

class ModuleStripColor : public Module
{
 Server *Srv;
 ConfigReader *Conf, *MyConf;
 
 public:
	ModuleStripColor(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;

		Srv->AddExtendedMode('S',MT_CHANNEL,false,0,0);
		Srv->AddExtendedMode('S',MT_CLIENT,false,0,0);
	}

	void Implements(char* List)
	{
		List[I_OnExtendedMode] = List[I_On005Numeric] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
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
		InsertMode(output,"S",4);
	}
 	
	virtual ~ModuleStripColor()
	{
	}
	
	// ANSI colour stripping by Doc (Peter Wood)
	virtual void ReplaceLine(std::string &text)
	{
		int i, a, len, remove;
		char sentence[MAXBUF];
		strlcpy(sentence,text.c_str(),MAXBUF);
  
		len = strlen(sentence);

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

					if (isdigit(sentence[i + remove]))
						remove++;

					if (isdigit(sentence[i + remove]))
						remove++;

					if (sentence[i + remove] == ',')
     					{
						remove += 2;

						if (isdigit(sentence[i + remove]))
							remove++;
					}
				break;
			}

			if (remove != 0)
			{
				len -= remove;

				for (a = i; a <= len; a++)
					sentence[a] = sentence[a + remove];
				i--;
			}
		}
		
		text = sentence;
	}
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
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
			active = (t->IsModeSet('S'));
		}
		if (active)
		{
			this->ReplaceLine(text);
		}
		return 0;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreMessage(user,dest,target_type,text,status);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleStripColor(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleStripColorFactory;
}

