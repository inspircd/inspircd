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

/* $ModDesc: Implements extban +b j: - matching channel bans */

class ModuleBadChannelExtban : public Module
{
 private:
 public:
	ModuleBadChannelExtban(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnUserPreJoin, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleBadChannelExtban()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,2,0,0,VF_VENDOR,API_VERSION);
	}

	virtual int OnUserPreJoin(User *user, Channel *c, const char *cname, std::string &privs, const std::string &key)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (!c)
			return 0;


		for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
		{
			if (c->IsExtBanned(i->first->name, 'j'))
			{
				// XXX: send a numeric here
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Cannot join " + c->name + ", as you match a ban");
				return 1;
			}
		}

		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('j');
	}
};


MODULE_INIT(ModuleBadChannelExtban)

