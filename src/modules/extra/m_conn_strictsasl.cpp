/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 Matt Schatz <genius3000@g3k.solutions>
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
/// $ModConfig: <strictsasl reason="Fix your SASL authentication settings and try again.">
/// $ModDepends: core 3
/// $ModDesc: Disconnect users that fail a SASL auth.


#include "inspircd.h"
#include "modules/account.h"

class ModuleConnStrictSasl : public Module
{
	LocalIntExt sentauth;
	std::string reason;

 public:
	ModuleConnStrictSasl()
		: sentauth("sentauth", ExtensionItem::EXT_USER, this)
	{
	}

	void Prioritize() CXX11_OVERRIDE
	{
		// m_cap will hold registration until 'CAP END', so SASL can try more than once
		Module* cap = ServerInstance->Modules->Find("m_cap.so");
		ServerInstance->Modules->SetPriority(this, I_OnCheckReady, PRIORITY_AFTER, cap);
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		reason = ServerInstance->Config->ConfValue("strictsasl")->getString("reason", "Fix your SASL authentication settings and try again.");
	}

	void OnPostCommand(Command* command, const CommandBase::Params&, LocalUser* user, CmdResult, bool) CXX11_OVERRIDE
	{
		if (command->name == "AUTHENTICATE")
			sentauth.set(user, 1);
	}

	ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE
	{
		// Check that they have sent the AUTHENTICATE command
		if (!sentauth.get(user))
			return MOD_RES_PASSTHRU;

		// Let them through if they have an account
		const AccountExtItem* accountext = GetAccountExtItem();
		const std::string* account = accountext ? accountext->get(user) : NULL;
		if (account)
			return MOD_RES_PASSTHRU;

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Failed SASL auth from: %s (%s) [%s]",
			user->GetFullRealHost().c_str(), user->GetIPString().c_str(), user->GetRealName().c_str());
		ServerInstance->Users->QuitUser(user, reason);
		return MOD_RES_DENY;

	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		if (sentauth.get(user))
			sentauth.set(user, 0);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Disconnect users that fail a SASL auth.");
	}
};

MODULE_INIT(ModuleConnStrictSasl)
