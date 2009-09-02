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

/* $ModDesc: Provides support for Austhex style +k / UnrealIRCD +S services mode */

/** Handles user mode +k
 */
class ServProtectMode : public ModeHandler
{
 public:
	ServProtectMode(InspIRCd* Instance, Module* Creator) : ModeHandler(Instance, Creator, 'k', 0, 0, false, MODETYPE_USER, true) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
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

	bool NeedsOper() { return true; }
};

class ModuleServProtectMode : public Module
{

	ServProtectMode bm;
 public:
	ModuleServProtectMode(InspIRCd* Me)
		: Module(Me), bm(Me, this)
	{
		if (!ServerInstance->Modes->AddMode(&bm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnWhois, I_OnKill, I_OnWhoisLine, I_OnRawMode, I_OnUserPreKick };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}


	virtual ~ModuleServProtectMode()
	{
		ServerInstance->Modes->DelMode(&bm);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

	virtual void OnWhois(User* src, User* dst)
	{
		if (dst->IsModeSet('k'))
		{
			ServerInstance->SendWhoisLine(src, dst, 310, std::string(src->nick)+" "+std::string(dst->nick)+" :is an "+ServerInstance->Config->Network+" Service");
		}
	}

	virtual int OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt, bool servermode)
	{
		/* Check that the mode is not a server mode, it is being removed, the user making the change is local, there is a parameter,
		 * and the user making the change is not a uline
		 */
		if (!servermode && !adding && chan && IS_LOCAL(user) && !param.empty() && !ServerInstance->ULine(user->server))
		{
			/* Check if the parameter is a valid nick/uuid
			 */
			User *u = ServerInstance->FindNick(param);
			if (u)
			{
				/* The target user has +k set on themselves, and you are trying to remove a privilege mode the user has set on themselves.
				 * This includes any prefix permission mode, even those registered in other modules, e.g. +qaohv. Using ::ModeString()
				 * here means that the number of modes is restricted to only modes the user has, limiting it to as short a loop as possible.
				 */
				if (u->IsModeSet('k') && ServerInstance->Modes->ModeString(u, chan, false).find(mode) != std::string::npos)
				{
					/* BZZZT, Denied! */
					user->WriteNumeric(482, "%s %s :You are not permitted to remove privileges from %s services", user->nick.c_str(), chan->name.c_str(), ServerInstance->Config->Network);
					return ACR_DENY;
				}
			}
		}
		/* Mode allowed */
		return 0;
	}

	virtual int OnKill(User* src, User* dst, const std::string &reason)
	{
		if (src == NULL)
			return 0;

		if (dst->IsModeSet('k'))
		{
			src->WriteNumeric(485, "%s :You are not permitted to kill %s services!", src->nick.c_str(), ServerInstance->Config->Network);
			ServerInstance->SNO->WriteGlobalSno('a', std::string(src->nick)+" tried to kill service "+dst->nick+" ("+reason+")");
			return 1;
		}
		return 0;
	}

	virtual int OnUserPreKick(User *src, User *dst, Channel *c, const std::string &reason)
	{
		if (dst->IsModeSet('k'))
		{
			src->WriteNumeric(484, "%s %s :You are not permitted to kick services", src->nick.c_str(), c->name.c_str());
			return 1;
		}

		return 0;
	}

	virtual int OnWhoisLine(User* src, User* dst, int &numeric, std::string &text)
	{
		return ((src != dst) && (numeric == 319) && dst->IsModeSet('k'));
	}
};


MODULE_INIT(ModuleServProtectMode)
