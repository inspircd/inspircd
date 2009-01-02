/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Implements extban +b s: - server name bans */

class ModuleServerBan : public Module
{
 private:
 public:
	ModuleServerBan(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnUserPreJoin, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleServerBan()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

	virtual int OnUserPreJoin(User *user, Channel *c, const char *cname, std::string &privs, const std::string &key)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (!c)
			return 0;

		if (c->IsExtBanned(user->server, 's'))
		{
			user->WriteNumeric(ERR_BANNEDFROMCHAN, "%s %s :Cannot join channel (You're banned)", user->nick.c_str(),  c->name.c_str());
			return 1;
		}

		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('s');
	}
};


MODULE_INIT(ModuleServerBan)

