/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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
	ModuleSeeNicks(InspIRCd* Me)
		: Module(Me)
	{
		ServerInstance->SNO->EnableSnomask('n',"NICK");
		ServerInstance->SNO->EnableSnomask('N',"REMOTENICK");
		Implementation eventlist[] = { I_OnUserPostNick };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModuleSeeNicks()
	{
		ServerInstance->SNO->DisableSnomask('n');
		ServerInstance->SNO->DisableSnomask('N');
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}


	virtual void OnUserPostNick(User* user, const std::string &oldnick)
	{
		ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'n' : 'N',"User %s changed their nickname to %s", oldnick.c_str(), user->nick.c_str());
	}
};

MODULE_INIT(ModuleSeeNicks)
