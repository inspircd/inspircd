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
#include "wildcard.h"

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
	
	virtual void OnRehash(User* user, const std::string &param)
	{
		delete Conf;
		Conf = new ConfigReader(ServerInstance);
	}

	virtual ~ModuleDenyChannels()
	{
		delete Conf;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}


	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs)
	{
		for (int j =0; j < Conf->Enumerate("badchan"); j++)
		{
			if (match(cname, Conf->ReadValue("badchan","name",j).c_str()))
			{
				if (IS_OPER(user) && Conf->ReadFlag("badchan","allowopers",j))
				{
					return 0;
				}
				else
				{
					std::string reason = Conf->ReadValue("badchan","reason",j);
					std::string redirect = Conf->ReadValue("badchan","redirect",j);

					for (int j = 0; j < Conf->Enumerate("goodchan"); j++)
					{
						if (match(cname, Conf->ReadValue("goodchan", "name", j).c_str()))
						{
							return 0;
						}
					}
					
					if (ServerInstance->IsChannel(redirect.c_str()))
					{
						user->writeserv("926 %s %s :Channel %s is forbidden, redirecting to %s: %s",user->nick,cname,cname,redirect.c_str(), reason.c_str());
						Channel::JoinUser(ServerInstance,user,redirect.c_str(),false,"",false,ServerInstance->Time(true));
						return 1;
					}

					user->WriteServ("926 %s %s :Channel %s is forbidden: %s",user->nick,cname,cname,reason.c_str());
					return 1;
				}
			}
		}
		return 0;
	}
};

MODULE_INIT(ModuleDenyChannels)
