/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for channel mode +N & extban +b N: which prevents nick changes on channel */

class NoNicks : public SimpleChannelModeHandler
{
 public:
	NoNicks(Module* Creator) : SimpleChannelModeHandler(Creator, "nonick", 'N') { }
};

class ModuleNoNickChange : public Module
{
	NoNicks nn;
	bool override;
 public:
	ModuleNoNickChange() : nn(this)
	{
	}

	void init()
	{
		OnRehash(NULL);
		ServerInstance->Modules->AddService(nn);
		Implementation eventlist[] = { I_OnUserPreNick, I_On005Numeric, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleNoNickChange()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for channel mode +N & extban +b N: which prevents nick changes on channel", VF_VENDOR);
	}


	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('N');
	}

	virtual ModResult OnUserPreNick(User* user, const std::string &newnick)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		// Allow forced nick changes.
		if (ServerInstance->NICKForced.get(user))
			return MOD_RES_PASSTHRU;

		for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
		{
			Channel* curr = *i;

			ModResult res;
			FIRST_MOD_RESULT(OnChannelRestrictionApply, res, (user,curr,"nonick"));

			if (res == MOD_RES_ALLOW)
				continue;

			if (override && IS_OPER(user))
				continue;

			if (!curr->GetExtBanStatus(user, 'N').check(!curr->IsModeSet(&nn)))
			{
				user->WriteNumeric(ERR_CANTCHANGENICK, "%s :Can't change nickname while on %s (+N is set)",
					user->nick.c_str(), curr->name.c_str());
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf;
		override = Conf.ReadFlag("nonicks", "operoverride", "no", 0);
	}
};

MODULE_INIT(ModuleNoNickChange)
