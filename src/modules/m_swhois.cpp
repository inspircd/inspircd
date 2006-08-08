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

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides the SWHOIS command which allows setting of arbitary WHOIS lines */

class cmd_swhois : public command_t
{
	Server* Srv;
 public:
	cmd_swhois(Server* server) : command_t("SWHOIS",'o',2)
	{
		this->Srv = server;
		this->source = "m_swhois.so";
		syntax = "<nick> <swhois>";
	}

	void Handle(const char** parameters, int pcnt, userrec* user)
	{
		userrec* dest = Srv->FindNick(std::string(parameters[0]));
		if(dest)
		{
			std::string line;
			for(int i = 1; i < pcnt; i++)
			{
				if (i != 1)
					line.append(" ");
					
				line.append(parameters[i]);
			}
			
			std::string* text;
			dest->GetExt("swhois", text);
	
			if(text)
			{
				// We already had it set...
				
				if (!Srv->IsUlined(user->server))
					// Ulines set SWHOISes silently
					WriteOpers("*** %s used SWHOIS to set %s's extra whois from '%s' to '%s'", user->nick, dest->nick, text->c_str(), line.c_str());
				
				dest->Shrink("swhois");
				DELETE(text);
			}
			else if(!Srv->IsUlined(user->server))
			{
				// Ulines set SWHOISes silently
				WriteOpers("*** %s used SWHOIS to set %s's extra whois to '%s'", user->nick, dest->nick, line.c_str());
			}
			
			text = new std::string(line);
			dest->Extend("swhois", text);
		}
	}
};

class ModuleSWhois : public Module
{
	cmd_swhois* mycommand;
	Server* Srv;
	ConfigReader* Conf;
	
 public:
	ModuleSWhois(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		Conf = new ConfigReader();
		mycommand = new cmd_swhois(Srv);
		Srv->AddCommand(mycommand);
	}

	void OnRehash(const std::string &parameter)
	{
		DELETE(Conf);
		Conf = new ConfigReader();
	}

	void Implements(char* List)
	{
		List[I_OnWhois] = List[I_OnSyncUserMetaData] = List[I_OnUserQuit] = List[I_OnCleanup] = List[I_OnRehash] = List[I_OnOper] = 1;
	}

	// :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		std::string* swhois;
		dest->GetExt("swhois", swhois);
		if (swhois)
		{
			source->WriteServ("320 %s %s :%s",source->nick,dest->nick,swhois->c_str());
		}
	}

	// Whenever the linking module wants to send out data, but doesnt know what the data
	// represents (e.g. it is metadata, added to a userrec or chanrec by a module) then
	// this method is called. We should use the ProtoSendMetaData function after we've
	// corrected decided how the data should look, to send the metadata on its way if
	// it is ours.
	virtual void OnSyncUserMetaData(userrec* user, Module* proto, void* opaque, const std::string &extname)
	{
		// check if the linking module wants to know about OUR metadata
		if (extname == "swhois")
		{
			// check if this user has an swhois field to send
			std::string* swhois;
			user->GetExt("swhois", swhois);
			if (swhois)
			{
				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque,TYPE_USER,user,extname,*swhois);
			}
		}
	}

	// when a user quits, tidy up their metadata
	virtual void OnUserQuit(userrec* user, const std::string &message)
	{
		std::string* swhois;
		user->GetExt("swhois", swhois);
		if (swhois)
		{
			user->Shrink("swhois");
			DELETE(swhois);
		}
	}

	// if the module is unloaded, tidy up all our dangling metadata
	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			std::string* swhois;
			user->GetExt("swhois", swhois);
			if (swhois)
			{
				user->Shrink("swhois");
				DELETE(swhois);
			}
		}
	}

	// Whenever the linking module receives metadata from another server and doesnt know what
	// to do with it (of course, hence the 'meta') it calls this method, and it is up to each
	// module in turn to figure out if this metadata key belongs to them, and what they want
	// to do with it.
	// In our case we're only sending a single string around, so we just construct a std::string.
	// Some modules will probably get much more complex and format more detailed structs and classes
	// in a textual way for sending over the link.
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_USER) && (extname == "swhois"))
		{
			userrec* dest = (userrec*)target;
			// if they dont already have an swhois field, accept the remote server's
			std::string* text;
			if (!dest->GetExt("swhois", text))
			{
				std::string* text = new std::string(extdata);
				dest->Extend("swhois",text);
			}
		}
	}
	
	virtual void OnOper(userrec* user, const std::string &opertype)
	{
		for(int i =0; i < Conf->Enumerate("type"); i++)
		{
			std::string type = Conf->ReadValue("type", "name", i);
			
			if(strcmp(type.c_str(), user->oper) == 0)
			{
				std::string swhois = Conf->ReadValue("type", "swhois", i);
				
				if(swhois.length())
				{
					std::string* old;
					if(user->GetExt("swhois", old))
					{
						user->Shrink("swhois");
						DELETE(old);
					}
			
					std::string* text = new std::string(swhois);
					user->Extend("swhois", text);
					
					break;
				}
			}
		}		
	}
	
	virtual ~ModuleSWhois()
	{
		DELETE(Conf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}
};


class ModuleSWhoisFactory : public ModuleFactory
{
 public:
	ModuleSWhoisFactory()
	{
	}
	
	~ModuleSWhoisFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSWhois(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSWhoisFactory;
}

