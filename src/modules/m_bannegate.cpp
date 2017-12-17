/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Dylan Frank <b00mx0r@aureus.pw>
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

class ModuleBanNegate : public Module
{
 public:
	ModResult OnCheckBan(User *source, Channel *chan, const std::string& mask) CXX11_OVERRIDE
	{
		// If our matching mask begins with the negate charater, but does not have multiple in a row (to avoid nested loops)
		if (mask[0] == '~' && mask[1] != '~')
		{
			bool RealResult = chan->CheckBan(source, mask.substr(1));
			return (RealResult ? MOD_RES_ALLOW : MOD_RES_DENY);
		}
		return MOD_RES_PASSTHRU;
	}

	void Prioritize() CXX11_OVERRIDE
	{
		// Run before other modules to detect if it is a negated ban first
		ServerInstance->Modules.SetPriority(this, I_OnCheckBan, PRIORITY_FIRST);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Enables negating any ban by putting a ~ before its mask and matching extban", VF_VENDOR | VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleBanNegate)
