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
#include "helperfuncs.h"
#include "inspircd.h"

typedef std::map<irc::string,irc::string> censor_t;

/* $ModDesc: Provides user and channel +G mode */

extern InspIRCd* ServerInstance;

class CensorException : public ModuleException
{
 public:
	virtual const char* GetReason()
	{
		return "Could not find <censor file=\"\"> definition in your config file!";
	}
};

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

	Server *Srv;
	censor_t censors;
	CensorUser *cu;
	CensorChannel *cc;
 
 public:
	ModuleCensor(Server* Me)
		: Module::Module(Me)
	{
		/*
		 * read the configuration file on startup.
		 * it is perfectly valid to set <censor file> to the value of the
		 * main config file, then append your <badword> tags to the bottom
		 * of the main config... but rather messy. That's why the capability
		 * of using a seperate config file is provided.
		 *
		 * XXX - Really, it'd be nice to scraip this kind of thing, and have something like
		 * an include directive to include additional configuration files. Might make our lives easier. --w00t
		 *
		 * XXX - These module pre-date the include directive which exists since beta 5 -- Brain
		 */
		Srv = Me;
		OnRehash("");
		cu = new CensorUser(ServerInstance);
		cc = new CensorChannel(ServerInstance);
		Srv->AddMode(cu, 'G');
		Srv->AddMode(cc, 'G');
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_On005Numeric] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}


	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->ModeGrok->InsertMode(output,"G",4);
	}
 
	virtual ~ModuleCensor()
	{
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
		ConfigReader* Conf = new ConfigReader;
		std::string Censorfile = Conf->ReadValue("censor","file",0);
		// this automatically re-reads the configuration file into the class
		ConfigReader* MyConf = new ConfigReader(Censorfile);
		if ((Censorfile == "") || (!MyConf->Verify()))
		{
			CensorException e;
			throw(e);
		}
		censors.clear();
		for (int index = 0; index < MyConf->Enumerate("badword"); index++)
		{
			irc::string pattern = (MyConf->ReadValue("badword","text",index)).c_str();
			irc::string replace = (MyConf->ReadValue("badword","replace",index)).c_str();
			censors[pattern] = replace;
		}
		DELETE(Conf);
		DELETE(MyConf);
	}
	
	virtual Version GetVersion()
	{
		// This is version 2 because version 1.x is the unreleased unrealircd module
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleCensor(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleCensorFactory;
}
