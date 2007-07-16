/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
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
 
	 
 public:
	ModuleFoobar(InspIRCd* Me)
		: Module(Me)
	{
		// The constructor just makes a copy of the server class
	
		
	}
	
	virtual ~ModuleFoobar()
	{
	}
	
	virtual Version GetVersion()
	{
		// this method instantiates a class of type Version, and returns
		// the modules version information using it.
	
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnUserConnect] = List[I_OnUserQuit] = List[I_OnUserJoin] = List[I_OnUserPart] = 1;
	}
	
	virtual void OnUserConnect(userrec* user)
	{
		// method called when a user connects
	
		std::string b = user->nick;
		ServerInstance->Log(DEBUG,"Foobar: User connecting: "+b);
	}

	virtual void OnUserQuit(userrec* user, const std::string &reason, const std::string &oper_message)
	{
		// method called when a user disconnects
	
		std::string b = user->nick;
		ServerInstance->Log(DEBUG,"Foobar: User quitting: "+b);
	}
	
	virtual void OnUserJoin(userrec* user, chanrec* channel, bool &silent)
	{
		// method called when a user joins a channel
	
		std::string c = channel->name;
		std::string b = user->nick;
		ServerInstance->Log(DEBUG,"Foobar: User "+b+" joined "+c);
	}

	virtual void OnUserPart(userrec* user, chanrec* channel, const std::string &partreason, bool &silent)
	{
		// method called when a user parts a channel
	
		std::string c = channel->name;
		std::string b = user->nick;
		ServerInstance->Log(DEBUG,"Foobar: User "+b+" parted "+c);
	}

};


MODULE_INIT(ModuleFoobar)

