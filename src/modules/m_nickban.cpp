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

/* $ModDesc: Implements extban +b n: - nick change bans */

class ModuleNickBan : public Module
{
 private:
 public:
	ModuleNickBan(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnUserPreNick, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}
	
	virtual ~ModuleNickBan()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,2,0,0,VF_VENDOR,API_VERSION);
	}

	virtual int OnUserPreNick(User *user, const std::string &newnick)
	{
		if (!IS_LOCAL(user))
			return 0;

		for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
		{
			if (i->first->IsExtBanned(user, 'n'))
			{
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Cannot change nick on " + i->first->name + ", as you match a nickname-change ban");
				return 1;
			}
		}

		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		if (output.find(" EXTBAN=:") == std::string::npos)
			output.append(" EXTBAN=:n");
		else
			output.insert(output.find(" EXTBAN=:") + 9, "n");
	}
};


MODULE_INIT(ModuleNickBan)

