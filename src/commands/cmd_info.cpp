/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
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
#include "commands/cmd_info.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandInfo(Instance);
}

/** Handle /INFO
 */
CmdResult CommandInfo::Handle (const std::vector<std::string>&, User *user)
{
	user->WriteNumeric(RPL_INFO, "%s :                   -/\\- \2InspIRCd\2 -\\/-", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :                 November 2002 - Present", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s : ", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :\2Core Developers\2:", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Craig Edwards,          Brain,      <brain@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Craig McLure,           Craig,      <craig@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Robin Burchell,         w00t,       <w00t@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Oliver Lupton,          Om,         <om@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    John Brooks,            Special,    <special@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Dennis Friis,           peavey,     <peavey@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Thomas Stagner,         aquanight,  <aquanight@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Uli Schlachter,         psychon,    <psychon@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Matt Smith,             dz,         <dz@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Daniel De Graaf         danieldg,   <danieldg@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :                            jackmcbarn, <jackmcbarn@inspircd.org>", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s : ", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :\2Regular Contributors\2:", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Majic          MacGyver        Namegduf       Ankit", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :    Phoenix        Taros", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s : ", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :\2Other Contributors\2:", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :   dmb             Zaba            skenmy         GreenReaper", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :   Dan             Jason           satmd          owine", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :   Adremelech      John2           jilles         HiroP", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :   eggy            Bricker         AnMaster       djGrrr", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :   nenolod         Quension        praetorian     pippijn", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s : ", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :\2Former Contributors\2:", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :   CC              jamie           typobox43      Burlex (win32)", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :   Stskeeps        ThaPrince       BuildSmart     Thunderhacker", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :   Skip            LeaChim", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s : ", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :\2Thanks To\2:", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s :   searchirc.com   irc-junkie.org  Brik", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s : ", user->nick.c_str());
	user->WriteNumeric(RPL_INFO, "%s : Best experienced with: \2An IRC client\2", user->nick.c_str());
	FOREACH_MOD(I_OnInfo,OnInfo(user));
	user->WriteNumeric(RPL_ENDOFINFO, "%s :End of /INFO list", user->nick.c_str());
	return CMD_SUCCESS;
}
