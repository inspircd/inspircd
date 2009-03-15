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

/* $ModDesc: Prevents users who's nicks are not registered from creating new channels */

class ModuleRegOnlyCreate : public Module
{
 public:
	ModuleRegOnlyCreate(InspIRCd* Me)
		: Module(Me)
	{
		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
			return 0;

		if (IS_OPER(user))
			return 0;

		if ((!user->IsModeSet('r')) && (!user->GetExt("accountname")))
		{
			// XXX. there may be a better numeric for this..
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must have a registered nickname to create a new channel", user->nick.c_str(), cname);
			return 1;
		}

		return 0;
	}

	virtual ~ModuleRegOnlyCreate()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleRegOnlyCreate)
