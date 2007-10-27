/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
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
CmdResult CommandInfo::Handle (const char**, int, User *user)
{
	user->WriteServ( "371 %s :                   -/\\- \2InspIRCd\2 -\\/-", user->nick);
	user->WriteServ( "371 %s :                 November 2002 - Present", user->nick);
	user->WriteServ( "371 %s : ", user->nick);
	user->WriteServ( "371 %s :\2Core Developers\2:", user->nick);
	user->WriteServ( "371 %s :    Craig Edwards,          Brain,     <brain@inspircd.org>", user->nick);
	user->WriteServ( "371 %s :    Craig McLure,           Craig,     <craig@inspircd.org>", user->nick);
	user->WriteServ( "371 %s :    Robin Burchell,         w00t,      <w00t@inspircd.org>", user->nick);
	user->WriteServ( "371 %s :    Oliver Lupton,          Om,        <om@inspircd.org>", user->nick);
	user->WriteServ( "371 %s :    John Brooks,            Special,   <special@inspircd.org>", user->nick);
	user->WriteServ( "371 %s :    Pippijn van Steenhoven, pippijn,   <pippijn@one09.net>", user->nick);
	user->WriteServ( "371 %s :    Dennis Friis,           peavey,    <peavey@inspircd.org>", user->nick);
	user->WriteServ( "371 %s :                            Burlex,    <burlex@inspircd.org>", user->nick);
	user->WriteServ( "371 %s : ", user->nick);
	user->WriteServ( "371 %s :\2Regular Contributors\2:", user->nick);
	user->WriteServ( "371 %s :    satmd          nenolod         BuildSmart     HiroP", user->nick);
	user->WriteServ( "371 %s :    jilles         Stskeeps        eggy           Trystan", user->nick);
	user->WriteServ( "371 %s :    Bricker        danieldg        DarkStorm      Majic", user->nick);
	user->WriteServ( "371 %s :    ThaPrince      Thunderhacker   praetorian", user->nick);
	user->WriteServ( "371 %s :    searchirc.com  irc-junkie.org", user->nick);
	user->WriteServ( "371 %s : ", user->nick);
	user->WriteServ( "371 %s :\2Other Contributors\2:", user->nick);
	user->WriteServ( "371 %s :   dmb             CC              skenmy         LeaChim", user->nick);
	user->WriteServ( "371 %s :   typobox43       jamie           Jason          owine", user->nick);
	user->WriteServ( "371 %s :   Adremelech      dz", user->nick);
	user->WriteServ( "371 %s : ", user->nick);
	user->WriteServ( "371 %s : Best experienced with: \2An IRC client\2", user->nick);
	FOREACH_MOD(I_OnInfo,OnInfo(user));
	user->WriteServ( "374 %s :End of /INFO list", user->nick);
	return CMD_SUCCESS;
}
