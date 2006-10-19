/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides channel +S mode (strip ansi colour) */

/** Handles channel mode +S
 */
class ChannelStripColor : public ModeHandler
{
 public:
	ChannelStripColor(InspIRCd* Instance) : ModeHandler(Instance, 'S', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		/* Only opers can change other users modes */
		if ((source != dest) && (!*source->oper))
			return MODEACTION_DENY;

		if (adding)
		{
			if (!channel->IsModeSet('S'))
			{
				channel->SetMode('S',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('S'))
			{
				channel->SetMode('S',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

/** Handles user mode +S
 */
class UserStripColor : public ModeHandler
{
 public:
	UserStripColor(InspIRCd* Instance) : ModeHandler(Instance, 'S', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		/* Only opers can change other users modes */
		if ((source != dest) && (!*source->oper))
			return MODEACTION_DENY;

		if (adding)
		{
			if (!dest->IsModeSet('S'))
			{
				dest->SetMode('S',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('S'))
			{
				dest->SetMode('S',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};


class ModuleStripColor : public Module
{
 
 ConfigReader *Conf, *MyConf;
 ChannelStripColor *csc;
 UserStripColor *usc;
 
 public:
	ModuleStripColor(InspIRCd* Me)
		: Module::Module(Me)
	{
		

		usc = new UserStripColor(ServerInstance);
		csc = new ChannelStripColor(ServerInstance);

		ServerInstance->AddMode(usc, 'S');
		ServerInstance->AddMode(csc, 'S');
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual ~ModuleStripColor()
	{
		ServerInstance->Modes->DelMode(usc);
		ServerInstance->Modes->DelMode(csc);
		DELETE(usc);
		DELETE(csc);
	}
	
	// ANSI colour stripping by Doc (Peter Wood)
	virtual void ReplaceLine(std::string &text)
	{
		int i, a, len, remove;
		char sentence[MAXBUF];
		strlcpy(sentence,text.c_str(),MAXBUF);
  
		len = text.length();

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
			active = t->modes['S'-65];
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
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleStripColor(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleStripColorFactory;
}

