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

/* $ModDesc: Provides user and channel +G mode */

class ModuleCensor : public Module
{
 Server *Srv;
 ConfigReader *Conf, *MyConf;
 
 public:
	ModuleCensor()
	{
		// read the configuration file on startup.
		// it is perfectly valid to set <censor file> to the value of the
		// main config file, then append your <badword> tags to the bottom
		// of the main config... but rather messy. That's why the capability
		// of using a seperate config file is provided.
		Srv = new Server;
		Conf = new ConfigReader;
		std::string Censorfile = Conf->ReadValue("censor","file",0);
		MyConf = new ConfigReader(Censorfile);
		if ((Censorfile == "") || (!MyConf->Verify()))
		{
			printf("Error, could not find <censor file=\"\"> definition in your config file!");
			exit(0);
		}
		Srv->Log(DEFAULT,std::string("m_censor: read configuration from ")+Censorfile);
		Srv->AddExtendedMode('G',MT_CHANNEL,false,0,0);
		Srv->AddExtendedMode('G',MT_CLIENT,false,0,0);
	}

	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if (modechar == 'G')
  		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
 	
	virtual ~ModuleCensor()
	{
		delete Srv;
		delete MyConf;
		delete Conf;
	}
	
	virtual void ReplaceLine(std::string &text,std::string pattern, std::string replace)
	{
		if ((pattern != "") && (text != ""))
		{
			while (strstr(text.c_str(),pattern.c_str()))
			{
				int pos = text.find(pattern);
				text.erase(pos,pattern.length());
				text.insert(pos,replace);
			}
		}
	}
	
	// format of a config entry is <badword text="shit" replace="poo">
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text)
	{
		bool active = false;
		for (int index = 0; index < MyConf->Enumerate("badword"); index++)
		{
			std::string pattern = MyConf->ReadValue("badword","text",index);
			if (strstr(text.c_str(),pattern.c_str()))
			{
				std::string replace = MyConf->ReadValue("badword","replace",index);

				if (target_type == TYPE_USER)
				{
					userrec* t = (userrec*)dest;
					active = (strchr(t->modes,'G') > 0);
				}
				else if (target_type == TYPE_CHANNEL)
				{
					chanrec* t = (chanrec*)dest;
					active = (t->IsCustomModeSet('G'));
				}
				
				if (active)
				{
					this->ReplaceLine(text,pattern,replace);
				}
				
				return 0;
			}
		}
		return 0;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text)
	{
		bool active = false;
		for (int index = 0; index < MyConf->Enumerate("badword"); index++)
		{
			std::string pattern = MyConf->ReadValue("badword","text",index);
			if (strstr(text.c_str(),pattern.c_str()))
			{
				std::string replace = MyConf->ReadValue("badword","replace",index);

				if (target_type == TYPE_USER)
				{
					userrec* t = (userrec*)dest;
					active = (strchr(t->modes,'G') > 0);
				}
				else if (target_type == TYPE_CHANNEL)
				{
					chanrec* t = (chanrec*)dest;
					active = (t->IsCustomModeSet('G'));
				}
				
				if (active)
				{
					this->ReplaceLine(text,pattern,replace);
				}
				
				return 0;
			}
		}
		return 0;
	}
	
	virtual void OnRehash()
	{
		// reload our config file on rehash - we must destroy and re-allocate the classes
		// to call the constructor again and re-read our data.
		delete Conf;
		delete MyConf;
		Conf = new ConfigReader;
		std::string Censorfile = Conf->ReadValue("censor","file",0);
		// this automatically re-reads the configuration file into the class
		MyConf = new ConfigReader(Censorfile);
		if ((Censorfile == "") || (!MyConf->Verify()))
		{
			// bail if the user forgot to create a config file
			printf("Error, could not find <censor file=\"\"> definition in your config file!");
			exit(0);
		}
		Srv->Log(DEFAULT,std::string("m_censor: read configuration from ")+Censorfile);
	}
	
	virtual Version GetVersion()
	{
		// This is version 2 because version 1.x is the unreleased unrealircd module
		return Version(1,0,0,0);
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
	
	virtual Module * CreateModule()
	{
		return new ModuleCensor;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleCensorFactory;
}

