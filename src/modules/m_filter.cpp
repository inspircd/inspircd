// Message and notice filtering using glob patterns
// a module based on the original work done by Craig Edwards in 2003
// for the chatspike network.

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: An enhanced version of the unreal m_filter.so used by chatspike.net */

	 

class ModuleFilter : public Module
{
 Server *Srv;
 ConfigReader *Conf, *MyConf;
 
 public:
	ModuleFilter()
	{
		// read the configuration file on startup.
		// it is perfectly valid to set <filter file> to the value of the
		// main config file, then append your <keyword> tags to the bottom
		// of the main config... but rather messy. That's why the capability
		// of using a seperate config file is provided.
		Srv = new Server;
		Conf = new ConfigReader;
		std::string filterfile = Conf->ReadValue("filter","file",0);
		if (filterfile == "")
		{
			printf("Error, could not find <filter file=\"\"> definition in your config file!");
			exit(0);
		}
		MyConf = new ConfigReader(filterfile);
		Srv->Log(DEFAULT,std::string("m_filter: read configuration from ")+filterfile);
	}
	
	virtual ~ModuleFilter()
	{
		delete Srv;
		delete MyConf;
		delete Conf;
	}
	
	// format of a config entry is <keyword pattern="*glob*" reason="Some reason here">
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string text)
	{
		text = text + " ";
		for (int index = 0; index < MyConf->Enumerate("keyword"); index++)
		{
			std::string pattern = MyConf->ReadValue("keyword","pattern",index);
			if (Srv->MatchText(text,pattern))
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
					Srv->SendOpers(std::string("FILTER: ")+std::string(user->nick)+
							std::string(" had their message filtered, target was ")+
							target+": "+reason);
					// this form of SendTo (with the source as NuLL) sends a server notice
					Srv->SendTo(NULL,user,"NOTICE "+std::string(user->nick)+
							" :Your message has been filtered and opers notified: "+reason);
				}

				Srv->Log(DEFAULT,std::string("FILTER: ")+std::string(user->nick)+
    						std::string(" had their message filtered, target was ")+
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
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string text)
	{
		text = text + " ";
		for (int index = 0; index < MyConf->Enumerate("keyword"); index++)
		{
			std::string pattern = MyConf->ReadValue("keyword","pattern",index);
			if (Srv->MatchText(text,pattern))
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
					Srv->SendOpers(std::string("FILTER: ")+std::string(user->nick)+
    							std::string(" had their notice filtered, target was ")+
    							target+": "+reason);
					Srv->SendTo(NULL,user,"NOTICE "+std::string(user->nick)+
    							" :Your notice has been filtered and opers notified: "+reason);
    				}
				Srv->Log(DEFAULT,std::string("FILTER: ")+std::string(user->nick)+
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
	
	virtual void OnRehash()
	{
		// reload our config file on rehash - we must destroy and re-allocate the classes
		// to call the constructor again and re-read our data.
		delete Conf;
		delete MyConf;
		Conf = new ConfigReader;
		std::string filterfile = Conf->ReadValue("filter","file",0);
		if (filterfile == "")
		{
			// bail if the user forgot to create a config file
			printf("Error, could not find <filter file=\"\"> definition in your config file!");
			exit(0);
		}
		// this automatically re-reads the configuration file into the class
		MyConf = new ConfigReader(filterfile);
		Srv->Log(DEFAULT,std::string("m_filter: read configuration from ")+filterfile);
	}
	
	virtual Version GetVersion()
	{
		// This is version 2 because version 1.x is the unreleased unrealircd module
		return Version(2,0,0,0);
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
	
	virtual Module * CreateModule()
	{
		return new ModuleFilter;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleFilterFactory;
}

