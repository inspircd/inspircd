/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <string>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "message.h"
#include "commands.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "cmd_info.h"

extern ServerConfig* Config;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

void cmd_info::Handle (char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"371 %s :. o O (The Inspire Internet Relay Chat Server) O o .",user->nick);
        WriteServ(user->fd,"371 %s : ",user->nick);
        WriteServ(user->fd,"371 %s :Core developers: Craig Edwards (Brain)",user->nick);
        WriteServ(user->fd,"371 %s :                 Craig McLure",user->nick);
        WriteServ(user->fd,"371 %s : ",user->nick);
        WriteServ(user->fd,"371 %s :Contributors:    typobox43",user->nick);
        WriteServ(user->fd,"371 %s :                 w00t",user->nick);
        WriteServ(user->fd,"371 %s :                 Om",user->nick);
        WriteServ(user->fd,"371 %s :                 Jazza",user->nick);
        WriteServ(user->fd,"371 %s : ",user->nick);
        WriteServ(user->fd,"371 %s :Testers:         CC",user->nick);
        WriteServ(user->fd,"371 %s :                 Piggles",user->nick);
        WriteServ(user->fd,"371 %s :                 Foamy",user->nick);
        WriteServ(user->fd,"371 %s :                 Hart",user->nick);
        WriteServ(user->fd,"371 %s :                 RageD",user->nick);
        WriteServ(user->fd,"371 %s :                 [ed]",user->nick);
        WriteServ(user->fd,"371 %s :                 Azhrarn",user->nick);
        WriteServ(user->fd,"371 %s :                 nenolod",user->nick);
        WriteServ(user->fd,"371 %s :                 luigiman",user->nick);
        WriteServ(user->fd,"371 %s :                 Chu",user->nick);
        WriteServ(user->fd,"371 %s :                 aquanight",user->nick);
        WriteServ(user->fd,"371 %s :                 xptek",user->nick);
        WriteServ(user->fd,"371 %s :                 Grantlinks",user->nick);
        WriteServ(user->fd,"371 %s :                 Rob",user->nick);
        WriteServ(user->fd,"371 %s :                 angelic",user->nick);
        WriteServ(user->fd,"371 %s :                 Jason",user->nick);
        WriteServ(user->fd,"371 %s :                 ThaPrince",user->nick);
        WriteServ(user->fd,"371 %s : ",user->nick);
        WriteServ(user->fd,"371 %s :Thanks to irc-junkie and searchirc",user->nick);
        WriteServ(user->fd,"371 %s :for the nice comments and the help",user->nick);
        WriteServ(user->fd,"371 %s :you gave us in attracting users to",user->nick);
        WriteServ(user->fd,"371 %s :this software.",user->nick);
        WriteServ(user->fd,"371 %s : ",user->nick);
        WriteServ(user->fd,"371 %s :Best experienced with: An IRC client.",user->nick);
	FOREACH_MOD(I_OnInfo,OnInfo(user));
	WriteServ(user->fd,"374 %s :End of /INFO list",user->nick);
}


