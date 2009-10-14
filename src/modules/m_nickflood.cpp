/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides channel mode +F (nick flood protection) */

/** Holds settings and state associated with channel mode +F
 */
class nickfloodsettings : public classbase
{
 public:
	int secs;
	int nicks;
	time_t reset;
	time_t unlocktime;
	int counter;
	bool locked;

	nickfloodsettings(int b, int c) : secs(b), nicks(c)
	{
		reset = ServerInstance->Time() + secs;
		counter = 0;
		locked = false;
	};

	void addnick()
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
		/* XXX HACK: using counter + 1 here now to allow the counter to only be incremented
		 * on successful nick changes; this will be checked before the counter is
		 * incremented.
		 */
		return (counter + 1 >= this->nicks);
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
class NickFlood : public ModeHandler
{
 public:
	SimpleExtItem<nickfloodsettings> ext;
	NickFlood(Module* Creator) : ModeHandler(Creator, "nickflood", 'F', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("nickflood", Creator) { }

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		nickfloodsettings* x = ext.get(channel);
		if (x)
			return std::make_pair(true, ConvToStr(x->nicks)+":"+ConvToStr(x->secs));
		else
			return std::make_pair(false, parameter);
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		nickfloodsettings *f = ext.get(channel);
		if (adding)
		{
			char ndata[MAXBUF];
			char* data = ndata;
			strlcpy(ndata,parameter.c_str(),MAXBUF);
			char* nicks = data;
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
				int nnicks = atoi(nicks);
				int nsecs = atoi(secs);
				if ((nnicks<1) || (nsecs<1))
				{
					source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
					parameter.clear();
					return MODEACTION_DENY;
				}
				else
				{
					if (!f)
					{
						parameter = ConvToStr(nnicks) + ":" +ConvToStr(nsecs);
						f = new nickfloodsettings(nsecs, nnicks);
						ext.set(channel, f);
						channel->SetModeParam('F', parameter);
						return MODEACTION_ALLOW;
					}
					else
					{
						std::string cur_param = channel->GetModeParameter('F');
						parameter = ConvToStr(nnicks) + ":" +ConvToStr(nsecs);
						if (cur_param == parameter)
						{
							// mode params match
							return MODEACTION_DENY;
						}
						else
						{
							// new mode param, replace old with new
							if ((nsecs > 0) && (nnicks > 0))
							{
								f = new nickfloodsettings(nsecs, nnicks);
								ext.set(channel, f);
								channel->SetModeParam('F', parameter);
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
			if (f)
			{
				ext.unset(channel);
				channel->SetModeParam('F', "");
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}
};

class ModuleNickFlood : public Module
{
	NickFlood nf;

 public:

	ModuleNickFlood()
		: nf(this)
	{
		if (!ServerInstance->Modes->AddMode(&nf))
			throw ModuleException("Could not add new modes!");
		ServerInstance->Extensions.Register(&nf.ext);
		Implementation eventlist[] = { I_OnUserPreNick, I_OnUserPostNick };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	ModResult OnUserPreNick(User* user, const std::string &newnick)
	{
		if (isdigit(newnick[0])) /* allow switches to UID */
			return MOD_RES_PASSTHRU;

		for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
		{
			Channel *channel = *i;
			ModResult res;

			nickfloodsettings *f = nf.ext.get(channel);
			if (f)
			{
				FIRST_MOD_RESULT(OnChannelRestrictionApply, res, (user,channel,"nickflood"));
				if (res == MOD_RES_ALLOW)
					continue;

				if (f->islocked())
				{
					user->WriteNumeric(447, "%s :%s has been locked for nickchanges for 60 seconds because there have been more than %d nick changes in %d seconds", user->nick.c_str(), channel->name.c_str(), f->nicks, f->secs);
					return MOD_RES_DENY;
				}

				if (f->shouldlock())
				{
					f->clear();
					f->lock();
					channel->WriteChannelWithServ((char*)ServerInstance->Config->ServerName.c_str(), "NOTICE %s :No nick changes are allowed for 60 seconds because there have been more than %d nick changes in %d seconds.", channel->name.c_str(), f->nicks, f->secs);
					return MOD_RES_DENY;
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	/*
	 * XXX: HACK: We do the increment on the *POST* event here (instead of all together) because we have no way of knowing whether other modules would block a nickchange.
	 */
	void OnUserPostNick(User* user, const std::string &oldnick)
	{
		if (isdigit(user->nick[0])) /* allow switches to UID */
			return;

		for (UCListIter i = user->chans.begin(); i != user->chans.end(); ++i)
		{
			Channel *channel = *i;
			ModResult res;

			nickfloodsettings *f = nf.ext.get(channel);
			if (f)
			{
				FIRST_MOD_RESULT(OnChannelRestrictionApply, res, (user,channel,"nickflood"));
				if (res == MOD_RES_ALLOW)
					return;
				
				/* moved this here to avoid incrementing the counter for nick
				 * changes that are denied for some other reason (bans, +N, etc.)
				 * per bug #874.
				 */
				f->addnick();
			}
		}
		return;
	}

	~ModuleNickFlood()
	{
	}

	Version GetVersion()
	{
		return Version("Channel mode F - nick flood protection", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleNickFlood)
