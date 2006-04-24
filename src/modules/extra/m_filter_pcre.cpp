/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2004 ChatSpike-Dev.
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

// Message and notice filtering using regex patterns
// a module based on the original work done by Craig Edwards in 2003
// for the chatspike network.

#include <stdio.h>
#include <string>
#include <pcre.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

class FilterPCREException : public ModuleException
{
 public:
	virtual const char* GetReason()
	{
		return "Could not find <filter file=\"\"> definition in your config file!";
	}
};

/* $ModDesc: m_filter with regexps */
/* $CompileFlags: -I/usr/local/include */
/* $LinkerFlags: -L/usr/local/lib -lpcre */

class ModuleFilterPCRE : public Module
{
	Server *Srv;
	ConfigReader *Conf, *MyConf;
	std::vector<pcre*> filters;
	pcre *re;
	const char *error;
	int erroffset;
 
 public:
	ModuleFilterPCRE(Server* Me)
		: Module::Module(Me)
	{
		// read the configuration file on startup.
		// it is perfectly valid to set <filter file> to the value of the
		// main config file, then append your <keyword> tags to the bottom
		// of the main config... but rather messy. That's why the capability
		// of using a seperate config file is provided.
		Srv = Me;
		Conf = new ConfigReader;
		std::string filterfile = Conf->ReadValue("filter","file",0);
		MyConf = new ConfigReader(filterfile);
		if ((filterfile == "") || (!MyConf->Verify()))
		{
			FilterPCREException e;
			throw(e);
		}
		Srv->Log(DEFAULT,std::string("m_filter_pcre: read configuration from ")+filterfile);

		filters.clear();
		for (int index = 0; index < MyConf->Enumerate("keyword"); index++)
		{
			std::string pattern = MyConf->ReadValue("keyword","pattern",index);
			re = pcre_compile(pattern.c_str(),0,&error,&erroffset,NULL);
			if (!re)
			{
				log(DEFAULT,"Error in regular expression: %s at offset %d: %s\n", pattern.c_str(), erroffset, error);
				log(DEFAULT,"Regular expression %s not loaded.", pattern.c_str());
			}
			else
			{
				filters.push_back(re);
				log(DEFAULT,"Regular expression %s loaded.", pattern.c_str());
			}
		}

	}
	
	virtual ~ModuleFilterPCRE()
	{
		DELETE(MyConf);
		DELETE(Conf);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnRehash] = 1;
	}

	// format of a config entry is <keyword pattern="^regexp$" reason="Some reason here" action="kill/block">
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreNotice(user,dest,target_type,text,status);
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		for (unsigned int index = 0; index < filters.size(); index++)
		{
			if (pcre_exec(filters[index],NULL,text.c_str(),text.length(),0,0,NULL,0) > -1)
			{
				std::string target = "";
				std::string reason = MyConf->ReadValue("keyword","reason",index);
				std::string do_action = MyConf->ReadValue("keyword","action",index);

				if (do_action == "")
					do_action = "none";
					
				if (target_type == TYPE_USER)
				{
					userrec* t = (userrec*)dest;
					target = std::string(t->nick);
				}
				else if (target_type == TYPE_CHANNEL)
				{
					chanrec* t = (chanrec*)dest;
					target = std::string(t->name);
				}
				if (do_action == "block")
	      			{	
					Srv->SendOpers(std::string("Filter: ")+std::string(user->nick)+
    							std::string(" had their notice filtered, target was ")+
    							target+": "+reason);
					Srv->SendTo(NULL,user,"NOTICE "+std::string(user->nick)+
    							" :Your notice has been filtered and opers notified: "+reason);
    				}
				Srv->Log(DEFAULT,std::string("Filter: ")+std::string(user->nick)+
    						std::string(" had their notice filtered, target was ")+
    						target+": "+reason+" Action: "+do_action);

				if (do_action == "kill")
				{
					Srv->QuitUser(user,reason);
				}
				return 1;
			}
		}
		return 0;
	}
	
	virtual void OnRehash(const std::string &parameter)
	{
		// reload our config file on rehash - we must destroy and re-allocate the classes
		// to call the constructor again and re-read our data.
		DELETE(Conf);
		DELETE(MyConf);
		Conf = new ConfigReader;
		std::string filterfile = Conf->ReadValue("filter","file",0);
		// this automatically re-reads the configuration file into the class
		MyConf = new ConfigReader(filterfile);
		if ((filterfile == "") || (!MyConf->Verify()))
		{
			FilterPCREException e;
			// bail if the user forgot to create a config file
			throw(e);
		}
		Srv->Log(DEFAULT,std::string("m_filter_pcre: read configuration from ")+filterfile);

		filters.clear();
		for (int index = 0; index < MyConf->Enumerate("keyword"); index++)
		{
			std::string pattern = MyConf->ReadValue("keyword","pattern",index);
			re = pcre_compile(pattern.c_str(),0,&error,&erroffset,NULL);
			if (!re)
			{
				log(DEFAULT,"Error in regular expression: %s at offset %d: %s\n", pattern.c_str(), erroffset, error);
				log(DEFAULT,"Regular expression %s not loaded.", pattern.c_str());
			}
			else
			{
				filters.push_back(re);
				log(DEFAULT,"Regular expression %s loaded.", pattern.c_str());
			}
		}

	}
	
	virtual Version GetVersion()
	{
		// This is version 2 because version 1.x is the unreleased unrealircd module
		return Version(3,0,0,0,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleFilterPCREFactory : public ModuleFactory
{
 public:
	ModuleFilterPCREFactory()
	{
	}
	
	~ModuleFilterPCREFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleFilterPCRE(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleFilterPCREFactory;
}

