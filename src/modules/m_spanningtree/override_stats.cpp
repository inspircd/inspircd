/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012, 2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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

#include "main.h"
#include "utils.h"
#include "link.h"

ModResult ModuleSpanningTree::OnStats(Stats::Context& stats)
{
	if ((stats.GetSymbol() == 'c') || (stats.GetSymbol() == 'n'))
	{
		for (const auto& L : Utils->LinkBlocks)
		{
			std::string ipaddr = "*@";
			if (L->HiddenFromStats)
				ipaddr.append("<hidden>");
			else
				ipaddr.append(L->IPAddr);

			const std::string hook = (L->Hook.empty() ? "plaintext" : L->Hook);
			stats.AddRow(213, stats.GetSymbol(), ipaddr, '*', L->Name, L->Port, hook);
			if (stats.GetSymbol() == 'c')
				stats.AddRow(244, 'H', '*', '*', L->Name);
		}
		return MOD_RES_DENY;
	}
	else if (stats.GetSymbol() == 'U')
	{
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("services", ServerInstance->Config->ConfTags("uline")))
		{
			std::string name = tag->getString("server");
			if (!name.empty())
				stats.AddRow(248, 'U', name);
		}
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}
