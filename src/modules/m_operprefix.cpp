/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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


/*
 * Originally by Chernov-Phoenix Alexey (Phoenix@RusNet) mailto:phoenix /email address separator/ pravmail.ru
 */

#include "inspircd.h"

#define OPERPREFIX_VALUE 1000000

class OperPrefixMode : public PrefixMode
{
	public:
		OperPrefixMode(Module* Creator)
			: PrefixMode(Creator, "operprefix", 'y', OPERPREFIX_VALUE)
		{
			std::string pfx = ServerInstance->Config->ConfValue("operprefix")->getString("prefix", "!");
			prefix = pfx.empty() ? '!' : pfx[0];
			levelrequired = INT_MAX;
		}
};

class ModuleOperPrefixMode;
class HideOperWatcher : public ModeWatcher
{
	ModuleOperPrefixMode* parentmod;

 public:
	HideOperWatcher(ModuleOperPrefixMode* parent);
	void AfterMode(User* source, User* dest, Channel* channel, const std::string &parameter, bool adding);
};

class ModuleOperPrefixMode : public Module
{
	OperPrefixMode opm;
	HideOperWatcher hideoperwatcher;
	UserModeReference hideopermode;

 public:
	ModuleOperPrefixMode()
		: opm(this), hideoperwatcher(this)
		, hideopermode(this, "hideoper")
	{
		/* To give clients a chance to learn about the new prefix we don't give +y to opers
		 * right now. That means if the module was loaded after opers have joined channels
		 * they need to rejoin them in order to get the oper prefix.
		 */
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if ((user->IsOper()) && (!user->IsModeSet(hideopermode)))
			privs.push_back('y');
		return MOD_RES_PASSTHRU;
	}

	void OnPostJoin(Membership* memb)
	{
		if ((!IS_LOCAL(memb->user)) || (!memb->user->IsOper()) || (memb->user->IsModeSet(hideopermode)))
			return;

		if (memb->HasMode(&opm))
			return;

		// The user was force joined and OnUserPreJoin() did not run. Set the operprefix now.
		Modes::ChangeList changelist;
		changelist.push_add(&opm, memb->user->nick);
		ServerInstance->Modes.Process(ServerInstance->FakeClient, memb->chan, NULL, changelist);
	}

	void SetOperPrefix(User* user, bool add)
	{
		Modes::ChangeList changelist;
		changelist.push(&opm, add, user->nick);
		for (User::ChanList::iterator v = user->chans.begin(); v != user->chans.end(); v++)
			ServerInstance->Modes->Process(ServerInstance->FakeClient, (*v)->chan, NULL, changelist);
	}

	void OnPostOper(User* user, const std::string& opername, const std::string& opertype) CXX11_OVERRIDE
	{
		if (IS_LOCAL(user) && (!user->IsModeSet(hideopermode)))
			SetOperPrefix(user, true);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Gives opers cmode +y which provides a staff prefix.", VF_VENDOR);
	}

	void Prioritize()
	{
		// m_opermodes may set +H on the oper to hide him, we don't want to set the oper prefix in that case
		Module* opermodes = ServerInstance->Modules->Find("m_opermodes.so");
		ServerInstance->Modules->SetPriority(this, I_OnPostOper, PRIORITY_AFTER, opermodes);
	}
};

HideOperWatcher::HideOperWatcher(ModuleOperPrefixMode* parent)
	: ModeWatcher(parent, "hideoper", MODETYPE_USER)
	, parentmod(parent)
{
}

void HideOperWatcher::AfterMode(User* source, User* dest, Channel* channel, const std::string& parameter, bool adding)
{
	// If hideoper is being unset because the user is deopering, don't set +y
	if (IS_LOCAL(dest) && dest->IsOper())
		parentmod->SetOperPrefix(dest, !adding);
}

MODULE_INIT(ModuleOperPrefixMode)
