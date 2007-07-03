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

/* $ModDesc: Prevents users who's nicks are not registered from creating new channels */

class ModuleRegOnlyCreate : public Module
{
 public:
	ModuleRegOnlyCreate(InspIRCd* Me)
		: Module(Me)
	{
	}

	void Implements(char* List)
	{
		List[I_OnUserPreJoin] = 1;
	}

	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname, std::string &privs)
	{
		if (chan)
			return 0;

		if (IS_OPER(user))
			return 0;

		if ((!user->IsModeSet('r')) && (!user->GetExt("accountname")))
		{
			user->WriteServ("482 %s %s :You must have a registered nickname to create a new channel", user->nick, cname);
			return 1;
		}

		return 0;
	}
	
	virtual ~ModuleRegOnlyCreate()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleRegOnlyCreate)
