/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *				       E-mail:
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
#include "inspircd.h"

typedef std::map<irc::string,irc::string> censor_t;

/* $ModDesc: Provides user and channel +G mode */

/** Handles usermode +G
 */
class CensorUser : public ModeHandler
{
 public:
	CensorUser(InspIRCd* Instance) : ModeHandler(Instance, 'G', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		/* Only opers can change other users modes */
		if ((source != dest) && (!*source->oper))
			return MODEACTION_DENY;

		if (adding)
		{
			if (!dest->IsModeSet('G'))
			{
				dest->SetMode('G',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('G'))
			{
				dest->SetMode('G',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

/** Handles channel mode +G
 */
class CensorChannel : public ModeHandler
{
 public:
	CensorChannel(InspIRCd* Instance) : ModeHandler(Instance, 'G', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('G'))
			{
				channel->SetMode('G',false);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('G'))
			{
				channel->SetMode('G',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_ALLOW;
	}
};

class ModuleCensor : public Module
{

	
	censor_t censors;
	CensorUser *cu;
	CensorChannel *cc;
 
 public:
	ModuleCensor(InspIRCd* Me)
		: Module::Module(Me)
	{
		/* Read the configuration file on startup.
		 */
		OnRehash("");
		cu = new CensorUser(ServerInstance);
		cc = new CensorChannel(ServerInstance);
		ServerInstance->AddMode(cu, 'G');
		ServerInstance->AddMode(cc, 'G');
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual ~ModuleCensor()
	{
		ServerInstance->Modes->DelMode(cu);
		ServerInstance->Modes->DelMode(cc);
		DELETE(cu);
		DELETE(cc);
	}

	virtual void ReplaceLine(irc::string &text, irc::string pattern, irc::string replace)
	{
		if ((pattern != "") && (text != ""))
		{
			while (text.find(pattern) != irc::string::npos)
			{
				int pos = text.find(pattern);
				text.erase(pos,pattern.length());
				text.insert(pos,replace);
			}
		}
	}

	// format of a config entry is <badword text="shit" replace="poo">
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		bool active = false;
		irc::string text2 = text.c_str();
		for (censor_t::iterator index = censors.begin(); index != censors.end(); index++)
		{ 
			if (text2.find(index->first) != irc::string::npos)
			{
				if (target_type == TYPE_USER)
				{
					userrec* t = (userrec*)dest;
					active = t->IsModeSet('G');
				}
				else if (target_type == TYPE_CHANNEL)
				{
					chanrec* t = (chanrec*)dest;
					active = t->IsModeSet('G');
				}
				
				if (active)
				{
					this->ReplaceLine(text2,index->first,index->second);
					text = text2.c_str();
				}
			}
		}
		return 0;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreMessage(user,dest,target_type,text,status);
	}
	
	virtual void OnRehash(const std::string &parameter)
	{
		/*
		 * reload our config file on rehash - we must destroy and re-allocate the classes
		 * to call the constructor again and re-read our data.
		 */
		ConfigReader* MyConf = new ConfigReader(ServerInstance);
		censors.clear();
		for (int index = 0; index < MyConf->Enumerate("badword"); index++)
		{
			irc::string pattern = (MyConf->ReadValue("badword","text",index)).c_str();
			irc::string replace = (MyConf->ReadValue("badword","replace",index)).c_str();
			censors[pattern] = replace;
		}
		DELETE(MyConf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleCensorFactory : public ModuleFactory
{
 public:
	ModuleCensorFactory()
	{
	}
	
	~ModuleCensorFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleCensor(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleCensorFactory;
}
