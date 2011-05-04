/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Implements extban +b/+e i: - invite requirements/exemptions */

class ModuleInviteExtban : public Module
{
public:
	ModuleInviteExtban()
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_On005Numeric, I_OnCheckJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('i');
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if(!join.chan)
			return;
		join.needs_invite = !join.chan->GetExtBanStatus(join.user, 'i').check(!join.needs_invite);
	}

	Version GetVersion()
	{
		return Version("Implements extban +b/+e i: - invite requirements/exemptions", VF_OPTCOMMON|VF_VENDOR);
	}
};

MODULE_INIT(ModuleInviteExtban)
