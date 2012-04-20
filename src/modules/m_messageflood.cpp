/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
class floodsettings : public classbase
{
 private:
	InspIRCd *ServerInstance;
 public:
	bool ban;
	int secs;
	int lines;
	time_t reset;
	std::map<User*,int> counters;

	floodsettings(InspIRCd *Instance, bool a, int b, int c) : ServerInstance(Instance), ban(a), secs(b), lines(c)
	{
		reset = ServerInstance->Time() + secs;
	};

	void addmessage(User* who)
	{
		std::map<User*,int>::iterator iter = counters.find(who);
		if (iter != counters.end())
		{
			iter->second++;
		}
		else
		{
			counters[who] = 1;
		}
		if (ServerInstance->Time() > reset)
		{
			counters.clear();
			reset = ServerInstance->Time() + secs;
		}
	}

	bool shouldkick(User* who)
	{
		std::map<User*,int>::iterator iter = counters.find(who);
		if (iter != counters.end())
		{
			return (iter->second >= this->lines);
		}
		else return false;
	}

	void clear(User* who)
	{
		std::map<User*,int>::iterator iter = counters.find(who);
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
	MsgFlood(InspIRCd* Instance) : ModeHandler(Instance, 'f', 1, 0, false, MODETYPE_CHANNEL, false) { }

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		floodsettings* x;
		if (channel->GetExt("flood",x))
			return std::make_pair(true, (x->ban ? "*" : "")+ConvToStr(x->lines)+":"+ConvToStr(x->secs));
		else
			return std::make_pair(false, parameter);
	}

	bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, Channel* channel)
	{
		/* When TS is equal, the alphabetically later one wins */
		return (their_param < our_param);
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		floodsettings *f;

		if (adding)
		{
			char ndata[MAXBUF];
			char* data = ndata;
			strlcpy(ndata,parameter.c_str(),MAXBUF);
			char* lines = data;
			char* secs = NULL;
			bool ban = false;
			if (*data == '*')
			{
				ban = true;
				lines++;
			}
			else
			{
				ban = false;
			}
			while (*data)
			{
				if (*data == ':')
				{
					*data = 0;
					data++;
					secs = data;
					break;
				}
				else data++;
			}
			if (secs)
			{
				/* Set up the flood parameters for this channel */
				int nlines = atoi(lines);
				int nsecs = atoi(secs);
				if ((nlines<2) || (nsecs<1))
				{
					source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
					parameter.clear();
					return MODEACTION_DENY;
				}
				else
				{
					if (!channel->GetExt("flood", f))
					{
						parameter = std::string(ban ? "*" : "") + ConvToStr(nlines) + ":" +ConvToStr(nsecs);
						floodsettings *fs = new floodsettings(ServerInstance,ban,nsecs,nlines);
						channel->Extend("flood",fs);
						channel->SetModeParam('f', parameter);
						return MODEACTION_ALLOW;
					}
					else
					{
						std::string cur_param = channel->GetModeParameter('f');
						parameter = std::string(ban ? "*" : "") + ConvToStr(nlines) + ":" +ConvToStr(nsecs);
						if (cur_param == parameter)
						{
							// mode params match
							return MODEACTION_DENY;
						}
						else
						{
							if ((((nlines != f->lines) || (nsecs != f->secs) || (ban != f->ban))) && (((nsecs > 0) && (nlines > 0))))
							{
								delete f;
								floodsettings *fs = new floodsettings(ServerInstance,ban,nsecs,nlines);
								channel->Shrink("flood");
								channel->Extend("flood",fs);
								channel->SetModeParam('f', parameter);
								return MODEACTION_ALLOW;
							}
							else
							{
								return MODEACTION_DENY;
							}
						}
					}
				}
			}
			else
			{
				source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
				parameter.clear();
				return MODEACTION_DENY;
			}
		}
		else
		{
			if (channel->GetExt("flood", f))
			{
				delete f;
				channel->Shrink("flood");
				channel->SetModeParam('f', "");
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleMsgFlood : public Module
{

	MsgFlood* mf;

 public:

	ModuleMsgFlood(InspIRCd* Me)
		: Module(Me)
	{

		mf = new MsgFlood(ServerInstance);
		if (!ServerInstance->Modes->AddMode(mf))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnChannelDelete, I_OnUserPreNotice, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	int ProcessMessages(User* user,Channel* dest, const std::string &text)
	{
		if (!IS_LOCAL(user) || (CHANOPS_EXEMPT(ServerInstance, 'f') && dest->GetStatus(user) == STATUS_OP))
		{
			return 0;
		}

		floodsettings *f;
		if (dest->GetExt("flood", f))
		{
			f->addmessage(user);
			if (f->shouldkick(user))
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
				snprintf(kickmessage, MAXBUF, "Channel flood triggered (limit is %d lines in %d secs)", f->lines, f->secs);

				if (!dest->ServerKickUser(user, kickmessage))
				{
					delete dest;
				}

				return 1;
			}
		}

		return 0;
	}

	virtual int OnUserPreMessage(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
			return ProcessMessages(user,(Channel*)dest,text);

		return 0;
	}

	virtual int OnUserPreNotice(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
			return ProcessMessages(user,(Channel*)dest,text);

		return 0;
	}

	void OnChannelDelete(Channel* chan)
	{
		floodsettings* f;
		if (chan->GetExt("flood", f))
		{
			delete f;
			chan->Shrink("flood");
		}
	}


	virtual ~ModuleMsgFlood()
	{
		ServerInstance->Modes->DelMode(mf);
		delete mf;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleMsgFlood)
