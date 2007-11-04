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
#include "wildcard.h"
#include "commands/cmd_modules.h"

char* itab[] = {
	"OnUserConnect", "OnUserQuit", "OnUserDisconnect", "OnUserJoin", "OnUserPart", "OnRehash", "OnServerRaw",
	"OnUserPreJoin", "OnUserPreKick", "OnUserKick", "OnOper", "OnInfo", "OnWhois", "OnUserPreInvite",
	"OnUserInvite", "OnUserPreMessage", "OnUserPreNotice", "OnUserPreNick", "OnUserMessage", "OnUserNotice", "OnMode",
	"OnGetServerDescription", "OnSyncUser", "OnSyncChannel", "OnSyncChannelMetaData", "OnSyncUserMetaData",
	"OnDecodeMetaData", "ProtoSendMode", "ProtoSendMetaData", "OnWallops", "OnChangeHost", "OnChangeName", "OnAddGLine",
	"OnAddZLine", "OnAddQLine", "OnAddKLine", "OnAddELine", "OnDelGLine", "OnDelZLine", "OnDelKLine", "OnDelELine", "OnDelQLine",
	"OnCleanup", "OnUserPostNick", "OnAccessCheck", "On005Numeric", "OnKill", "OnRemoteKill", "OnLoadModule", "OnUnloadModule",
	"OnBackgroundTimer", "OnSendList", "OnPreCommand", "OnCheckReady", "OnUserRegister", "OnCheckInvite",
	"OnCheckKey", "OnCheckLimit", "OnCheckBan", "OnStats", "OnChangeLocalUserHost", "OnChangeLocalUserGecos", "OnLocalTopicChange",
	"OnPostLocalTopicChange", "OnEvent", "OnRequest", "OnOperCompre", "OnGlobalOper", "OnPostConnect", "OnAddBan", "OnDelBan",
	"OnRawSocketAccept", "OnRawSocketClose", "OnRawSocketWrite", "OnRawSocketRead", "OnChangeLocalUserGECOS", "OnUserRegister",
	"OnOperCompare", "OnChannelDelete", "OnPostOper", "OnSyncOtherMetaData", "OnSetAway", "OnCancelAway", "OnNamesList",
	"OnPostCommand", "OnPostJoin", "OnWhoisLine", "OnBuildExemptList", "OnRawSocketConnect", "OnGarbageCollect", NULL
};

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandModules(Instance);
}

/** Handle /MODULES
 */
CmdResult CommandModules::Handle (const char**, int, User *user)
{
	std::vector<std::string> module_names = ServerInstance->Modules->GetAllModuleNames(0);

  	for (unsigned int i = 0; i < module_names.size(); i++)
	{
		Module* m = ServerInstance->Modules->Find(module_names[i]);
		Version V = m->GetVersion();
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
		strlcpy(modulename,module_names[i].c_str(),256);
		if (IS_OPER(user))
		{
			user->WriteServ("900 %s :0x%08lx %d.%d.%d.%d %s (%s)",user->nick,m,V.Major,V.Minor,V.Revision,V.Build,ServerConfig::CleanFilename(modulename),flagstate+2);
		}
		else
		{
			user->WriteServ("900 %s :%s",user->nick,ServerConfig::CleanFilename(modulename));
		}
	}
	user->WriteServ("901 %s :End of MODULES list",user->nick);

	return CMD_SUCCESS;
}
