/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2016 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <connect forceident="example">
/// $ModDepends: core 3
/// $ModDesc: Allows forcing idents on users based on their connect class.


#include "inspircd.h"

class ModuleForceIdent : public Module
{
 public:
	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		ConfigTag* tag = user->MyClass->config;
		std::string ident = tag->getString("forceident");
		if (ServerInstance->IsIdent(ident.c_str()))
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Setting ident of user '%s' (%s) in class '%s' to '%s'.",
				user->nick.c_str(), user->uuid.c_str(), user->MyClass->name.c_str(), ident.c_str());
			user->ident = ident;
			user->InvalidateCache();
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows forcing idents on users based on their connect class.");
	}
};

MODULE_INIT(ModuleForceIdent)
