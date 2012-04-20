/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
 private:


	ConfigReader *Conf;

 public:
	ModuleDenyChannels(InspIRCd* Me) : Module(Me)
	{

		Conf = new ConfigReader(ServerInstance);
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void OnRehash(User* user)
	{
		delete Conf;
		Conf = new ConfigReader(ServerInstance);
		/* check for redirect validity and loops/chains */
		for (int i =0; i < Conf->Enumerate("badchan"); i++)
		{
			std::string name = Conf->ReadValue("badchan","name",i);
			std::string redirect = Conf->ReadValue("badchan","redirect",i);

			if (!redirect.empty())
			{

				if (!ServerInstance->IsChannel(redirect.c_str(), ServerInstance->Config->Limits.ChanMax))
				{
					if (user)
						user->WriteServ("NOTICE %s :Invalid badchan redirect '%s'", user->nick.c_str(), redirect.c_str());
					throw ModuleException("Invalid badchan redirect, not a channel");
				}

				for (int j =0; j < Conf->Enumerate("badchan"); j++)
				{
					if (InspIRCd::Match(redirect, Conf->ReadValue("badchan","name",j)))
					{
						bool goodchan = false;
						for (int k =0; k < Conf->Enumerate("goodchan"); k++)
						{
							if (InspIRCd::Match(redirect, Conf->ReadValue("goodchan","name",k)))
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
		delete Conf;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR,API_VERSION);
	}


	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		for (int j =0; j < Conf->Enumerate("badchan"); j++)
		{
			if (InspIRCd::Match(cname, Conf->ReadValue("badchan","name",j)))
			{
				if (IS_OPER(user) && Conf->ReadFlag("badchan","allowopers",j))
				{
					return 0;
				}
				else
				{
					std::string reason = Conf->ReadValue("badchan","reason",j);
					std::string redirect = Conf->ReadValue("badchan","redirect",j);

					for (int i = 0; i < Conf->Enumerate("goodchan"); i++)
					{
						if (InspIRCd::Match(cname, Conf->ReadValue("goodchan", "name", i)))
						{
							return 0;
						}
					}

					if (ServerInstance->IsChannel(redirect.c_str(), ServerInstance->Config->Limits.ChanMax))
					{
						/* simple way to avoid potential loops: don't redirect to +L channels */
						Channel *newchan = ServerInstance->FindChan(redirect);
						if ((!newchan) || (!(newchan->IsModeSet('L'))))
						{
							user->WriteNumeric(926, "%s %s :Channel %s is forbidden, redirecting to %s: %s",user->nick.c_str(),cname,cname,redirect.c_str(), reason.c_str());
							Channel::JoinUser(ServerInstance,user,redirect.c_str(),false,"",false,ServerInstance->Time());
							return 1;
						}
					}

					user->WriteNumeric(926, "%s %s :Channel %s is forbidden: %s",user->nick.c_str(),cname,cname,reason.c_str());
					return 1;
				}
			}
		}
		return 0;
	}
};

MODULE_INIT(ModuleDenyChannels)
