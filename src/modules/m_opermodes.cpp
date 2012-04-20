/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Sets (and unsets) modes on opers when they oper up */

class ModuleModesOnOper : public Module
{
 private:


	ConfigReader *Conf;

 public:
	ModuleModesOnOper(InspIRCd* Me)
		: Module(Me)
	{

		Conf = new ConfigReader(ServerInstance);
		Implementation eventlist[] = { I_OnPostOper, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual void OnRehash(User* user)
	{
		delete Conf;
		Conf = new ConfigReader(ServerInstance);
	}

	virtual ~ModuleModesOnOper()
	{
		delete Conf;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

	virtual void OnPostOper(User* user, const std::string &opertype, const std::string &opername)
	{
		// whenever a user opers, go through the oper types, find their <type:modes>,
		// and if they have one apply their modes. The mode string can contain +modes
		// to add modes to the user or -modes to take modes from the user.
		for (int j =0; j < Conf->Enumerate("type"); j++)
		{
			std::string typen = Conf->ReadValue("type","name",j);
			if (typen == user->oper)
			{
				std::string ThisOpersModes = Conf->ReadValue("type","modes",j);
				if (!ThisOpersModes.empty())
				{
					ApplyModes(user, ThisOpersModes);
				}
				break;
			}
		}

		if (!opername.empty()) // if user is local ..
		{
			for (int j = 0; j < Conf->Enumerate("oper"); j++)
			{
				if (opername == Conf->ReadValue("oper", "name", j))
				{
					std::string ThisOpersModes = Conf->ReadValue("oper", "modes", j);
					if (!ThisOpersModes.empty())
					{
						ApplyModes(user, ThisOpersModes);
					}
					break;
				}
			}
		}
	}

	void ApplyModes(User *u, std::string &smodes)
	{
		char first = *(smodes.c_str());
		if ((first != '+') && (first != '-'))
			smodes = "+" + smodes;

		std::string buf;
		std::stringstream ss(smodes);
		std::vector<std::string> tokens;

		// split into modes and mode params
		while (ss >> buf)
			tokens.push_back(buf);

		std::vector<std::string> modes;
		modes.push_back(u->nick);

		// process mode params
		for (unsigned int k = 0; k < tokens.size(); k++)
		{
			modes.push_back(tokens[k]);
		}

		ServerInstance->SendGlobalMode(modes, u);
	}
};

MODULE_INIT(ModuleModesOnOper)
