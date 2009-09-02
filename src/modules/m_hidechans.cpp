/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
	HideChans(InspIRCd* Instance) : ModeHandler(Instance, 'I', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
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
	ModuleHideChans(InspIRCd* Me) : Module(Me), hm(Me)
	{
		if (!ServerInstance->Modes->AddMode(&hm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnWhoisLine, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		OnRehash(NULL);
	}

	virtual ~ModuleHideChans()
	{
		ServerInstance->Modes->DelMode(&hm);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader conf(ServerInstance);
		AffectsOpers = conf.ReadFlag("hidechans", "affectsopers", 0);
	}

	int OnWhoisLine(User* user, User* dest, int &numeric, std::string &text)
	{
		/* always show to self */
		if (user == dest)
			return 0;

		/* don't touch anything except 319 */
		if (numeric != 319)
			return 0;

		/* don't touch if -I */
		if (!dest->IsModeSet('I'))
			return 0;

		/* if it affects opers, we don't care if they are opered */
		if (AffectsOpers)
			return 1;

		/* doesn't affect opers, sender is opered */
		if (user->HasPrivPermission("users/auspex"))
			return 0;

		/* user must be opered, boned. */
		return 1;
	}
};


MODULE_INIT(ModuleHideChans)
