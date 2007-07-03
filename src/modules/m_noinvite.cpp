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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for unreal-style channel mode +V */

class NoInvite : public ModeHandler
{
 public:
	NoInvite(InspIRCd* Instance) : ModeHandler(Instance, 'V', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('V'))
			{
				channel->SetMode('V',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('V'))
			{
				channel->SetMode('V',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleNoInvite : public Module
{
	NoInvite *ni;
 public:

	ModuleNoInvite(InspIRCd* Me) : Module(Me)
	{
		ni = new NoInvite(ServerInstance);
		if (!ServerInstance->AddMode(ni, 'V'))
			throw ModuleException("Could not add new modes!");
	}

	void Implements(char* List)
	{
		List[I_OnUserPreInvite] = 1;
	}

	virtual int OnUserPreInvite(userrec* user,userrec* dest,chanrec* channel)
	{
		if (channel->IsModeSet('V'))
		{
			user->WriteServ("492 %s %s :Can't invite %s to channel (+V set)",user->nick, channel->name, dest->nick);
			return 1;
		}
		return 0;
	}

	virtual ~ModuleNoInvite()
	{
		ServerInstance->Modes->DelMode(ni);
		DELETE(ni);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleNoInvite)
