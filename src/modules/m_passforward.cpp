 /*       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "command_parse.h"

class ModulePassForward : public Module
{
 private:
	std::string nickrequired, forwardmsg, forwardcmd;
	bool uline = true;

 public:
	void init()
	{
		Implementation eventlist[] = { I_OnPostConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Sends server password to NickServ", VF_VENDOR);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* tag = ServerInstance->Config->GetTag("passforward");
		nickrequired = tag->getString("nick", "NickServ");
		forwardmsg = tag->getString("forwardmsg", "NOTICE $nick :*** Forwarding PASS to $nickrequired");
		forwardcmd = tag->getString("cmd", "PRIVMSG $nickrequired :IDENTIFY $pass");
		uline = tag->getBool("uline", true);
		//Allow for configuring whether or not we want to check for a U:LINE
	}

	virtual void OnPostConnect(User* ruser)
	{
		LocalUser* user = IS_LOCAL(ruser);
		if (!user || user->password.empty())
			return;

		if (!nickrequired.empty())
		{
			/* Check if nick exists and its server is ulined */
			User* u = ServerInstance->FindNick(nickrequired.c_str());
			if (!u)
				return;
			if (uline && !ServerInstance->ULine(u->server))
				return;
		}

		SubstMap subst;
		user->PopulateInfoMap(subst);
		subst["nickrequired"] = nickrequired;
		subst["pass"] = user->password;
		user->WriteServ(MapFormatSubst(forwardmsg, subst));
		std::string buf = MapFormatSubst(forwardcmd, subst);
		ServerInstance->Parser->ProcessBuffer(buf, user);
	}
};

MODULE_INIT(ModulePassForward)