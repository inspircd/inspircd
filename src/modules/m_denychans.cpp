/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Implements config tags which allow blocking of joins to channels */

class ModuleDenyChannels : public Module
{
 public:
	ModuleDenyChannels()
	{
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	ConfigTag* FindBadChan(const std::string& cname)
	{
		ConfigTagList tags = ServerInstance->Config->ConfTags("badchan");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			if (InspIRCd::Match(cname, i->second->getString("name")))
			{
				// found a bad channel. Maybe it has a goodchan pattern?
				tags = ServerInstance->Config->ConfTags("goodchan");
				for (ConfigIter j = tags.first; j != tags.second; ++j)
				{
					if (InspIRCd::Match(cname, i->second->getString("name")))
						return NULL;
				}
				// nope
				return i->second;
			}
		}
		// no <badchan> found
		return NULL;
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf;
		/* check for redirect validity and loops/chains */
		ConfigTagList tags = ServerInstance->Config->ConfTags("badchan");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			std::string name = i->second->getString("name");
			std::string redirect = i->second->getString("redirect");

			if (!redirect.empty())
			{
				if (!ServerInstance->IsChannel(redirect.c_str(), ServerInstance->Config->Limits.ChanMax))
				{
					if (user)
						user->WriteServ("NOTICE %s :Invalid badchan redirect '%s'", user->nick.c_str(), redirect.c_str());
					throw ModuleException("Invalid badchan redirect, not a channel");
				}

				ConfigTag* tag = FindBadChan(redirect);

				if (tag)
				{
					/* <badchan:redirect> is a badchan */
					if (user)
						user->WriteServ("NOTICE %s :Badchan %s redirects to badchan %s", user->nick.c_str(), name.c_str(), redirect.c_str());
					throw ModuleException("Badchan redirect loop");
				}
			}
		}
	}

	virtual ~ModuleDenyChannels()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Implements config tags which allow blocking of joins to channels", VF_VENDOR);
	}


	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		ConfigTag* tag = FindBadChan(cname);
		if (!tag)
			return MOD_RES_PASSTHRU;
		if (IS_OPER(user) && tag->getBool("allowopers"))
			return MOD_RES_PASSTHRU;
		std::string reason = tag->getString("reason");
		std::string redirect = tag->getString("redirect");

		if (ServerInstance->IsChannel(redirect.c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			/* simple way to avoid potential loops: don't redirect to +L channels */
			Channel *newchan = ServerInstance->FindChan(redirect);
			if (!newchan || !newchan->IsModeSet("redirect"))
			{
				user->WriteNumeric(926, "%s %s :Channel %s is forbidden, redirecting to %s: %s",user->nick.c_str(),cname,cname,redirect.c_str(), reason.c_str());
				Channel::JoinUser(user,redirect.c_str(),false,"",false,ServerInstance->Time());
				return MOD_RES_DENY;
			}
		}

		user->WriteNumeric(926, "%s %s :Channel %s is forbidden: %s",user->nick.c_str(),cname,cname,reason.c_str());
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleDenyChannels)
