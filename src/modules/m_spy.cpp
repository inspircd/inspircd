/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides the ability to see the complete names list of channels an oper is not a member of */

#include "inspircd.h"

class ModuleSpy : public Module
{
 public:
	ModuleSpy() 	{
		ServerInstance->Modules->Attach(I_OnUserList, this);
	}

	virtual ModResult OnUserList(User* user, Channel* Ptr)
	{
		/* User has priv and is NOT on the channel */
		if (user->HasPrivPermission("channels/auspex") && !Ptr->HasUser(user))
			return MOD_RES_ALLOW;

		return MOD_RES_PASSTHRU;
	}

	void Prioritize()
	{
		/* To ensure that we get priority over namesx and delayjoin for names list generation */
		Module* list[] = { ServerInstance->Modules->Find("m_namesx.so"), ServerInstance->Modules->Find("m_delayjoin.so") };
		ServerInstance->Modules->SetPriority(this, I_OnUserList, PRIORITY_BEFORE, list, 2);
	}

	virtual ~ModuleSpy()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides the ability to see the complete names list of channels an oper is not a member of", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSpy)

