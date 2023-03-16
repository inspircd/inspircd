/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Manuel Leiner <satmd@euirc.net>
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

/// $ModAuthor: Manuel Leiner
/// $ModAuthorMail: satmd@euirc.net
/// $ModDesc: Implement JOIN 0 (joining 0 makes a user part all channels), RFC2812
/// $ModDepends: core 3

class ModuleJoinZero : public Module
{
 public:
	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) CXX11_OVERRIDE
	{
		if (validated && command == "JOIN" && (parameters.size() == 1) && parameters[0] == "0")
		{
			for (User::ChanList::iterator i = user->chans.begin(); i != user->chans.end(); )
			{
				Channel* chan = (*i)->chan;
				++i;

				std::string reason("Left all channels");
				chan->PartUser(user, reason);
			}
			return MOD_RES_DENY;
		}
		else
			return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implement JOIN 0 (joining 0 makes a user part all channels), RFC2812");
	}
};

MODULE_INIT(ModuleJoinZero)
