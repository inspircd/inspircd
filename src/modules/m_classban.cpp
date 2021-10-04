/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016 Johanna A <johanna-a@users.noreply.github.com>
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

class ClassExtBan final
	: public ExtBan::MatchingBase
{
 private:
	std::string space;
	std::string underscore;

 public:
	ClassExtBan(Module* Creator)
		: ExtBan::MatchingBase(Creator, "class", 'n')
		, space(" ")
		, underscore("_")
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (!luser)
			return false;

		// Replace spaces with underscores as they're prohibited in mode parameters.
		std::string classname(luser->GetClass()->name);
		stdalgo::string::replace_all(classname, space, underscore);
		return InspIRCd::Match(classname, text);
	}
};

class ModuleClassBan final
	: public Module
{
 private:
	ClassExtBan extban;

 public:
	ModuleClassBan()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds extended ban n: which check whether users are in a connect class matching the specified glob pattern.")
		, extban(this)
	{
	}
};

MODULE_INIT(ModuleClassBan)
