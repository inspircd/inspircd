/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
	user->WriteServ( "371 %s :                   -/\\- \2InspIRCd\2 -\\/-", user->nick.c_str());
	user->WriteServ( "371 %s :                 November 2002 - Present", user->nick.c_str());
	user->WriteServ( "371 %s : ", user->nick.c_str());
	user->WriteServ( "371 %s :\2Core Developers\2:", user->nick.c_str());
	user->WriteServ( "371 %s :    Craig Edwards,          Brain,     <brain@inspircd.org>", user->nick.c_str());
	user->WriteServ( "371 %s :    Craig McLure,           Craig,     <craig@inspircd.org>", user->nick.c_str());
	user->WriteServ( "371 %s :    Robin Burchell,         w00t,      <w00t@inspircd.org>", user->nick.c_str());
	user->WriteServ( "371 %s :    Oliver Lupton,          Om,        <om@inspircd.org>", user->nick.c_str());
	user->WriteServ( "371 %s :    John Brooks,            Special,   <special@inspircd.org>", user->nick.c_str());
	user->WriteServ( "371 %s :    Pippijn van Steenhoven, pippijn,   <pippijn@one09.net>", user->nick.c_str());
	user->WriteServ( "371 %s :    Dennis Friis,           peavey,    <peavey@inspircd.org>", user->nick.c_str());
	user->WriteServ( "371 %s :    Thomas Stagner,         aquanight, <aquanight@inspircd.org>", user->nick.c_str());
	user->WriteServ( "371 %s :    Eric Dietz,             fez,       <fez@inspircd.org>", user->nick.c_str());
	user->WriteServ( "371 %s : ", user->nick.c_str());
	user->WriteServ( "371 %s :\2Regular Contributors\2:", user->nick.c_str());
	user->WriteServ( "371 %s :    satmd          nenolod         John2          Skip", user->nick.c_str());
	user->WriteServ( "371 %s :    jilles         Majic           eggy           Trystan", user->nick.c_str());
	user->WriteServ( "371 %s :    Quension       danieldg        praetorian", user->nick.c_str());
	user->WriteServ( "371 %s :    GreenReaper    searchirc.com   irc-junkie.org", user->nick.c_str());
	user->WriteServ( "371 %s : ", user->nick.c_str());
	user->WriteServ( "371 %s :\2Other Contributors\2:", user->nick.c_str());
	user->WriteServ( "371 %s :   dmb             Zaba            skenmy         LeaChim", user->nick.c_str());
	user->WriteServ( "371 %s :   Thunderhacker   Dan             Jason          owine", user->nick.c_str());
	user->WriteServ( "371 %s :   Adremelech      dz              BuildSmart     HiroP", user->nick.c_str());
	user->WriteServ( "371 %s :   ThaPrince       Bricker         AnMaster       djGrrr", user->nick.c_str());
	user->WriteServ( "371 %s : ", user->nick.c_str());
	user->WriteServ( "371 %s :\2Former Contributors\2:", user->nick.c_str());
	user->WriteServ( "371 %s :   CC              jamie           typobox43      Burlex (win32)", user->nick.c_str());
	user->WriteServ( "371 %s :   Stskeeps        ", user->nick.c_str());
	user->WriteServ( "371 %s : ", user->nick.c_str());
	user->WriteServ( "371 %s : Best experienced with: \2An IRC client\2", user->nick.c_str());
	FOREACH_MOD(I_OnInfo,OnInfo(user));
	user->WriteServ( "374 %s :End of /INFO list", user->nick.c_str());
	return CMD_SUCCESS;
}
