/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2006 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Only opers may create new channels if this module is loaded */

class ModuleRestrictChans : public Module
{


	std::map<irc::string,int> allowchans;

	void ReadConfig()
	{
		ConfigReader* MyConf = new ConfigReader(ServerInstance);
		allowchans.clear();
		for (int i = 0; i < MyConf->Enumerate("allowchannel"); i++)
		{
			std::string txt;
			txt = MyConf->ReadValue("allowchannel", "name", i);
			irc::string channel = txt.c_str();
			allowchans[channel] = 1;
		}
		delete MyConf;
	}

 public:
	ModuleRestrictChans(InspIRCd* Me)
		: Module(Me)
	{

		ReadConfig();
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void OnRehash(User* user)
	{
		ReadConfig();
	}


	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		irc::string x = cname;
		if (!IS_LOCAL(user))
			return 0;

		// channel does not yet exist (record is null, about to be created IF we were to allow it)
		if (!chan)
		{
			// user is not an oper and its not in the allow list
			if ((!IS_OPER(user)) && (allowchans.find(x) == allowchans.end()))
			{
				user->WriteNumeric(ERR_BANNEDFROMCHAN, "%s %s :Only IRC operators may create new channels",user->nick.c_str(),cname);
				return 1;
			}
		}
		return 0;
	}

	virtual ~ModuleRestrictChans()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleRestrictChans)
