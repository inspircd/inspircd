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

/* $ModDesc: Provides channel mode +f (message flood protection) */

/** Holds flood settings and state for mode +f
 */
class floodsettings
{
 public:
	bool ban;
	unsigned int secs;
	unsigned int lines;
	time_t reset;
	std::map<User*, unsigned int> counters;

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
		std::map<User*, unsigned int>::iterator iter = counters.find(who);
		if (iter != counters.end())
		{
			counters.erase(iter);
		}
	}
};

/** Handles channel mode +f
 */
class MsgFlood : public ModeHandler
{
 public:
	SimpleExtItem<floodsettings> ext;
	MsgFlood(Module* Creator) : ModeHandler(Creator, "flood", 'f', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("messageflood", Creator) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			std::string::size_type colon = parameter.find(':');
			if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos))
			{
				source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
				return MODEACTION_DENY;
			}

			/* Set up the flood parameters for this channel */
			bool ban = (parameter[0] == '*');
			unsigned int nlines = ConvToInt(parameter.substr(ban ? 1 : 0, ban ? colon-1 : colon));
			unsigned int nsecs = ConvToInt(parameter.substr(colon+1));

			if ((nlines<2) || (nsecs<1))
			{
				source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
				return MODEACTION_DENY;
			}

			floodsettings* f = ext.get(channel);
			if ((f) && (nlines == f->lines) && (nsecs == f->secs) && (ban == f->ban))
				// mode params match
				return MODEACTION_DENY;

			ext.set(channel, new floodsettings(ban, nsecs, nlines));
			parameter = std::string(ban ? "*" : "") + ConvToStr(nlines) + ":" + ConvToStr(nsecs);
			channel->SetModeParam('f', parameter);
			return MODEACTION_ALLOW;
		}
		else
		{
			if (!channel->IsModeSet('f'))
				return MODEACTION_DENY;

			ext.unset(channel);
			channel->SetModeParam('f', "");
			return MODEACTION_ALLOW;
		}
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

	void init()
	{
		ServerInstance->Modules->AddService(mf);
		ServerInstance->Modules->AddService(mf.ext);
		Implementation eventlist[] = { I_OnUserPreNotice, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult ProcessMessages(User* user,Channel* dest, const std::string &text)
	{
		if ((!IS_LOCAL(user)) || !dest->IsModeSet('f'))
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
					std::vector<std::string> parameters;
					parameters.push_back(dest->name);
					parameters.push_back("+b");
					parameters.push_back(user->MakeWildHost());
					ServerInstance->SendGlobalMode(parameters, ServerInstance->FakeClient);
				}

				char kickmessage[MAXBUF];
				snprintf(kickmessage, MAXBUF, "Channel flood triggered (limit is %u lines in %u secs)", f->lines, f->secs);

				dest->KickUser(ServerInstance->FakeClient, user, kickmessage);

				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
			return ProcessMessages(user,(Channel*)dest,text);

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNotice(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
			return ProcessMessages(user,(Channel*)dest,text);

		return MOD_RES_PASSTHRU;
	}

	void Prioritize()
	{
		// we want to be after all modules that might deny the message (e.g. m_muteban, m_noctcp, m_blockcolor, etc.)
		ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
		ServerInstance->Modules->SetPriority(this, I_OnUserPreNotice, PRIORITY_LAST);
	}

	Version GetVersion()
	{
		return Version("Provides channel mode +f (message flood protection)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleMsgFlood)
