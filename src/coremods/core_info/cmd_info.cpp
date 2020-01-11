/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
	"                   -/\\- \002InspIRCd\002 -\\/-",
	"                 November 2002 - Present",
	" ",
	"\002Core Developers\002:",
	"    Attila Molnar,          Attila,     <attilamolnar@hush.com>",
	"    Sadie Powell,           SadieCat,   <sadie@witchery.services>",
	" ",
	"\002Former Developers\002:",
	"    Oliver Lupton,          Om,         <om@inspircd.org>",
	"    John Brooks,            Special,    <special@inspircd.org>",
	"    Dennis Friis,           peavey,     <peavey@inspircd.org>",
	"    Thomas Stagner,         aquanight,  <aquanight@inspircd.org>",
	"    Uli Schlachter,         psychon,    <psychon@inspircd.org>",
	"    Matt Smith,             dz,         <dz@inspircd.org>",
	"    Daniel De Graaf,        danieldg,   <danieldg@inspircd.org>",
	" ",
	"\002Founding Developers\002:",
	"    Craig Edwards,          Brain,      <brain@inspircd.org>",
	"    Craig McLure,           Craig,      <craig@inspircd.org>",
	"    Robin Burchell,         w00t,       <w00t@inspircd.org>",
	" ",
	"\002Active Contributors\002:",
	"    Adam           linuxdaemon     Sheogorath",
	" ",
	"\002Former Contributors\002:",
	"   dmb             Zaba            skenmy         GreenReaper",
	"   Dan             Jason           satmd          owine",
	"   Adremelech      John2           jilles         HiroP",
	"   eggy            Bricker         AnMaster       djGrrr",
	"   nenolod         Quension        praetorian     pippijn",
	"   CC              jamie           typobox43      Burlex",
	"   Stskeeps        ThaPrince       BuildSmart     Thunderhacker",
	"   Skip            LeaChim         Majic          MacGyver",
	"   Namegduf        Ankit           Phoenix        Taros",
	"   jackmcbarn      ChrisTX         Shawn          Shutter",
	" ",
	"\002Thanks To\002:",
	"   Asmo            Brik            fraggeln       genius3000",
	" ",
	" Best experienced with: \002An IRC client\002",
	NULL
};

/** Handle /INFO
 */
CmdResult CommandInfo::Handle(User* user, const Params& parameters)
{
	if (parameters.size() > 0 && !irc::equals(parameters[0], ServerInstance->Config->ServerName))
		return CMD_SUCCESS;

	int i=0;
	while (lines[i])
		user->WriteRemoteNumeric(RPL_INFO, lines[i++]);

	user->WriteRemoteNumeric(RPL_ENDOFINFO, "End of /INFO list");
	return CMD_SUCCESS;
}
