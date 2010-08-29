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

/* $ModDesc: Provides support for seeing local and remote nickchanges via snomasks */

class ModuleSeeNicks : public Module
{
 public:
	void init()
	{
		ServerInstance->SNO->EnableSnomask('n',"NICK");
		Implementation eventlist[] = { I_OnUserPostNick };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for seeing local and remote nickchanges via snomasks", VF_VENDOR);
	}

	virtual void OnUserPostNick(User* user, const std::string &oldnick)
	{
		ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'n' : 'N',"User %s changed their nickname to %s", oldnick.c_str(), user->nick.c_str());
	}
};

MODULE_INIT(ModuleSeeNicks)
