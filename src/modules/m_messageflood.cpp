/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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
#include "modules/ctctags.h"
#include "modules/exemption.h"

/** Holds flood settings and state for mode +f
 */
class floodsettings
{
 public:
	bool ban;
	unsigned int secs;
	unsigned int lines;
	time_t reset;
	insp::flat_map<User*, unsigned int> counters;

	floodsettings(bool a, unsigned int b, unsigned int c)
		: ban(a)
		, secs(b)
		, lines(c)
	{
		reset = ServerInstance->Time() + secs;
	}

	bool addmessage(User* who)
	{
		if (ServerInstance->Time() > reset)
		{
			counters.clear();
			reset = ServerInstance->Time() + secs;
		}

		return (++counters[who] >= this->lines);
	}

	void clear(User* who)
	{
		counters.erase(who);
	}
};

/** Handles channel mode +f
 */
class MsgFlood : public ParamMode<MsgFlood, SimpleExtItem<floodsettings> >
{
 public:
	MsgFlood(Module* Creator)
		: ParamMode<MsgFlood, SimpleExtItem<floodsettings> >(Creator, "flood", 'f')
	{
	}

	ModeAction OnSet(User* source, Channel* channel, std::string& parameter) CXX11_OVERRIDE
	{
		std::string::size_type colon = parameter.find(':');
		if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return MODEACTION_DENY;
		}

		/* Set up the flood parameters for this channel */
		bool ban = (parameter[0] == '*');
		unsigned int nlines = ConvToNum<unsigned int>(parameter.substr(ban ? 1 : 0, ban ? colon-1 : colon));
		unsigned int nsecs = ConvToNum<unsigned int>(parameter.substr(colon+1));

		if ((nlines<2) || (nsecs<1))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return MODEACTION_DENY;
		}

		ext.set(channel, new floodsettings(ban, nsecs, nlines));
		return MODEACTION_ALLOW;
	}

	void SerializeParam(Channel* chan, const floodsettings* fs, std::string& out)
	{
		if (fs->ban)
			out.push_back('*');
		out.append(ConvToStr(fs->lines)).push_back(':');
		out.append(ConvToStr(fs->secs));
	}
};

class ModuleMsgFlood
	: public Module
	, public CTCTags::EventListener
{
private:
	CheckExemption::EventProvider exemptionprov;
	MsgFlood mf;

 public:
	ModuleMsgFlood()
		: CTCTags::EventListener(this)
		, exemptionprov(this)
		, mf(this)
	{
	}

	ModResult HandleMessage(User* user, const MessageTarget& target)
	{
		if (target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* dest = target.Get<Channel>();
		if ((!IS_LOCAL(user)) || !dest->IsModeSet(mf))
			return MOD_RES_PASSTHRU;

		ModResult res = CheckExemption::Call(exemptionprov, user, dest, "flood");
		if (res == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		floodsettings *f = mf.ext.get(dest);
		if (f)
		{
			if (f->addmessage(user))
			{
				/* Youre outttta here! */
				f->clear(user);
				if (f->ban)
				{
					Modes::ChangeList changelist;
					changelist.push_add(ServerInstance->Modes->FindMode('b', MODETYPE_CHANNEL), "*!*@" + user->GetDisplayedHost());
					ServerInstance->Modes->Process(ServerInstance->FakeClient, dest, NULL, changelist);
				}

				const std::string kickMessage = "Channel flood triggered (trigger is " + ConvToStr(f->lines) +
					" lines in " + ConvToStr(f->secs) + " secs)";

				dest->KickUser(ServerInstance->FakeClient, user, kickMessage);

				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		return HandleMessage(user, target);
	}

	ModResult OnUserPreTagMessage(User* user, const MessageTarget& target, CTCTags::TagMessageDetails& details) CXX11_OVERRIDE
	{
		return HandleMessage(user, target);
	}

	void Prioritize() CXX11_OVERRIDE
	{
		// we want to be after all modules that might deny the message (e.g. m_muteban, m_noctcp, m_blockcolor, etc.)
		ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +f, message flood protection", VF_VENDOR);
	}
};

MODULE_INIT(ModuleMsgFlood)
