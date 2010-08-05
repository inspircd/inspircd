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

/* $ModDesc: Provides support for hiding channels with user mode +I */

/** Handles user mode +I
 */
class HideChans : public ModeHandler
{
 public:
	HideChans(Module* Creator) : ModeHandler(Creator, "hidechans", 'I', PARAM_NONE, MODETYPE_USER) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('I'))
			{
				dest->SetMode('I',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('I'))
			{
				dest->SetMode('I',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleHideChans : public Module
{
	bool AffectsOpers;
	HideChans hm;
 public:
	ModuleHideChans() : hm(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(hm);
		Implementation eventlist[] = { I_OnWhoisLine, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		OnRehash(NULL);
	}

	virtual ~ModuleHideChans()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for hiding channels with user mode +I", VF_VENDOR);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader conf;
		AffectsOpers = conf.ReadFlag("hidechans", "affectsopers", 0);
	}

	ModResult OnWhoisLine(User* user, User* dest, int &numeric, std::string &text)
	{
		/* always show to self */
		if (user == dest)
			return MOD_RES_PASSTHRU;

		/* don't touch anything except 319 */
		if (numeric != 319)
			return MOD_RES_PASSTHRU;

		/* don't touch if -I */
		if (!dest->IsModeSet('I'))
			return MOD_RES_PASSTHRU;

		/* if it affects opers, we don't care if they are opered */
		if (AffectsOpers)
			return MOD_RES_DENY;

		/* doesn't affect opers, sender is opered */
		if (user->HasPrivPermission("users/auspex"))
			return MOD_RES_PASSTHRU;

		/* user must be opered, boned. */
		return MOD_RES_DENY;
	}
};


MODULE_INIT(ModuleHideChans)
