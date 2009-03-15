/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
	ModuleSpy(InspIRCd* Me) : Module(Me)
	{
		ServerInstance->Modules->Attach(I_OnUserList, this);
	}

	virtual int OnUserList(User* user, Channel* Ptr, CUList* &nameslist)
	{
		/* User has priv and is NOT on the channel */
		if (user->HasPrivPermission("channels/auspex") && !Ptr->HasUser(user))
			return -1;

		return 0;
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
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSpy)

