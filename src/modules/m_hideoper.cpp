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

/* $ModDesc: Provides support for hiding oper status with user mode +H */

/** Handles user mode +H
 */
class HideOper : public ModeHandler
{
 public:
	HideOper(Module* Creator) : ModeHandler(Creator, "hideoper", 'H', PARAM_NONE, MODETYPE_USER)
	{
		oper = true;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('H'))
			{
				dest->SetMode('H',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('H'))
			{
				dest->SetMode('H',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleHideOper : public Module
{
	HideOper hm;
 public:
	ModuleHideOper()
		: hm(this)
	{
		if (!ServerInstance->Modes->AddMode(&hm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnWhoisLine, I_OnSendWhoLine };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual ~ModuleHideOper()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for hiding oper status with user mode +H", VF_VENDOR);
	}

	ModResult OnWhoisLine(User* user, User* dest, int &numeric, std::string &text)
	{
		/* Dont display numeric 313 (RPL_WHOISOPER) if they have +H set and the
		 * person doing the WHOIS is not an oper
		 */
		if (numeric != 313)
			return MOD_RES_PASSTHRU;

		if (!dest->IsModeSet('H'))
			return MOD_RES_PASSTHRU;

		if (!user->HasPrivPermission("users/auspex"))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	void OnSendWhoLine(User* source, User* user, Channel* channel, std::string& line)
	{
		if (user->IsModeSet('H') && !source->HasPrivPermission("users/auspex"))
		{
			// hide the "*" that marks the user as an oper from the /WHO line
			std::string::size_type pos = line.find("* ");
			if (pos != std::string::npos)
				line.erase(pos);
		}
	}
};


MODULE_INIT(ModuleHideOper)
