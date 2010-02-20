/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides channel mode +f (message flood protection) */

/** Holds flood settings and state for mode +f
 */
class floodsettings
{
 public:
	bool ban;
	int secs;
	int lines;
	time_t reset;
	std::map<User*,int> counters;

	floodsettings(bool a, int b, int c) : ban(a), secs(b), lines(c)
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
	SimpleExtItem<floodsettings> ext;
	MsgFlood(Module* Creator) : ModeHandler(Creator, "flood", 'f', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("messageflood", Creator) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		floodsettings *f = ext.get(channel);

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
					if (!f)
					{
						parameter = std::string(ban ? "*" : "") + ConvToStr(nlines) + ":" +ConvToStr(nsecs);
						f = new floodsettings(ban,nsecs,nlines);
						ext.set(channel, f);
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
								floodsettings *fs = new floodsettings(ban,nsecs,nlines);
								ext.set(channel, fs);
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
			if (f)
			{
				ext.unset(channel);
				channel->SetModeParam('f', "");
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleMsgFlood : public Module
{
	MsgFlood mf;

 public:

	ModuleMsgFlood()
		: mf(this)
	{
		if (!ServerInstance->Modes->AddMode(&mf))
			throw ModuleException("Could not add new modes!");
		ServerInstance->Extensions.Register(&mf.ext);
		Implementation eventlist[] = { I_OnUserPreNotice, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	ModResult ProcessMessages(User* user,Channel* dest, const std::string &text)
	{
		ModResult res = ServerInstance->OnCheckExemption(user,dest,"flood");
		if (!IS_LOCAL(user) || res == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		floodsettings *f = mf.ext.get(dest);
		if (f)
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

	~ModuleMsgFlood()
	{
	}

	Version GetVersion()
	{
		return Version("Provides channel mode +f (message flood protection)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleMsgFlood)
