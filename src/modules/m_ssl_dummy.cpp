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

/* $ModDesc: Makes remote /whoises to SSL servers work on a non-ssl server */

class ModuleSSLDummy : public Module
{
	
	char* dummy;
 public:
	
	ModuleSSLDummy(InspIRCd* Me)	: Module(Me)
	{
		
		Implementation eventlist[] = { I_OnSyncUserMetaData, I_OnDecodeMetaData, I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}
	
	virtual ~ModuleSSLDummy()
	{
	}
		
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR, API_VERSION);
	}


	// :kenny.chatspike.net 320 Om Epy|AFK :is a Secure Connection
	virtual void OnWhois(User* source, User* dest)
	{
		if(dest->GetExt("ssl", dummy))
		{
			ServerInstance->SendWhoisLine(source, dest, 320, "%s %s :is using a secure connection", source->nick, dest->nick);
		}
	}
	
	virtual void OnSyncUserMetaData(User* user, Module* proto, void* opaque, const std::string &extname, bool displayable)
	{
		// check if the linking module wants to know about OUR metadata
		if(extname == "ssl")
		{
			// check if this user has an ssl field to send
			if(user->GetExt(extname, dummy))
			{
				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, displayable ? "Enabled" : "ON");
			}
		}
	}
	
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_USER) && (extname == "ssl"))
		{
			User* dest = (User*)target;
			// if they dont already have an ssl flag, accept the remote server's
			if (!dest->GetExt(extname, dummy))
			{
				dest->Extend(extname, "ON");
			}
		}
	}
};

MODULE_INIT(ModuleSSLDummy)
