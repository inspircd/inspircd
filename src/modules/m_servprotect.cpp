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

/* $ModDesc: Provides support for Austhex style +k / UnrealIRCD +S services mode */

/** Handles user mode +k
 */
class ServProtectMode : public ModeHandler
{
 public:
	ServProtectMode(Module* Creator) : ModeHandler(Creator, "servprotect", 'k', PARAM_NONE, MODETYPE_USER) { oper = true; }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		/* Because this returns MODEACTION_DENY all the time, there is only ONE
		 * way to add this mode and that is at client introduction in the UID command,
		 * as this calls OnModeChange for each mode but disregards the return values.
		 * The mode cannot be manually added or removed, not even by a server or by a remote
		 * user or uline, which prevents its (ab)use as a kiddie 'god mode' on such networks.
		 * I'm sure if someone really wants to do that they can make a copy of this module
		 * that does the job. It won't be me though!
		 */
		return MODEACTION_DENY;
	}
};

class ModuleServProtectMode : public Module
{
	ServProtectMode bm;
 public:
	ModuleServProtectMode()
		: bm(this)
	{
		ServerInstance->Modules->AddService(bm);
		Implementation eventlist[] = { I_OnWhois, I_OnKill, I_OnWhoisLine, I_OnRawMode, I_OnPermissionCheck };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}


	~ModuleServProtectMode()
	{
	}

	Version GetVersion()
	{
		return Version("Provides support for Austhex style +k / UnrealIRCD +S services mode", VF_VENDOR);
	}

	void OnWhois(User* src, User* dst)
	{
		if (dst->IsModeSet('k'))
		{
			ServerInstance->SendWhoisLine(src, dst, 310, std::string(src->nick)+" "+std::string(dst->nick)+" :is an "+ServerInstance->Config->Network+" Service");
		}
	}

	ModResult OnRawMode(User* user, Channel* chan, irc::modechange& mc)
	{
		/* Check that the mode is not a server mode, it is being removed, the user making the change is local, there is a parameter,
		 * and the user making the change is not a uline
		 */
		if (!mc.adding && chan && IS_LOCAL(user) && !ServerInstance->ULine(user->server))
		{
			ModeHandler* mh = ServerInstance->Modes->FindMode(mc.mode);
			if (mh->GetTranslateType() != TR_NICK)
				return MOD_RES_PASSTHRU;

			/* Check if the parameter is a valid nick/uuid
			 */
			User *u = ServerInstance->FindNick(mc.value);
			if (u)
			{
				/* The target user has +k set on themselves, and you are trying to remove a privilege mode the user has set on themselves.
				 * This includes any prefix permission mode, even those registered in other modules, e.g. +qaohv. Using ::ModeString()
				 * here means that the number of modes is restricted to only modes the user has, limiting it to as short a loop as possible.
				 */
				if (u->IsModeSet('k'))
				{
					/* BZZZT, Denied! */
					user->WriteNumeric(482, "%s %s :You are not permitted to remove privileges from %s services", user->nick.c_str(), chan->name.c_str(), ServerInstance->Config->Network.c_str());
					return MOD_RES_DENY;
				}
			}
		}
		/* Mode allowed */
		return MOD_RES_PASSTHRU;
	}

	ModResult OnKill(User* src, User* dst, const std::string &reason)
	{
		if (src == NULL)
			return MOD_RES_PASSTHRU;

		if (dst->IsModeSet('k'))
		{
			src->WriteNumeric(485, "%s :You are not permitted to kill %s services!", src->nick.c_str(), ServerInstance->Config->Network.c_str());
			ServerInstance->SNO->WriteGlobalSno('a', std::string(src->nick)+" tried to kill service "+dst->nick+" ("+reason+")");
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		if (perm.name != "kick")
			return;
		if (perm.user->IsModeSet('k'))
		{
			perm.ErrorNumeric(484, "%s :You are not permitted to kick services", perm.chan->name.c_str());
			perm.result = MOD_RES_DENY;
		}
	}

	ModResult OnWhoisLine(User* src, User* dst, int &numeric, std::string &text)
	{
		return ((src != dst) && (numeric == 319) && dst->IsModeSet('k')) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}
};


MODULE_INIT(ModuleServProtectMode)
