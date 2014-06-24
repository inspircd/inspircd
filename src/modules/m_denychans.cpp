/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

/* $ModDesc: Implements config tags which allow blocking of joins to channels */

class ModuleDenyChannels : public Module
{
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void OnRehash(User* user)
	{
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

				for (ConfigIter j = tags.first; j != tags.second; ++j)
				{
					if (InspIRCd::Match(redirect, j->second->getString("name")))
					{
						bool goodchan = false;
						ConfigTagList goodchans = ServerInstance->Config->ConfTags("goodchan");
						for (ConfigIter k = goodchans.first; k != goodchans.second; ++k)
						{
							if (InspIRCd::Match(redirect, k->second->getString("name")))
								goodchan = true;
						}

						if (!goodchan)
						{
							/* <badchan:redirect> is a badchan */
							if (user)
								user->WriteServ("NOTICE %s :Badchan %s redirects to badchan %s", user->nick.c_str(), name.c_str(), redirect.c_str());
							throw ModuleException("Badchan redirect loop");
						}
					}
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
		ConfigTagList tags = ServerInstance->Config->ConfTags("badchan");
		for (ConfigIter j = tags.first; j != tags.second; ++j)
		{
			if (InspIRCd::Match(cname, j->second->getString("name")))
			{
				if (IS_OPER(user) && j->second->getBool("allowopers"))
				{
					return MOD_RES_PASSTHRU;
				}
				else
				{
					std::string reason = j->second->getString("reason");
					std::string redirect = j->second->getString("redirect");

					ConfigTagList goodchans = ServerInstance->Config->ConfTags("goodchan");
					for (ConfigIter i = goodchans.first; i != goodchans.second; ++i)
					{
						if (InspIRCd::Match(cname, i->second->getString("name")))
						{
							return MOD_RES_PASSTHRU;
						}
					}

					if (ServerInstance->IsChannel(redirect.c_str(), ServerInstance->Config->Limits.ChanMax))
					{
						/* simple way to avoid potential loops: don't redirect to +L channels */
						Channel *newchan = ServerInstance->FindChan(redirect);
						if ((!newchan) || (!(newchan->IsModeSet('L'))))
						{
							user->WriteNumeric(926, "%s %s :Channel %s is forbidden, redirecting to %s: %s",user->nick.c_str(),cname,cname,redirect.c_str(), reason.c_str());
							Channel::JoinUser(user,redirect.c_str(),false,"",false,ServerInstance->Time());
							return MOD_RES_DENY;
						}
					}

					user->WriteNumeric(926, "%s %s :Channel %s is forbidden: %s",user->nick.c_str(),cname,cname,reason.c_str());
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleDenyChannels)
