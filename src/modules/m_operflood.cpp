/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
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

/* $ModDesc: Removes flood limits from users upon opering up. */
class ModuleOperFlood : public Module
{
public:
	ModuleOperFlood(InspIRCd * Me) : Module(Me)
	{
                Implementation eventlist[] = { I_OnPostOper };
                ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

	void OnPostOper(User* user, const std::string &opertype, const std::string &opername)
	{
		if(!IS_LOCAL(user))
			return;

		user->ExemptFromPenalty = true;
		user->WriteServ("NOTICE %s :*** You are now free from flood limits.", user->nick.c_str());
	}
};

MODULE_INIT(ModuleOperFlood)
