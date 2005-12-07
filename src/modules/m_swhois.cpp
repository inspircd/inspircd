/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
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
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides the SWHOIS command which allows setting of arbitary WHOIS lines */

Server *Srv;

void handle_swhois(char **parameters, int pcnt, userrec *user)
{
	userrec* dest = Srv->FindNick(std::string(parameters[0]));
	if (dest)
	{
		std::string line = "";
		for (int i = 1; i < pcnt; i++)
		{
			if (i != 1)
				line = line + " ";
			line = line + std::string(parameters[i]);
		}
		char* field = dest->GetExt("swhois");
		if (field)
		{
			std::string* text = (std::string*)field;
			dest->Shrink("swhois");
			delete text;
		}
		std::string* text = new std::string(line);
		dest->Extend("swhois",(char*)text);
	}
}

class ModuleSWhois : public Module
{
 public:
	ModuleSWhois(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Srv->AddCommand("SWHOIS",handle_swhois,'o',2,"m_swhois.so");
	}

	// :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		char* desc = dest->GetExt("swhois");
		if (desc)
		{
			std::string* swhois = (std::string*)desc;
			WriteServ(source->fd,"320 %s %s :%s",source->nick,dest->nick,swhois->c_str());
		}
	}

	// Whenever the linking module wants to send out data, but doesnt know what the data
	// represents (e.g. it is metadata, added to a userrec or chanrec by a module) then
	// this method is called. We should use the ProtoSendMetaData function after we've
	// corrected decided how the data should look, to send the metadata on its way if
	// it is ours.
	virtual void OnSyncUserMetaData(userrec* user, Module* proto, void* opaque, std::string extname)
	{
		// check if the linking module wants to know about OUR metadata
		if (extname == "swhois")
		{
			// check if this user has an swhois field to send
			char* field = user->GetExt("swhois");
			if (field)
			{
				// get our extdata out with a cast
				std::string* swhois = (std::string*)field;
				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque,TYPE_USER,user,extname,*swhois);
			}
		}
	}

	// when a user quits, tidy up their metadata
	virtual void OnUserQuit(userrec* user, std::string message)
	{
		char* field = user->GetExt("swhois");
		if (field)
		{
			std::string* swhois = (std::string*)field;
			user->Shrink("swhois");
			delete swhois;
		}
	}

	// if the module is unloaded, tidy up all our dangling metadata
	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			char* field = user->GetExt("swhois");
			if (field)
			{
				std::string* swhois = (std::string*)field;
				user->Shrink("swhois");
				delete swhois;
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
	virtual void OnDecodeMetaData(int target_type, void* target, std::string extname, std::string extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_USER) && (extname == "swhois"))
		{
			userrec* dest = (userrec*)target;
			// if they dont already have an swhois field, accept the remote server's
			if (!dest->GetExt("swhois"))
			{
				std::string* text = new std::string(extdata);
				dest->Extend("swhois",(char*)text);
			}
		}
	}
	
	virtual ~ModuleSWhois()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
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

