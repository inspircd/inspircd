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

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: A dummy module for testing */

// Class ModuleFoobar inherits from Module
// It just outputs simple debug strings to show its methods are working.

class ModuleFoobar : public Module
{
 private:
	 
	 // It is recommended that your class makes use of one or more Server
	 // objects. A server object is a class which contains methods which
	 // encapsulate the exports from the core of the ircd.
	 // such methods include Debug, SendChannel, etc.
 
	 Server *Srv;
 public:
	ModuleFoobar(Server* Me)
		: Module::Module(Me)
	{
		// The constructor just makes a copy of the server class
	
		Srv = Me;
	}
	
	virtual ~ModuleFoobar()
	{
	}
	
	virtual Version GetVersion()
	{
		// this method instantiates a class of type Version, and returns
		// the modules version information using it.
	
		return Version(1,0,0,1,VF_VENDOR);
	}

	void Implements(char* List)
	{
		List[I_OnUserConnect] = List[I_OnUserQuit] = List[I_OnUserJoin] = List[I_OnUserPart] = 1;
	}
	
	virtual void OnUserConnect(userrec* user)
	{
		// method called when a user connects
	
		std::string b = user->nick;
		Srv->Log(DEBUG,"Foobar: User connecting: " + b);
	}

	virtual void OnUserQuit(userrec* user, std::string reason)
	{
		// method called when a user disconnects
	
		std::string b = user->nick;
		Srv->Log(DEBUG,"Foobar: User quitting: " + b);
	}
	
	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
		// method called when a user joins a channel
	
		std::string c = channel->name;
		std::string b = user->nick;
		Srv->Log(DEBUG,"Foobar: User " + b + " joined " + c);
	}

	virtual void OnUserPart(userrec* user, chanrec* channel)
	{
		// method called when a user parts a channel
	
		std::string c = channel->name;
		std::string b = user->nick;
		Srv->Log(DEBUG,"Foobar: User " + b + " parted " + c);
	}

};


//
// The ModuleFoobarFactory class inherits from ModuleFactory
// and creates a ModuleFoobar object when requested.
//

class ModuleFoobarFactory : public ModuleFactory
{
 public:
	ModuleFoobarFactory()
	{
	}
	
	~ModuleFoobarFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleFoobar(Me);
	}
	
};


//
// The "C" linkage factory0() function creates the ModuleFoobarFactory
// class for this library
//

extern "C" void * init_module( void )
{
	return new ModuleFoobarFactory;
}

