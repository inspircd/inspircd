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
#include "hashcomp.h"
#include "configreader.h"

/* $ModDesc: Provides support for channel mode +N which prevents nick changes on channel */

class NoNicks : public ModeHandler
{
 public:
	NoNicks(InspIRCd* Instance) : ModeHandler(Instance, 'N', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('N'))
			{
				channel->SetMode('N',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('N'))
			{
				channel->SetMode('N',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleNoNickChange : public Module
{
	NoNicks* nn;
 public:
	ModuleNoNickChange(InspIRCd* Me)
		: Module(Me)
	{
		
		nn = new NoNicks(ServerInstance);
		ServerInstance->AddMode(nn, 'N');
	}
	
	virtual ~ModuleNoNickChange()
	{
		ServerInstance->Modes->DelMode(nn);
		DELETE(nn);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_COMMON|VF_VENDOR,API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreNick] = 1;
	}

	virtual int OnUserPreNick(userrec* user, const std::string &newnick)
	{
		if (IS_LOCAL(user))
		{
			for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
			{
				chanrec* curr = i->first;

				if (curr->IsModeSet('N'))
				{
					if (CHANOPS_EXEMPT(ServerInstance, 'N') && curr->GetStatus(user) == STATUS_OP)
						continue;

					user->WriteServ("447 %s :Can't change nickname while on %s (+N is set)", user->nick, curr->name);
					return 1;
				}
			}
		}

		return 0;
	}
};

MODULE_INIT(ModuleNoNickChange)
