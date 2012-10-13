/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
 public:
	void init()
	{
		ServerInstance->Modules->Attach(I_OnPostOper, this);
	}

	virtual ~ModuleModesOnOper()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Sets (and unsets) modes on opers when they oper up", VF_VENDOR);
	}

	virtual void OnPostOper(User* user, const std::string &opertype, const std::string &opername)
	{
		if (!IS_LOCAL(user))
			return;

		// whenever a user opers, go through the oper types, find their <type:modes>,
		// and if they have one apply their modes. The mode string can contain +modes
		// to add modes to the user or -modes to take modes from the user.
		std::string ThisOpersModes = user->oper->getConfig("modes");
		if (!ThisOpersModes.empty())
		{
			ApplyModes(user, ThisOpersModes);
		}
	}

	void ApplyModes(User *u, std::string &smodes)
	{
		char first = *(smodes.c_str());
		if ((first != '+') && (first != '-'))
			smodes = "+" + smodes;

		std::string buf;
		std::stringstream ss(smodes);
		std::vector<std::string> modes;

		modes.push_back(u->nick);
		// split into modes and mode params
		while (ss >> buf)
			modes.push_back(buf);

		ServerInstance->SendMode(modes, u);
	}
};

MODULE_INIT(ModuleModesOnOper)
