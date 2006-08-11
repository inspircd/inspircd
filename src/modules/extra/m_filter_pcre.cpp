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
#include "inspircd.h"

class FilterPCREException : public ModuleException
{
 public:
	virtual const char* GetReason()
	{
		return "Could not find <filter file=\"\"> definition in your config file!";
	}
};

/* $ModDesc: m_filter with regexps */
/* $CompileFlags: -I`pcre-config --prefix`/include */
/* $LinkerFlags: `pcre-config --libs` `perl extra/pcre_rpath.pl` -lpcre */

class ModuleFilterPCRE : public Module
{
	class Filter
	{
	 public:
		pcre* regexp;
	 	std::string reason;
		std::string action;
		
		Filter(pcre* r, const std::string &rea, const std::string &act)
		: regexp(r), reason(rea), action(act)
		{
		}
	};
	
	InspIRCd* Srv;
	std::vector<Filter> filters;
	pcre *re;
	const char *error;
	int erroffset;
 
 public:
	ModuleFilterPCRE(InspIRCd* Me)
	: Module::Module(Me), Srv(Me)
	{
		OnRehash("");
	}
	
	virtual ~ModuleFilterPCRE()
	{
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
			Filter& filt = filters[index];
			
			if (pcre_exec(filt.regexp,NULL,text.c_str(),text.length(),0,0,NULL,0) > -1)
			{
				const char* target;
				
				if(filt.action.empty())
					filt.action = "none";
					
				if (target_type == TYPE_USER)
				{
					userrec* t = (userrec*)dest;
					target = t->nick;
				}
				else if (target_type == TYPE_CHANNEL)
				{
					chanrec* t = (chanrec*)dest;
					target = t->name;
				}
				else
				{
					target = "";
				}
				
				log(DEFAULT, "Filter: %s had their notice filtered, target was %s: %s Action: %s", user->nick, target, filt.reason.c_str(), filt.action.c_str());
				
				if (filt.action == "block")
				{	
					Srv->WriteOpers("Filter: %s had their notice filtered, target was %s: %s", user->nick, target, filt.reason.c_str());
					user->WriteServ("NOTICE "+std::string(user->nick)+" :Your notice has been filtered and opers notified: "+filt.reason);
    			}
				else if (filt.action == "kill")
				{
					userrec::QuitUser(Srv, user, filt.reason);
				}
				
				return 1;
			}
		}
		return 0;
	}
	
	virtual void OnRehash(const std::string &parameter)
	{
		/* Read the configuration file on startup and rehash.
		 * It is perfectly valid to set <filter file> to the value of the
		 * main config file, then append your <keyword> tags to the bottom
		 * of the main config... but rather messy. That's why the capability
		 * of using a seperate config file is provided.
		 */
		
		ConfigReader Conf(Srv);
		
		std::string filterfile = Conf.ReadValue("filter", "file", 0);
		
		ConfigReader MyConf(Srv, filterfile);
		
		if (filterfile.empty() || !MyConf.Verify())
		{
			FilterPCREException e;
			throw(e);
		}
		
		log(DEFAULT,"m_filter_pcre: read configuration from "+filterfile);

		filters.clear();
		
		for (int index = 0; index < MyConf.Enumerate("keyword"); index++)
		{
			std::string pattern = MyConf.ReadValue("keyword","pattern",index);
			std::string reason = MyConf.ReadValue("keyword","reason",index);
			std::string action = MyConf.ReadValue("keyword","action",index);
			
			re = pcre_compile(pattern.c_str(),0,&error,&erroffset,NULL);
			
			if (!re)
			{
				log(DEFAULT,"Error in regular expression: %s at offset %d: %s\n", pattern.c_str(), erroffset, error);
				log(DEFAULT,"Regular expression %s not loaded.", pattern.c_str());
			}
			else
			{
				filters.push_back(Filter(re, reason, action));
				log(DEFAULT,"Regular expression %s loaded.", pattern.c_str());
			}
		}
	}
	
	virtual Version GetVersion()
	{
		/* Version 1.x is the unreleased unrealircd module */
		return Version(3,2,0,0,VF_VENDOR);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleFilterPCRE(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleFilterPCREFactory;
}
