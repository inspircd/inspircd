/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2022, 2024-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "core_info.h"

enum
{
	// From RFC 1459
	RPL_INFO = 371,
	RPL_ENDOFINFO = 374,
};

CommandInfo::CommandInfo(Module* parent)
	: SplitCommand(parent, "INFO")
{
	penalty = 3000;
}

static const char* const lines[] = {
	"                   -/\\- \002InspIRCd\002 -\\/-",
	"                 November 2002 - Present",
	" ",
	"\002Core Developers\002:",
	"    Sadie Powell (Sadie)",
	" ",
	"\002Former Developers\002:",
	"    Adam (Adam-)                    Attila Molnar (Attila)",
	"    Daniel De Graaf (danieldg)      Dennis Friis (peavey)",
	"    John Brooks (special)           Matt Schatz (genius3000)",
	"    Matt Smith (dz)                 Oliver Lupton (Om)",
	"    Thomas Stagner (aquanight)      Uli Schlachter (psychon)",
	" ",
	"\002Founding Developers\002:",
	"    Craig Edwards (Brain)           Craig McLure (FrostyCoolSlug)",
	"    Robin Burchell (w00t)",
	" ",
	"\002Active Contributors\002:",
	"    progval",
	" ",
	"\002Former Contributors\002:",
	"    Adremelech      Ankit           AnMaster        Bricker",
	"    BuildSmart      Burlex          CC              ChrisTX",
	"    Dan             djGrrr          dmb             eggy",
	"    fraggeln        GreenReaper     HiroP           jackmcbarn",
	"    jamie           Jason           jilles          John2",
	"    kaniini         LeaChim         linuxdaemon     MacGyver",
	"    majic           Namegduf        owine           Phoenix",
	"    pippijn         praetorian      Quension        Robby",
	"    satmd           Shawn           Sheogorath      Shutter",
	"    skenmy          Skip            Stskeeps        Taros",
	"    ThaPrince       Thunderhacker   typobox43       Zaba",
	" ",
	"\002Thanks To\002:",
	"    Asmo            Brik            dan-            Duck",
	"    jwheare         prawnsalad",
	" ",
	" Best experienced with \002an IRC client\002",
	nullptr
};

CmdResult CommandInfo::HandleLocal(LocalUser* user, const Params& parameters)
{
	for (size_t idx = 0; lines[idx]; ++idx)
		user->WriteRemoteNumeric(RPL_INFO, lines[idx]);
	user->WriteRemoteNumeric(RPL_ENDOFINFO, "End of /INFO list");
	return CmdResult::SUCCESS;
}
