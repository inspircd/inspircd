/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
		int prefixrank;
		OperPrefixMode(Module* Creator, char pfx, int rank) : ModeHandler(Creator, "operprefix", 'y', PARAM_ALWAYS, MODETYPE_CHANNEL)
		{
			list = true;
			prefix = pfx;
			prefixrank = rank;
			levelrequired = INT_MAX;
			m_paramtype = TR_NICK;
			fixed_letter = false;
			oper = true;
		}

		unsigned int GetPrefixRank()
		{
			return prefixrank;
		}

		ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
		{
			return MODEACTION_ALLOW;
		}
};

class ModuleOperPrefixMode : public Module
{
 private:
	OperPrefixMode* opm;
 public:
	ModuleOperPrefixMode() {}

	void init()
	{
		ConfigTag* tag = ServerInstance->Config->GetTag("operprefix");
		std::string pfx = tag->getString("prefix", "!");
		int rank = tag->getInt("rank", OPERPREFIX_VALUE);

		opm = new OperPrefixMode(this, pfx[0], rank);
		ServerInstance->Modules->AddService(*opm);

		Implementation eventlist[] = { I_OnPostJoin, I_OnOper };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void PushChanMode(Channel* channel, User* user)
	{
		irc::modechange mc(opm->id, user->nick, true);
		irc::modestacker ms;
		ms.push(mc);

		ServerInstance->SendMode(ServerInstance->FakeClient, channel, ms, false);
	}

	void OnPostJoin(Membership* memb)
	{
		if (IS_OPER(memb->user) && !memb->user->IsModeSet('H'))
			PushChanMode(memb->chan, memb->user);
	}

	void OnOper(User *user, const std::string&)
	{
		if (user && !user->IsModeSet('H'))
		{
			for (UCListIter v = user->chans.begin(); v != user->chans.end(); v++)
			{
				PushChanMode(v->chan, user);
			}
		}
	}

	~ModuleOperPrefixMode()
	{
		delete opm;
	}

	Version GetVersion()
	{
		return Version("Gives opers cmode +y which provides a staff prefix.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleOperPrefixMode)
