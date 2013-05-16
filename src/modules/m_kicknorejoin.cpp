/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 John Brooks <john.brooks@dereferenced.net>
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

/* $ModDesc: Provides channel mode +J (delay rejoin after kick) */

typedef std::map<std::string, time_t> delaylist;

/** Handles channel mode +J
 */
class KickRejoin : public ModeHandler
{
 public:
	unsigned int max;
	SimpleExtItem<delaylist> ext;
	KickRejoin(Module* Creator)
		: ModeHandler(Creator, "kicknorejoin", 'J', PARAM_SETONLY, MODETYPE_CHANNEL)
		, ext("norejoinusers", Creator)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding)
	{
		if (adding)
		{
			int v = ConvToInt(parameter);
			if (v <= 0)
				return MODEACTION_DENY;
			if (parameter == channel->GetModeParameter(this))
				return MODEACTION_DENY;

			if ((IS_LOCAL(source) && ((unsigned int)v > max)))
				v = max;

			parameter = ConvToStr(v);
			channel->SetModeParam(this, parameter);
		}
		else
		{
			if (!channel->IsModeSet(this))
				return MODEACTION_DENY;

			ext.unset(channel);
			channel->SetModeParam(this, "");
		}
		return MODEACTION_ALLOW;
	}
};

class ModuleKickNoRejoin : public Module
{
	KickRejoin kr;

public:

	ModuleKickNoRejoin()
		: kr(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(kr);
		ServerInstance->Modules->AddService(kr.ext);
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnUserKick, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	void OnRehash(User* user)
	{
		kr.max = ServerInstance->Duration(ServerInstance->Config->ConfValue("kicknorejoin")->getString("maxtime"));
		if (!kr.max)
			kr.max = 30*60;
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
		{
			delaylist* dl = kr.ext.get(chan);
			if (dl)
			{
				for (delaylist::iterator iter = dl->begin(); iter != dl->end(); )
				{
					if (iter->second > ServerInstance->Time())
					{
						if (iter->first == user->uuid)
						{
							std::string modeparam = chan->GetModeParameter(&kr);
							user->WriteNumeric(ERR_DELAYREJOIN, "%s %s :You must wait %s seconds after being kicked to rejoin (+J)",
								user->nick.c_str(), chan->name.c_str(), modeparam.c_str());
							return MOD_RES_DENY;
						}
						++iter;
					}
					else
					{
						// Expired record, remove.
						dl->erase(iter++);
					}
				}

				if (dl->empty())
					kr.ext.unset(chan);
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& excepts)
	{
		if (memb->chan->IsModeSet(&kr) && (IS_LOCAL(memb->user)) && (source != memb->user))
		{
			delaylist* dl = kr.ext.get(memb->chan);
			if (!dl)
			{
				dl = new delaylist;
				kr.ext.set(memb->chan, dl);
			}
			(*dl)[memb->user->uuid] = ServerInstance->Time() + ConvToInt(memb->chan->GetModeParameter(&kr));
		}
	}

	~ModuleKickNoRejoin()
	{
	}

	Version GetVersion()
	{
		return Version("Channel mode to delay rejoin after kick", VF_VENDOR);
	}
};


MODULE_INIT(ModuleKickNoRejoin)
