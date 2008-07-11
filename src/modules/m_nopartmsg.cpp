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
		return Version(1,2,0,0,VF_VENDOR,API_VERSION);
	}


    virtual void OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent)
	{
		if (!IS_LOCAL(user))
			return;

		if (channel->IsExtBanned(user, 'p'))
			partmessage = "";

		return;
	}

	virtual void On005Numeric(std::string &output)
	{
		if (output.find(" EXTBAN=:") == std::string::npos)
			output.append(" EXTBAN=:p");
		else
			output.insert(output.find(" EXTBAN=:") + 9, "p");
	}
};


MODULE_INIT(ModulePartMsgBan)

