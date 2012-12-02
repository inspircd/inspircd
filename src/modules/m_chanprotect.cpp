/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2004-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

/* $ModDesc: Provides channel modes +a and +q */

#define PROTECT_VALUE 40000
#define FOUNDER_VALUE 50000

struct ChanProtectSettings
{
	bool DeprivSelf;
	bool DeprivOthers;
	bool FirstInGetsFounder;
	bool booting;
	ChanProtectSettings() : booting(true) {}
};

static ChanProtectSettings settings;

/** Handles basic operation of +qa channel modes
 */
class FounderProtectBase
{
 private:
	const std::string type;
	const char mode;
	const int list;
	const int end;
 public:
	FounderProtectBase(char Mode, const std::string &mtype, int l, int e) :
		type(mtype), mode(Mode), list(l), end(e)
	{
	}

	void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		const UserMembList* cl = channel->GetUsers();
		std::vector<std::string> mode_junk;
		mode_junk.push_back(channel->name);
		irc::modestacker modestack(false);
		std::deque<std::string> stackresult;

		for (UserMembCIter i = cl->begin(); i != cl->end(); i++)
		{
			if (i->second->hasMode(mode))
			{
				if (stack)
					stack->Push(mode, i->first->nick);
				else
					modestack.Push(mode, i->first->nick);
			}
		}

		if (stack)
			return;

		while (modestack.GetStackedLine(stackresult))
		{
			mode_junk.insert(mode_junk.end(), stackresult.begin(), stackresult.end());
			ServerInstance->SendMode(mode_junk, ServerInstance->FakeClient);
			mode_junk.erase(mode_junk.begin() + 1, mode_junk.end());
		}
	}

	void DisplayList(User* user, Channel* channel)
	{
		const UserMembList* cl = channel->GetUsers();
		for (UserMembCIter i = cl->begin(); i != cl->end(); ++i)
		{
			if (i->second->hasMode(mode))
			{
				user->WriteServ("%d %s %s %s", list, user->nick.c_str(), channel->name.c_str(), i->first->nick.c_str());
			}
		}
		user->WriteServ("%d %s %s :End of channel %s list", end, user->nick.c_str(), channel->name.c_str(), type.c_str());
	}

	bool CanRemoveOthers(User* u1, Channel* c)
	{
		Membership* m1 = c->GetUser(u1);
		return (settings.DeprivOthers && m1 && m1->hasMode(mode));
	}
};

/** Abstraction of FounderProtectBase for channel mode +q
 */
class ChanFounder : public ModeHandler, public FounderProtectBase
{
 public:
	ChanFounder(Module* Creator)
		: ModeHandler(Creator, "founder", 'q', PARAM_ALWAYS, MODETYPE_CHANNEL),
		  FounderProtectBase('q', "founder", 386, 387)
	{
		ModeHandler::list = true;
		levelrequired = FOUNDER_VALUE;
		m_paramtype = TR_NICK;
	}

	void setPrefix(int pfx)
	{
		prefix = pfx;
	}

	unsigned int GetPrefixRank()
	{
		return FOUNDER_VALUE;
	}

	void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		FounderProtectBase::RemoveMode(channel, stack);
	}

	void RemoveMode(User* user, irc::modestacker* stack)
	{
	}
	
	ModResult AccessCheck(User* source, Channel* channel, std::string &parameter, bool adding)
	{
		User* theuser = ServerInstance->FindNick(parameter);
		// remove own privs?
		if (source == theuser && !adding && settings.DeprivSelf)
			return MOD_RES_ALLOW;

		if (!adding && FounderProtectBase::CanRemoveOthers(source, channel))
		{
			return MOD_RES_PASSTHRU;
		}
		else
		{
			source->WriteNumeric(468, "%s %s :Only servers may set channel mode +q", source->nick.c_str(), channel->name.c_str());
			return MOD_RES_DENY;
		}
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		return MODEACTION_ALLOW;
	}

	void DisplayList(User* user, Channel* channel)
	{
		FounderProtectBase::DisplayList(user,channel);
	}
};

/** Abstraction of FounderProtectBase for channel mode +a
 */
class ChanProtect : public ModeHandler, public FounderProtectBase
{
 public:
	ChanProtect(Module* Creator)
		: ModeHandler(Creator, "admin", 'a', PARAM_ALWAYS, MODETYPE_CHANNEL),
		  FounderProtectBase('a',"protected user", 388, 389)
	{
		ModeHandler::list = true;
		levelrequired = PROTECT_VALUE;
		m_paramtype = TR_NICK;
	}

	void setPrefix(int pfx)
	{
		prefix = pfx;
	}


	unsigned int GetPrefixRank()
	{
		return PROTECT_VALUE;
	}

	void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		FounderProtectBase::RemoveMode(channel, stack);
	}

	void RemoveMode(User* user, irc::modestacker* stack)
	{
	}

	ModResult AccessCheck(User* source, Channel* channel, std::string &parameter, bool adding)
	{
		User* theuser = ServerInstance->FindNick(parameter);
		// source has +q
		if (channel->GetPrefixValue(source) > PROTECT_VALUE)
			return MOD_RES_ALLOW;

		// removing own privs?
		if (source == theuser && !adding && settings.DeprivSelf)
			return MOD_RES_ALLOW;

		if (!adding && FounderProtectBase::CanRemoveOthers(source, channel))
		{
			return MOD_RES_PASSTHRU;
		}
		else
		{
			source->WriteNumeric(482, "%s %s :You are not a channel founder", source->nick.c_str(), channel->name.c_str());
			return MOD_RES_DENY;
		}
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		return MODEACTION_ALLOW;
	}

	void DisplayList(User* user, Channel* channel)
	{
		FounderProtectBase::DisplayList(user, channel);
	}

};

class ModuleChanProtect : public Module
{
	ChanProtect cp;
	ChanFounder cf;
 public:
	ModuleChanProtect() : cp(this), cf(this)
	{
	}

	void init()
	{
		/* Load config stuff */
		LoadSettings();
		settings.booting = false;

		ServerInstance->Modules->AddService(cf);
		ServerInstance->Modules->AddService(cp);

		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void LoadSettings()
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("chanprotect");

		settings.FirstInGetsFounder = tag->getBool("noservices");

		std::string qpre = tag->getString("qprefix");
		char QPrefix = qpre.empty() ? 0 : qpre[0];

		std::string apre = tag->getString("aprefix");
		char APrefix = apre.empty() ? 0 : apre[0];

		if ((APrefix && QPrefix) && APrefix == QPrefix)
			throw ModuleException("What the smeg, why are both your +q and +a prefixes the same character?");

		if (settings.booting)
		{
			if (APrefix && ServerInstance->Modes->FindPrefix(APrefix) && ServerInstance->Modes->FindPrefix(APrefix) != &cp)
				throw ModuleException("Looks like the +a prefix you picked for m_chanprotect is already in use. Pick another.");

			if (QPrefix && ServerInstance->Modes->FindPrefix(QPrefix) && ServerInstance->Modes->FindPrefix(QPrefix) != &cf)
				throw ModuleException("Looks like the +q prefix you picked for m_chanprotect is already in use. Pick another.");

			cp.setPrefix(APrefix);
			cf.setPrefix(QPrefix);
		}
		settings.DeprivSelf = tag->getBool("deprotectself", true);
		settings.DeprivOthers = tag->getBool("deprotectothers", true);
	}

	ModResult OnUserPreJoin(User *user, Channel *chan, const char *cname, std::string &privs, const std::string &keygiven)
	{
		// if the user is the first user into the channel, mark them as the founder, but only if
		// the config option for it is set

		if (settings.FirstInGetsFounder && !chan)
			privs += 'q';

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Founder and Protect modes (+qa)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanProtect)
