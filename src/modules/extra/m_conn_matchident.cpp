/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2019 Matt Schatz <genius3000@g3k.solutions>
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <connect matchident="myIdent thatIdent ~thisIdent">
/// $ModDepends: core 3
/// $ModDesc: Allows a connect class to match by ident(s).


#include "inspircd.h"

class ModuleConnMatchIdent : public Module
{
 public:
	void Prioritize() CXX11_OVERRIDE
	{
		// Go after requireident, which is recommended but not required.
		Module* requireident = ServerInstance->Modules->Find("m_ident.so");
		ServerInstance->Modules->SetPriority(this, I_OnSetConnectClass, PRIORITY_AFTER, requireident);
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* connclass) CXX11_OVERRIDE
	{
		const std::string matchident = connclass->config->getString("matchident");
		if (matchident.empty())
			return MOD_RES_PASSTHRU;

		irc::spacesepstream ss(matchident);
		for (std::string token; ss.GetToken(token); )
		{
			if (InspIRCd::Match(user->ident, token))
				return MOD_RES_PASSTHRU;
		}

		return MOD_RES_DENY;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows a connect class to match by ident(s).");
	}
};

MODULE_INIT(ModuleConnMatchIdent)
