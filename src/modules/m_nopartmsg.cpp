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

/* $ModDesc: Implements extban +b p: - part message bans */

class ModulePartMsgBan : public Module
{
 private:
 public:
	ModulePartMsgBan(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnUserPart, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModulePartMsgBan()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Implements extban +b p: - part message bans", VF_COMMON|VF_VENDOR, API_VERSION);
	}


	virtual void OnUserPart(Membership* memb, std::string &partmessage, CUList& excepts)
	{
		if (!IS_LOCAL(memb->user))
			return;

		if (memb->chan->GetExtBanStatus(memb->user, 'p') == MOD_RES_DENY)
			partmessage = "";

		return;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('p');
	}
};


MODULE_INIT(ModulePartMsgBan)

