/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
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
		ServerInstance->SNO->EnableSnomask('J', "REMOTECHANCREATE");
		Implementation eventlist[] = { I_OnUserJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModuleChanCreate()
	{
		ServerInstance->SNO->DisableSnomask('j');
		ServerInstance->SNO->DisableSnomask('J');
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}


	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent)
	{
		if (channel->GetUserCounter() == 1 && !channel->IsModeSet('P'))
		{
			if (IS_LOCAL(user))
				ServerInstance->SNO->WriteToSnoMask('j', "Channel %s created by %s!%s@%s", channel->name.c_str(), user->nick.c_str(), user->ident.c_str(), user->host.c_str());
			else
				ServerInstance->SNO->WriteToSnoMask('J', "Channel %s created by %s!%s@%s", channel->name.c_str(), user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		}
	}
};

MODULE_INIT(ModuleChanCreate)
