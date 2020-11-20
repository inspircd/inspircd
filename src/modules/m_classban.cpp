/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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

class ClassExtBan
	: public ExtBan::MatchingBase
{
 public:
	ClassExtBan(Module* Creator)
		: ExtBan::MatchingBase(Creator, "class", 'n')
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		LocalUser* luser = IS_LOCAL(user);
		return luser && InspIRCd::Match(luser->GetClass()->name, text);
	}
};

class ModuleClassBan
	: public Module
{
 private:
	ClassExtBan extban;

 public:
 	ModuleClassBan()
 		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the n extended ban which check whether users are in a connect class matching the specified glob pattern.")
		, extban(this)
	{
	}
};

MODULE_INIT(ModuleClassBan)
