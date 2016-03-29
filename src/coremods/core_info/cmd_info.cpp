/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2015 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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
#include "core_info.h"

CommandInfo::CommandInfo(Module* parent)
	: ServerTargetCommand(parent, "INFO")
{
	Penalty = 4;
	syntax = "[<servername>]";
}

static const char* const lines[] = {
	"                   -/\\- \2InspIRCd\2 -\\/-",
	"                 November 2002 - Present",
	" ",
	"\2Core Developers\2:",
	"    Attila Molnar,          Attila,     <attilamolnar@hush.com>",
	"    Peter Powell,           SaberUK,    <petpow@saberuk.com>",
	" ",
	"\2Former Developers\2:",
	"    Oliver Lupton,          Om,         <om@inspircd.org>",
	"    John Brooks,            Special,    <special@inspircd.org>",
	"    Dennis Friis,           peavey,     <peavey@inspircd.org>",
	"    Thomas Stagner,         aquanight,  <aquanight@inspircd.org>",
	"    Uli Schlachter,         psychon,    <psychon@inspircd.org>",
	"    Matt Smith,             dz,         <dz@inspircd.org>",
	"    Daniel De Graaf,        danieldg,   <danieldg@inspircd.org>",
	" ",
	"\2Founding Developers\2:",
	"    Craig Edwards,          Brain,      <brain@inspircd.org>",
	"    Craig McLure,           Craig,      <craig@inspircd.org>",
	"    Robin Burchell,         w00t,       <w00t@inspircd.org>",
	" ",
	"\2Active Contributors\2:",
	"    Adam           Shutter",
	" ",
	"\2Former Contributors\2:",
	"   dmb             Zaba            skenmy         GreenReaper",
	"   Dan             Jason           satmd          owine",
	"   Adremelech      John2           jilles         HiroP",
	"   eggy            Bricker         AnMaster       djGrrr",
	"   nenolod         Quension        praetorian     pippijn",
	"   CC              jamie           typobox43      Burlex (win32)",
	"   Stskeeps        ThaPrince       BuildSmart     Thunderhacker",
	"   Skip            LeaChim         Majic          MacGyver",
	"   Namegduf        Ankit           Phoenix        Taros",
	"   jackmcbarn      ChrisTX         Shawn",
	" ",
	"\2Thanks To\2:",
	"   Asmo            Brik            fraggeln",
	" ",
	" Best experienced with: \2An IRC client\2",
	NULL
};

/** Handle /INFO
 */
CmdResult CommandInfo::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 0 && parameters[0] != ServerInstance->Config->ServerName)
		return CMD_SUCCESS;

	int i=0;
	while (lines[i])
		user->WriteRemoteNumeric(RPL_INFO, lines[i++]);
	FOREACH_MOD(OnInfo, (user));
	user->WriteRemoteNumeric(RPL_ENDOFINFO, "End of /INFO list");
	return CMD_SUCCESS;
}
