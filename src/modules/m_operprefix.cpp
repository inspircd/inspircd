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

/* $ModDesc: Gives opers cmode +y which provides a staff prefix. */

#include "inspircd.h"

#define OPERPREFIX_VALUE 1000000

class OperPrefixMode : public ModeHandler
{
	public:
		OperPrefixMode(Module* Creator) : ModeHandler(Creator, "operprefix", 'y', PARAM_ALWAYS, MODETYPE_CHANNEL)
		{
			std::string pfx = ServerInstance->Config->ConfValue("operprefix")->getString("prefix", "!");
			list = true;
			prefix = pfx.empty() ? '!' : pfx[0];
			levelrequired = OPERPREFIX_VALUE;
			m_paramtype = TR_NICK;
			prefixrank = OPERPREFIX_VALUE;
		}

		ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
		{
			if (IS_SERVER(source) || ServerInstance->ULine(source->server))
				return MODEACTION_ALLOW;
			else
			{
				if (channel)
					source->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Only servers are permitted to change channel mode '%c'", source->nick.c_str(), channel->name.c_str(), 'y');
				return MODEACTION_DENY;
			}
		}

		bool NeedsOper() { return true; }
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

 public:
	ModuleOperPrefixMode()
		: opm(this), hideoperwatcher(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Modules->AddService(opm);

		Implementation eventlist[] = { I_OnUserPreJoin, I_OnPostOper };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		/* To give clients a chance to learn about the new prefix we don't give +y to opers
		 * right now. That means if the module was loaded after opers have joined channels
		 * they need to rejoin them in order to get the oper prefix.
		 */

		ServerInstance->Modes->AddModeWatcher(&hideoperwatcher);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if ((user->IsOper()) && (user->IsModeSet('H')))
			privs.push_back('y');
		return MOD_RES_PASSTHRU;
	}

	void SetOperPrefix(User* user, bool add)
	{
		std::vector<std::string> modechange;
		modechange.push_back("");
		modechange.push_back(add ? "+y" : "-y");
		modechange.push_back(user->nick);
		for (UCListIter v = user->chans.begin(); v != user->chans.end(); v++)
		{
			modechange[0] = (*v)->name;
			ServerInstance->Modes->Process(modechange, ServerInstance->FakeClient);
		}
	}

	void OnPostOper(User* user, const std::string& opername, const std::string& opertype) CXX11_OVERRIDE
	{
		if (IS_LOCAL(user) && (!user->IsModeSet('H')))
			SetOperPrefix(user, true);
	}


	~ModuleOperPrefixMode()
	{
		ServerInstance->Modes->DelModeWatcher(&hideoperwatcher);
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
	if (IS_LOCAL(dest))
		parentmod->SetOperPrefix(dest, !adding);
}

MODULE_INIT(ModuleOperPrefixMode)
