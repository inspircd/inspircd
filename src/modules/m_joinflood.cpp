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

/* $ModDesc: Provides channel mode +j (join flood protection) */

/** Holds settings and state associated with channel mode +j
 */
class joinfloodsettings
{
 public:
	int secs;
	int joins;
	time_t reset;
	time_t unlocktime;
	int counter;
	bool locked;

	joinfloodsettings(int b, int c) : secs(b), joins(c)
	{
		reset = ServerInstance->Time() + secs;
		counter = 0;
		locked = false;
	};

	void addjoin()
	{
		counter++;
		if (ServerInstance->Time() > reset)
		{
			counter = 0;
			reset = ServerInstance->Time() + secs;
		}
	}

	bool shouldlock()
	{
		return (counter >= this->joins);
	}

	void clear()
	{
		counter = 0;
	}

	bool islocked()
	{
		if (locked)
		{
			if (ServerInstance->Time() > unlocktime)
			{
				locked = false;
				return false;
			}
			else
			{
				return true;
			}
		}
		return false;
	}

	void lock()
	{
		locked = true;
		unlocktime = ServerInstance->Time() + 60;
	}

};

/** Handles channel mode +j
 */
class JoinFlood : public ModeHandler
{
 public:
	SimpleExtItem<joinfloodsettings> ext;
	JoinFlood(Module* Creator) : ModeHandler(Creator, "joinflood", 'j', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("joinflood", Creator) { fixed_letter = false; }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			char ndata[MAXBUF];
			char* data = ndata;
			strlcpy(ndata,parameter.c_str(),MAXBUF);
			char* joins = data;
			char* secs = NULL;
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
				int njoins = atoi(joins);
				int nsecs = atoi(secs);
				if ((njoins<1) || (nsecs<1))
				{
					source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
					parameter.clear();
					return MODEACTION_DENY;
				}
				else
				{
					joinfloodsettings* f = ext.get(channel);
					if (!f)
					{
						parameter = ConvToStr(njoins) + ":" +ConvToStr(nsecs);
						f = new joinfloodsettings(nsecs, njoins);
						ext.set(channel, f);
						channel->SetModeParam(this, parameter);
						return MODEACTION_ALLOW;
					}
					else
					{
						std::string cur_param = channel->GetModeParameter(this);
						parameter = ConvToStr(njoins) + ":" +ConvToStr(nsecs);
						if (cur_param == parameter)
						{
							// mode params match
							return MODEACTION_DENY;
						}
						else
						{
							// new mode param, replace old with new
							if ((nsecs > 0) && (njoins > 0))
							{
								f = new joinfloodsettings(nsecs, njoins);
								ext.set(channel, f);
								channel->SetModeParam(this, parameter);
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
				return MODEACTION_DENY;
			}
		}
		else
		{
			joinfloodsettings* f = ext.get(channel);
			if (f)
			{
				ext.unset(channel);
				channel->SetModeParam(this, "");
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}
};

class ModuleJoinFlood : public Module
{

	JoinFlood jf;

 public:

	ModuleJoinFlood()
		: jf(this)
	{

		ServerInstance->Modules->AddService(jf);
		ServerInstance->Extensions.Register(&jf.ext);
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnUserJoin };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const std::string& cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
		{
			joinfloodsettings *f = jf.ext.get(chan);
			if (f && f->islocked())
			{
				user->WriteNumeric(609, "%s %s :This channel is temporarily unavailable (+j). Please try again later.",user->nick.c_str(),chan->name.c_str());
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& excepts)
	{
		/* We arent interested in JOIN events caused by a network burst */
		if (sync)
			return;

		joinfloodsettings *f = jf.ext.get(memb->chan);

		/* But all others are OK */
		if (f)
		{
			f->addjoin();
			if (f->shouldlock())
			{
				f->clear();
				f->lock();
				memb->chan->WriteChannelWithServ((char*)ServerInstance->Config->ServerName.c_str(), "NOTICE %s :This channel has been closed to new users for 60 seconds because there have been more than %d joins in %d seconds.", memb->chan->name.c_str(), f->joins, f->secs);
			}
		}
	}

	~ModuleJoinFlood()
	{
	}

	Version GetVersion()
	{
		return Version("Provides channel mode +j (join flood protection)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleJoinFlood)
