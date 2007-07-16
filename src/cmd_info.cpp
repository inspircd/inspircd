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
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands/cmd_info.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_info(Instance);
}

/** Handle /INFO
 */
CmdResult cmd_info::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ( "371 %s :. o O ( \2The Inspire Internet Relay Chat Server\2  ) O o .", user->nick);
	user->WriteServ( "371 %s :      ( \2Putting the ricer into ircer since 2007\2 )", user->nick);
	user->WriteServ( "371 %s : ", user->nick);
	user->WriteServ( "371 %s :\2Core Developers\2:", user->nick);
	user->WriteServ( "371 %s :        Craig Edwards (Brain)", user->nick);
	user->WriteServ( "371 %s :        Craig McLure", user->nick);
	user->WriteServ( "371 %s :        w00t", user->nick);
	user->WriteServ( "371 %s :        Om", user->nick);
	user->WriteServ( "371 %s :        Special", user->nick);
	user->WriteServ( "371 %s :        pippijn", user->nick);
	user->WriteServ( "371 %s :        peavey", user->nick);
	user->WriteServ( "371 %s :        Burlex", user->nick);
	user->WriteServ( "371 %s : ", user->nick);
	user->WriteServ( "371 %s :\2Contributors\2:", user->nick);
	user->WriteServ( "371 %s :        typobox43   Jazza", user->nick);
	user->WriteServ( "371 %s :        jamie       LeaChim", user->nick);
	user->WriteServ( "371 %s :        satmd       nenolod", user->nick);
	user->WriteServ( "371 %s :        HiroP       BuildSmart", user->nick);
	user->WriteServ( "371 %s : ", user->nick);
	user->WriteServ( "371 %s :\2Quality Assurance\2:", user->nick);
	user->WriteServ( "371 %s :        Bricker     owine", user->nick);
	user->WriteServ( "371 %s :        dmb         Adremelech", user->nick);
	user->WriteServ( "371 %s :        ThePopeSVCD satmd", user->nick);
	user->WriteServ( "371 %s :\2Testers\2:", user->nick);
	user->WriteServ( "371 %s :        CC          Piggles", user->nick);
	user->WriteServ( "371 %s :        Foamy       Hart", user->nick);
	user->WriteServ( "371 %s :        RageD       [ed]", user->nick);
	user->WriteServ( "371 %s :        Azhrarn     luigiman", user->nick);
	user->WriteServ( "371 %s :        Chu         aquanight", user->nick);
	user->WriteServ( "371 %s :        xptek       Grantlinks", user->nick);
	user->WriteServ( "371 %s :        Rob         angelic", user->nick);
	user->WriteServ( "371 %s :        Jason       ThaPrince", user->nick);
	user->WriteServ( "371 %s :        eggy        skenmy", user->nick);
	user->WriteServ( "371 %s : ", user->nick);
	user->WriteServ( "371 %s :Contains portions of \2FireDNS\2 written by", user->nick);
	user->WriteServ( "371 %s :Ian Gulliver, (c) 2002.", user->nick);
	user->WriteServ( "371 %s : ", user->nick);
	user->WriteServ( "371 %s :Thanks to \2irc-junkie\2 and \2SearchIRC\2", user->nick);
	user->WriteServ( "371 %s :for the nice comments and the help", user->nick);
	user->WriteServ( "371 %s :you gave us in attracting users to", user->nick);
	user->WriteServ( "371 %s :this software.", user->nick);
	user->WriteServ( "371 %s : ", user->nick);
	user->WriteServ( "371 %s :Best experienced with: \2An IRC client\2.", user->nick);
	FOREACH_MOD(I_OnInfo,OnInfo(user));
	user->WriteServ( "374 %s :End of /INFO list", user->nick);
	return CMD_SUCCESS;
}
