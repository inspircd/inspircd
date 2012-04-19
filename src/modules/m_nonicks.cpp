/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for channel mode +N & extban +b N: which prevents nick changes on channel */

class NoNicks : public ModeHandler
{
 public:
	NoNicks(InspIRCd* Instance) : ModeHandler(Instance, 'N', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
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
	ModuleNoNickChange(InspIRCd* Me) : Module(Me)
	{

		nn = new NoNicks(ServerInstance);
		ServerInstance->Modes->AddMode(nn);
		Implementation eventlist[] = { I_OnUserPreNick, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleNoNickChange()
	{
		ServerInstance->Modes->DelMode(nn);
		delete nn;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}


	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('N');
	}

	virtual int OnUserPreNick(User* user, const std::string &newnick)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (isdigit(newnick[0])) /* don't even think about touching a switch to uid! */
			return 0;

		// Allow forced nick changes.
		if (user->GetExt("NICKForced"))
			return 0;

		for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
		{
			Channel* curr = i->first;

			if (CHANOPS_EXEMPT(ServerInstance, 'N') && curr->GetStatus(user) == STATUS_OP)
				continue;

			if (curr->IsModeSet('N') || curr->GetExtBanStatus(user, 'N') < 0)
			{
				user->WriteNumeric(ERR_CANTCHANGENICK, "%s :Can't change nickname while on %s (+N is set)", user->nick.c_str(), curr->name.c_str());
				return 1;
			}
		}

		return 0;
	}
};

MODULE_INIT(ModuleNoNickChange)
