/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Only opers may create new channels if this module is loaded */

class ModuleRestrictChans : public Module
{
	std::set<irc::string> allowchans;

	void ReadConfig()
	{
		allowchans.clear();
		ConfigTagList tags = ServerInstance->Config->ConfTags("allowchannel");
		for(ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string txt = tag->getString("name");
			allowchans.insert(txt.c_str());
		}
	}

 public:
	ModuleRestrictChans()
	{
		ReadConfig();
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void OnRehash(User* user)
	{
		ReadConfig();
	}


	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		irc::string x = cname;
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		// channel does not yet exist (record is null, about to be created IF we were to allow it)
		if (!chan)
		{
			// user is not an oper and its not in the allow list
			if ((!IS_OPER(user)) && (allowchans.find(x) == allowchans.end()))
			{
				user->WriteNumeric(ERR_BANNEDFROMCHAN, "%s %s :Only IRC operators may create new channels",user->nick.c_str(),cname);
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ~ModuleRestrictChans()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Only opers may create new channels if this module is loaded",VF_VENDOR);
	}
};

MODULE_INIT(ModuleRestrictChans)
