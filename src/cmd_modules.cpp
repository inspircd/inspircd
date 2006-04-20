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

#include <time.h>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include "hash_map.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "helperfuncs.h"
#include "wildcard.h"
#include "commands/cmd_modules.h"

extern ServerConfig* Config;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

char* itab[] = {
	"OnUserConnect", "OnUserQuit", "OnUserDisconnect", "OnUserJoin", "OnUserPart", "OnRehash", "OnServerRaw",
	"OnExtendedMode", "OnUserPreJoin", "OnUserPreKick", "OnUserKick", "OnOper", "OnInfo", "OnWhois", "OnUserPreInvite",
	"OnUserInvite", "OnUserPreMessage", "OnUserPreNotice", "OnUserPreNick", "OnUserMessage", "OnUserNotice", "OnMode",
	"OnGetServerDescription", "OnSyncUser", "OnSyncChannel", "OnSyncChannelMetaData", "OnSyncUserMetaData",
	"OnDecodeMetaData", "ProtoSendMode", "ProtoSendMetaData", "OnWallops", "OnChangeHost", "OnChangeName", "OnAddGLine",
	"OnAddZLine", "OnAddQLine", "OnAddKLine", "OnAddELine", "OnDelGLine", "OnDelZLine", "OnDelKLine", "OnDelELine", "OnDelQLine",
	"OnCleanup", "OnUserPostNick", "OnAccessCheck", "On005Numeric", "OnKill", "OnRemoteKill", "OnLoadModule", "OnUnloadModule",
	"OnBackgroundTimer", "OnSendList", "OnPreCommand", "OnCheckReady", "OnUserRrgister", "OnRawMode", "OnCheckInvite",
	"OnCheckKey", "OnCheckLimit", "OnCheckBan", "OnStats", "OnChangeLocalUserHost", "OnChangeLocalUserGecos", "OnLocalTopicChange",
	"OnPostLocalTopicChange", "OnEvent", "OnRequest", "OnOperCompre", "OnGlobalOper", "OnGlobalConnect", "OnAddBan", "OnDelBan",
	"OnRawSocketAccept", "OnRawSocketClose", "OnRawSocketWrite", "OnRawSocketRead", "OnChangeLocalUserGECOS", "OnUserRegister",
	"OnOperCompare", "OnChannelDelete", "OnPostOper", "OnSyncOtherMetaData", "OnSetAway", "OnCancelAway", NULL
};

void cmd_modules::Handle (char **parameters, int pcnt, userrec *user)
{
  	for (unsigned int i = 0; i < Config->module_names.size(); i++)
	{
		Version V = modules[i]->GetVersion();
		char modulename[MAXBUF];
		char flagstate[MAXBUF];
		*flagstate = 0;
		if (V.Flags & VF_STATIC)
			strlcat(flagstate,", static",MAXBUF);
		if (V.Flags & VF_VENDOR)
			strlcat(flagstate,", vendor",MAXBUF);
		if (V.Flags & VF_COMMON)
			strlcat(flagstate,", common",MAXBUF);
		if (V.Flags & VF_SERVICEPROVIDER)
			strlcat(flagstate,", service provider",MAXBUF);
		if (!flagstate[0])
			strcpy(flagstate,"  <no flags>");
		strlcpy(modulename,Config->module_names[i].c_str(),256);
		if (*user->oper)
		{
			if ((pcnt >= 2) && (!strcasecmp(parameters[0],"debug")))
			{
				if (match(Config->module_names[i].c_str(),parameters[1]))
				{
					WriteServ(user->fd,"900 %s :0x%08lx %d.%d.%d.%d %s (%s)",user->nick,modules[i],V.Major,V.Minor,V.Revision,V.Build,CleanFilename(modulename),flagstate+2);
					for (int it = 0; itab[it];)
					{
						char data[MAXBUF];
						char dlist[MAXBUF];
						*dlist = 0;
						for (int v = 0; v < 4; v++)
						{
							if (itab[it])
							{
								if (Config->implement_lists[i][it])
								{
									snprintf(data,MAXBUF,"%s=>%c ",itab[it],(Config->implement_lists[i][it] ? '1' : '0'));
									strncat(dlist,data,MAXBUF);
								}
								it++;
							}
						}
						if (*dlist)
							WriteServ(user->fd,"900 %s :%s [ %s]",user->nick,CleanFilename(modulename),dlist);
					}
					WriteServ(user->fd,"900 %s :=== DEBUG: Implementation counts ===",user->nick);
					for (int it = 0; itab[it]; it++)
					{
						if (Config->global_implementation[it])
							WriteServ(user->fd,"900 %s :%s: %d times",user->nick, itab[it],(int)Config->global_implementation[it]);
					}
				}
			}
			else
			{
				WriteServ(user->fd,"900 %s :0x%08lx %d.%d.%d.%d %s (%s)",user->nick,modules[i],V.Major,V.Minor,V.Revision,V.Build,CleanFilename(modulename),flagstate+2);
			}
		}
		else
		{
			WriteServ(user->fd,"900 %s :%s",user->nick,CleanFilename(modulename));
		}
	}
	WriteServ(user->fd,"901 %s :End of MODULES list",user->nick);
}
