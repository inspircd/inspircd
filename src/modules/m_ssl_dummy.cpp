/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *   E-mail:
 *	<brain@chatspike.net>
 *	<Craig@chatspike.net>
 * <omster@gmail.com>
 * 
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "users.h"
#include "modules.h"


/* $ModDesc: Makes remote /whoises to SSL servers work on a non-ssl server */

class ModuleSSLDummy : public Module
{
	
	char* dummy;
 public:
	
	ModuleSSLDummy(InspIRCd* Me)	: Module::Module(Me)
	{
		
	}
	
	virtual ~ModuleSSLDummy()
	{
	}
		
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnSyncUserMetaData] = List[I_OnDecodeMetaData] = List[I_OnWhois] = 1;
	}

	// :kenny.chatspike.net 320 Om Epy|AFK :is a Secure Connection
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		if(dest->GetExt("ssl", dummy))
		{
			source->WriteServ("320 %s %s :is using a secure connection", source->nick, dest->nick);
		}
	}
	
	virtual void OnSyncUserMetaData(userrec* user, Module* proto, void* opaque, const std::string &extname)
	{
		// check if the linking module wants to know about OUR metadata
		if(extname == "ssl")
		{
			// check if this user has an ssl field to send
			if(user->GetExt(extname, dummy))
			{
				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, "ON");
			}
		}
	}
	
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_USER) && (extname == "ssl"))
		{
			userrec* dest = (userrec*)target;
			// if they dont already have an ssl flag, accept the remote server's
			if (!dest->GetExt(extname, dummy))
			{
				dest->Extend(extname, "ON");
			}
		}
	}
};

class ModuleSSLDummyFactory : public ModuleFactory
{
 public:
	ModuleSSLDummyFactory()
	{
	}
	
	~ModuleSSLDummyFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSSLDummy(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleSSLDummyFactory;
}

