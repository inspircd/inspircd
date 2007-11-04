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

/* $ModDesc: Removes flood limits from users upon opering up. */
class ModuleOperFlood : public Module
{
public:
	ModuleOperFlood(InspIRCd * Me) : Module(Me) {}


	Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	void OnPostOper(User* user, const std::string &opertype)
	{
		if(!IS_LOCAL(user))
			return;

		user->ExemptFromPenalty = true;
		user->WriteServ("NOTICE %s :*** You are now free from flood limits.", user->nick);
	}
};

MODULE_INIT(ModuleOperFlood)
