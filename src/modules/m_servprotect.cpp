/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2018, 2020-2022, 2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "modules/whois.h"

enum
{
	// From UnrealIRCd.
	ERR_KILLDENY = 485
};

/** Handles user mode +k
 */
class ServProtectMode : public ModeHandler
{
 public:
	ServProtectMode(Module* Creator) : ModeHandler(Creator, "servprotect", 'k', PARAM_NONE, MODETYPE_USER) { oper = true; }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding) CXX11_OVERRIDE
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

class ModuleServProtectMode CXX11_FINAL
	: public Module
	, public Whois::LineEventListener
{
	ServProtectMode bm;
 public:
	ModuleServProtectMode()
		: Whois::LineEventListener(this)
		, bm(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds user mode k (servprotect) which protects services pseudoclients from being kicked, being killed, or having their channel prefix modes changed.", VF_VENDOR);
	}

	ModResult OnRawMode(User* user, Channel* chan, ModeHandler* mh, const std::string& param, bool adding) CXX11_OVERRIDE
	{
		/* Check that the mode is not a server mode, it is being removed, the user making the change is local, there is a parameter,
		 * and the user making the change is not a uline
		 */
		if (!adding && chan && IS_LOCAL(user) && !param.empty())
		{
			const PrefixMode* const pm = mh->IsPrefixMode();
			if (!pm)
				return MOD_RES_PASSTHRU;

			/* Check if the parameter is a valid nick/uuid
			 */
			User *u = ServerInstance->FindNick(param);
			if (u)
			{
				Membership* memb = chan->GetUser(u);
				/* The target user has +k set on themselves, and you are trying to remove a privilege mode the user has set on themselves.
				 * This includes any prefix permission mode, even those registered in other modules, e.g. +qaohv. Using ::ModeString()
				 * here means that the number of modes is restricted to only modes the user has, limiting it to as short a loop as possible.
				 */
				if ((u->IsModeSet(bm)) && (memb) && (memb->HasMode(pm)))
				{
					/* BZZZT, Denied! */
					user->WriteNumeric(ERR_RESTRICTED, chan->name, InspIRCd::Format("You are not permitted to remove privileges from %s services", ServerInstance->Config->Network.c_str()));
					return MOD_RES_DENY;
				}
			}
		}
		/* Mode allowed */
		return MOD_RES_PASSTHRU;
	}

	ModResult OnKill(User* src, User* dst, const std::string &reason) CXX11_OVERRIDE
	{
		if (src == NULL)
			return MOD_RES_PASSTHRU;

		if (dst->IsModeSet(bm))
		{
			src->WriteNumeric(ERR_KILLDENY, InspIRCd::Format("You are not permitted to kill %s services!", ServerInstance->Config->Network.c_str()));
			ServerInstance->SNO->WriteGlobalSno('a', src->nick+" tried to kill service "+dst->nick+" ("+reason+")");
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreKick(User *src, Membership* memb, const std::string &reason) CXX11_OVERRIDE
	{
		if (memb->user->IsModeSet(bm))
		{
			src->WriteNumeric(ERR_RESTRICTED, memb->chan->name, "You are not permitted to kick services");
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnWhoisLine(Whois::Context& whois, Numeric::Numeric& numeric) CXX11_OVERRIDE
	{
		if (numeric.GetNumeric() != RPL_WHOISCHANNELS && numeric.GetNumeric() != RPL_CHANNELSMSG)
			return MOD_RES_PASSTHRU;

		return whois.GetTarget()->IsModeSet(bm) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleServProtectMode)
