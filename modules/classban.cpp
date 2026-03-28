/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016 Johanna A
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
#include "modules/extban.h"
#include "numerichelper.h"

class ClassExtBan final
	: public ExtBan::MatchingBase
{
public:
	bool operonly;

	ClassExtBan(const WeakModulePtr& Creator)
		: ExtBan::MatchingBase(Creator, "class", 'n')
	{
	}

	bool IsMatch(ListModeBase* lm, User* user, Channel* channel, const std::string& text, const ExtBan::MatchConfig& config) override
	{
		auto* luser = user->AsLocal();
		if (!luser)
			return false;

		// Replace spaces with underscores as they're prohibited in mode parameters.
		std::string classname(luser->GetClass()->name);
		std::replace(classname.begin(), classname.end(), ' ', '_');
		return InspIRCd::Match(classname, text);
	}


	bool Validate(ListModeBase* lm, LocalUser* user, Channel* channel, std::string& text) override
	{
		if (operonly && !user->HasPrivPermission("users/auspex"))
		{
			user->WriteNumeric(Numerics::NoPrivileges(user, "your server operator account does not have the users/auspex privilege"));
			return false;
		}
		return true;
	}
};

class ModuleClassBan final
	: public Module
{
private:
	ClassExtBan extban;

public:
	ModuleClassBan()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds extended ban n: (class) which check whether users are in a connect class matching the specified glob pattern.")
		, extban(weak_from_this())
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("classban");
		extban.operonly = tag->getBool("operonly");
	}
};

MODULE_INIT(ModuleClassBan)
