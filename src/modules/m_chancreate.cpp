/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
	void init()
	{
		ServerInstance->SNO->EnableSnomask('j', "CHANCREATE");
		ServerInstance->SNO->EnableSnomask('J', "REMOTECHANCREATE");
		Implementation eventlist[] = { I_OnUserJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	~ModuleChanCreate()
	{
		ServerInstance->SNO->DisableSnomask('j');
		ServerInstance->SNO->DisableSnomask('J');
	}

	Version GetVersion()
	{
		return Version("Creates a snomask with notices whenever a new channel is created",VF_VENDOR);
	}


	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except)
	{
		if (created)
		{
			ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(memb->user) ? 'j' : 'J', "Channel %s created by %s!%s@%s",
				memb->chan->name.c_str(), memb->user->nick.c_str(), memb->user->ident.c_str(), memb->user->host.c_str());
		}
	}
};

MODULE_INIT(ModuleChanCreate)
