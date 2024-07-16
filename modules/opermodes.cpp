/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005 Craig Edwards <brain@inspircd.org>
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

class ModuleOperModes final
	: public Module
{
public:
	ModuleOperModes()
		: Module(VF_VENDOR, "Allows the server administrator to set user modes on server operators when they log into their server operator account.")
	{
	}

	void OnPostOperLogin(User* user, bool automatic) override
	{
		if (!IS_LOCAL(user))
			return; // We don't handle remote users.

		const std::string opermodes = user->oper->GetConfig()->getString("modes");
		if (opermodes.empty())
			return; // We don't have any modes to set.

		CommandBase::Params modeparams;
		modeparams.push_back(user->nick);

		irc::spacesepstream modestream(opermodes);
		for (std::string modeparam; modestream.GetToken(modeparam); )
			modeparams.push_back(modeparam);

		if (modeparams.size() > 1)
			ServerInstance->Parser.CallHandler("MODE", modeparams, user);
	}
};

MODULE_INIT(ModuleOperModes)
