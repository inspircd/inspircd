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

	floodsettings(bool a, int b, int c) : ban(a), secs(b), lines(c)
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

	ModeAction OnSet(User* source, Channel* channel, std::string& parameter)
	{
		std::string::size_type colon = parameter.find(':');
		if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos))
		{
			source->WriteNumeric(608, channel->name, "Invalid flood parameter");
			return MODEACTION_DENY;
		}

		/* Set up the flood parameters for this channel */
		bool ban = (parameter[0] == '*');
		unsigned int nlines = ConvToInt(parameter.substr(ban ? 1 : 0, ban ? colon-1 : colon));
		unsigned int nsecs = ConvToInt(parameter.substr(colon+1));

		if ((nlines<2) || (nsecs<1))
		{
			source->WriteNumeric(608, channel->name, "Invalid flood parameter");
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

class ModuleMsgFlood : public Module
{
	MsgFlood mf;

 public:

	ModuleMsgFlood()
		: mf(this)
	{
	}

	ModResult OnUserPreMessage(User* user, void* voiddest, int target_type, std::string& text, char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if (target_type != TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* dest = static_cast<Channel*>(voiddest);
		if ((!IS_LOCAL(user)) || !dest->IsModeSet(mf))
			return MOD_RES_PASSTHRU;

		if (ServerInstance->OnCheckExemption(user,dest,"flood") == MOD_RES_ALLOW)
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
					changelist.push_add(ServerInstance->Modes->FindMode('b', MODETYPE_CHANNEL), "*!*@" + user->dhost);
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

	void Prioritize()
	{
		// we want to be after all modules that might deny the message (e.g. m_muteban, m_noctcp, m_blockcolor, etc.)
		ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +f (message flood protection)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleMsgFlood)
