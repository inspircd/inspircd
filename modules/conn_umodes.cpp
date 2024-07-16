/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

class ModuleModesOnConnect final
	: public Module
{
public:
	ModuleModesOnConnect()
		: Module(VF_VENDOR, "Allows the server administrator to set user modes on connecting users.")
	{
	}

	void OnUserConnect(LocalUser* user) override
	{
		const std::string modestr = user->GetClass()->config->getString("modes");
		if (modestr.empty())
			return;

		CommandBase::Params params;
		params.push_back(user->nick);

		irc::spacesepstream modestream(modestr);
		for (std::string modetoken; modestream.GetToken(modetoken); )
			params.push_back(modetoken);

		ServerInstance->Parser.CallHandler("MODE", params, user);
	}
};

MODULE_INIT(ModuleModesOnConnect)
