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
		return Version("$Id$", VF_COMMON|VF_VENDOR, API_VERSION);
	}


	virtual void OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent)
	{
		if (!IS_LOCAL(user))
			return;

		if (channel->GetExtBanStatus(user, 'p') < 0)
			partmessage = "";

		return;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('p');
	}
};


MODULE_INIT(ModulePartMsgBan)

