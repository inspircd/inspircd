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

/* $ModDesc: Creates a snomask with notices whenever a new channel is created */

class ModuleChanCreate : public Module
{
 private:
 public:
	ModuleChanCreate(InspIRCd* Me)
		: Module(Me)
	{
		ServerInstance->SNO->EnableSnomask('j', "CHANCREATE");
		Implementation eventlist[] = { I_OnUserJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}
	
	virtual ~ModuleChanCreate()
	{
		ServerInstance->SNO->DisableSnomask('j');
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR,API_VERSION);
	}

	
	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent)
	{
		if (channel->GetUserCounter() == 1)
		{
			ServerInstance->SNO->WriteToSnoMask('j', "Channel %s created by %s!%s@%s", channel->name, user->nick, user->ident, user->host);
		}
	}
};

MODULE_INIT(ModuleChanCreate)
