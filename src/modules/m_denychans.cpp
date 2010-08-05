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
	{}

	void init()
	{
		Implementation eventlist[] = { I_OnCheckJoin, I_OnRehash };
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


	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (join.result != MOD_RES_PASSTHRU)
			return;
		ConfigTag* tag = FindBadChan(join.channel);
		if (!tag)
			return;
		if (IS_OPER(join.user) && tag->getBool("allowopers"))
			return;
		std::string reason = tag->getString("reason");
		std::string redirect = tag->getString("redirect");
		join.result = MOD_RES_DENY;

		if (!ServerInstance->RedirectJoin.get(join.user) && ServerInstance->IsChannel(redirect.c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			join.user->WriteNumeric(926, "%s %s :Channel %s is forbidden, redirecting to %s: %s",
				join.user->nick.c_str(),join.channel.c_str(),join.channel.c_str(),redirect.c_str(), reason.c_str());
			ServerInstance->RedirectJoin.set(join.user, 1);
			Channel::JoinUser(join.user,redirect.c_str(),false,"",false,ServerInstance->Time());
			ServerInstance->RedirectJoin.set(join.user, 0);
		} else {
			join.user->WriteNumeric(926, "%s %s :Channel %s is forbidden: %s",
				join.user->nick.c_str(),join.channel.c_str(),join.channel.c_str(),reason.c_str());
		}
	}
};

MODULE_INIT(ModuleDenyChannels)
