/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
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

/// $ModAuthor: Attila Molnar
/// $ModAuthorMail: attilamolnar@hush.com
/// $ModConfig: <connbanner text="Banner text goes here.">
/// $ModDepends: core 3
/// $ModDesc: Displays a static text to every connecting user before registration


#include "inspircd.h"

class ModuleConnBanner : public Module
{
	std::string text;

 public:
	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		text = ServerInstance->Config->ConfValue("connbanner")->getString("text");
	}

	void OnUserPostInit(LocalUser* user) CXX11_OVERRIDE
	{
		if (!text.empty())
			user->WriteNotice("*** " + text);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Displays a static text to every connecting user before registration");
	}
};

MODULE_INIT(ModuleConnBanner)
