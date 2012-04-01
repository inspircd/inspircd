/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModAuthor: Shawn Smith */
/* $ModDesc: Sends server notices when a user joins/parts a channel. */

class ModuleJoinPartSNO : public Module
{
 public:
	void init()
	{
		ServerInstance->SNO->EnableSnomask('e', "JOIN");
		ServerInstance->SNO->EnableSnomask('p', "PART");
		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPart };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Creates SNOMask for user joins/parts", VF_VENDOR);
	}


	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except)
	{
		if (IS_LOCAL(memb->user))
		{
			ServerInstance->SNO->WriteToSnoMask('e', "User %s!%s@%s joined %s",
				memb->user->nick.c_str(), memb->user->ident.c_str(), memb->user->host.c_str(), memb->chan->name.c_str());
		}
		else
		{
			ServerInstance->SNO->WriteGlobalSno('E', "User %s!%s@%s joined %s",
				memb->user->nick.c_str(), memb->user->ident.c_str(), memb->user->host.c_str(), memb->chan->name.c_str());
		}
	}

	void OnUserPart(Membership* memb, std::string &partmessage, CUList &except)
	{
		if (IS_LOCAL(memb->user))
		{
			ServerInstance->SNO->WriteToSnoMask('p', "User %s!%s@%s parted %s",
				memb->user->nick.c_str(), memb->user->ident.c_str(), memb->user->host.c_str(), memb->chan->name.c_str());
		}
		else
		{
			ServerInstance->SNO->WriteGlobalSno('P', "User %s!%s@%s parted %s",
				memb->user->nick.c_str(), memb->user->ident.c_str(), memb->user->host.c_str(), memb->chan->name.c_str());
		}
	}
};

MODULE_INIT(ModuleJoinPartSNO)
