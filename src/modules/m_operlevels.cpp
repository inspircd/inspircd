/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2006 Craig Edwards <brain@inspircd.org>
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

class ModuleOperLevels final
	: public Module
{
public:
	ModuleOperLevels()
		: Module(VF_VENDOR, "Allows the server administrator to define ranks for server operators which prevent lower ranked server operators from using /KILL on higher ranked server operators.")
	{
	}

	ModResult OnKill(User* source, User* dest, const std::string &reason) override
	{
		// oper killing an oper?
		if (dest->IsOper() && source->IsOper())
		{
			unsigned long dest_level = dest->oper->GetConfig()->getNum<unsigned long>("level", 0);
			unsigned long source_level = source->oper->GetConfig()->getNum<unsigned long>("level", 0);

			if (dest_level > source_level)
			{
				if (IS_LOCAL(source))
				{
					ServerInstance->SNO.WriteGlobalSno('a', "Oper {} (level {}) attempted to /KILL a higher level oper: {} (level {}), reason: {}",
						source->nick, source_level, dest->nick, dest_level, reason);
				}
				dest->WriteNotice("*** Oper " + source->nick + " attempted to /KILL you!");
				source->WriteNumeric(ERR_NOPRIVILEGES, INSP_FORMAT("Permission Denied - Oper {} is a higher level than you", dest->nick));
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleOperLevels)
