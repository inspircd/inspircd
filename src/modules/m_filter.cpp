/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

// Message and notice filtering using glob patterns
// a module based on the original work done by Craig Edwards in 2003
// for the chatspike network.

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: An enhanced version of the unreal m_filter.so used by chatspike.net */

class Filter
{
 public:
	std::string reason;
	std::string action;
};

typedef std::map<std::string,Filter*> filter_t;

class FilterException : public ModuleException
{
 public:
	virtual const char* GetReason()
	{
		return "Could not find <filter file=\"\"> definition in your config file!";
	}
};

class ModuleFilter : public Module
{
 Server *Srv;
 filter_t filters;
 
 public:
	ModuleFilter(Server* Me)
		: Module::Module(Me)
	{
		// read the configuration file on startup.
		// it is perfectly valid to set <filter file> to the value of the
		// main config file, then append your <keyword> tags to the bottom
		// of the main config... but rather messy. That's why the capability
		// of using a seperate config file is provided.
		Srv = Me;
		OnRehash("");
	}
	
	virtual ~ModuleFilter()
	{
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnRehash] = 1;
	}
	
	// format of a config entry is <keyword pattern="*glob*" reason="Some reason here" action="kill/block">
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreNotice(user,dest,target_type,text,status);
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		std::string text2 = text+" ";
		for (filter_t::iterator index = filters.begin(); index != filters.end(); index++)
		{
			if ((Srv->MatchText(text2,index->first)) || (Srv->MatchText(text,index->first)))
			{
				Filter* f = (Filter*)index->second;
				std::string target = "";

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

				if (f->action == "block")
	      			{	
					Srv->SendOpers(std::string("FILTER: ")+std::string(user->nick)+
    							std::string(" had their notice filtered, target was ")+
    							target+": "+f->reason);
					Srv->SendTo(NULL,user,"NOTICE "+std::string(user->nick)+
    							" :Your notice has been filtered and opers notified: "+f->reason);
    				}
				Srv->Log(DEFAULT,std::string("FILTER: ")+std::string(user->nick)+
    						std::string(" had their notice filtered, target was ")+
    						target+": "+f->reason+" Action: "+f->action);

				if (f->action == "kill")
				{
					Srv->QuitUser(user,f->reason);
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
		ConfigReader* Conf = new ConfigReader;
		std::string filterfile = Conf->ReadValue("filter","file",0);
		// this automatically re-reads the configuration file into the class
		ConfigReader* MyConf = new ConfigReader(filterfile);
		if ((filterfile == "") || (!MyConf->Verify()))
		{
			// bail if the user forgot to create a config file
			FilterException e;
			throw(e);
		}
		for (filter_t::iterator n = filters.begin(); n != filters.end(); n++)
		{
			delete n->second;
		}
		filters.clear();
		for (int index = 0; index < MyConf->Enumerate("keyword"); index++)
		{
			std::string pattern = MyConf->ReadValue("keyword","pattern",index);
			std::string reason = MyConf->ReadValue("keyword","reason",index);
			std::string do_action = MyConf->ReadValue("keyword","action",index);
			if (do_action == "")
				do_action = "none";
			Filter* x = new Filter;
			x->reason = reason;
			x->action = do_action;
			filters[pattern] = x;
		}
		Srv->Log(DEFAULT,std::string("m_filter: read configuration from ")+filterfile);
		delete Conf;
		delete MyConf;
	}
	
	virtual Version GetVersion()
	{
		// This is version 2 because version 1.x is the unreleased unrealircd module
		return Version(2,0,0,2,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleFilterFactory : public ModuleFactory
{
 public:
	ModuleFilterFactory()
	{
	}
	
	~ModuleFilterFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleFilter(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleFilterFactory;
}

