/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

static bool kludgeme = false;

/* $ModDesc: Povides support for services +r user/chan modes and more */

/** Channel mode +r - mark a channel as identified
 */
class Channel_r : public ModeHandler
{
	
 public:
	Channel_r(InspIRCd* Instance) : ModeHandler(Instance, 'r', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		// only a u-lined server may add or remove the +r mode.
		if ((ServerInstance->ULine(source->nick)) || (ServerInstance->ULine(source->server)) || (!*source->server || (strchr(source->nick,'.'))))
		{
			channel->SetMode('r',adding);
			return MODEACTION_ALLOW;
		}
		else
		{
			source->WriteServ("500 %s :Only a server may modify the +r channel mode", source->nick);
			return MODEACTION_DENY;
		}
	}
};

/** User mode +r - mark a user as identified
 */
class User_r : public ModeHandler
{
	
 public:
	User_r(InspIRCd* Instance) : ModeHandler(Instance, 'r', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if ((kludgeme) || (ServerInstance->ULine(source->nick)) || (ServerInstance->ULine(source->server)) || (!*source->server || (strchr(source->nick,'.'))))
		{
			if ((adding && !dest->IsModeSet('r')) || (!adding && dest->IsModeSet('r')))
			{
				dest->SetMode('r',adding);
				return MODEACTION_ALLOW;
			}
			return MODEACTION_DENY;
		}
		else
		{
			source->WriteServ("500 %s :Only a server may modify the +r user mode", source->nick);
			return MODEACTION_DENY;
		}
	}
};

/** Channel mode +R - registered users only
 */
class Channel_R : public ModeHandler
{
 public:
	Channel_R(InspIRCd* Instance) : ModeHandler(Instance, 'R', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('R'))
			{
				channel->SetMode('R',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('R'))
			{
				channel->SetMode('R',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

/** User mode +R - only allow PRIVMSG and NOTICE from registered users
 */
class User_R : public ModeHandler
{
 public:
	User_R(InspIRCd* Instance) : ModeHandler(Instance, 'R', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('R'))
			{
				dest->SetMode('R',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('R'))
			{
				dest->SetMode('R',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

/** Channel mode +M - only allow privmsg and notice to channel from registered users
 */
class Channel_M : public ModeHandler
{
 public:
	Channel_M(InspIRCd* Instance) : ModeHandler(Instance, 'M', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('M'))
			{
				channel->SetMode('M',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('M'))
			{
				channel->SetMode('M',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

/** Dreamnforge-like services support
 */
class ModuleServices : public Module
{
	
	Channel_r* m1;
	Channel_R* m2;
	Channel_M* m3;
	User_r* m4;
	User_R* m5;
 public:
	ModuleServices(InspIRCd* Me)
		: Module(Me)
	{
		
		m1 = new Channel_r(ServerInstance);
		m2 = new Channel_R(ServerInstance);
		m3 = new Channel_M(ServerInstance);
		m4 = new User_r(ServerInstance);
		m5 = new User_R(ServerInstance);

		if (!ServerInstance->AddMode(m1) || !ServerInstance->AddMode(m2) || !ServerInstance->AddMode(m3)
			|| !ServerInstance->AddMode(m4) || !ServerInstance->AddMode(m5))
		{
			throw ModuleException("Could not add user and channel modes!");
		}
	
		kludgeme = false;
		Implementation eventlist[] = { I_OnWhois, I_OnUserPostNick, I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}

	/* <- :stitch.chatspike.net 307 w00t w00t :is a registered nick */
	virtual void OnWhois(User* source, User* dest)
	{
		if (dest->IsModeSet('r'))
		{
			/* user is registered */
			ServerInstance->SendWhoisLine(source, dest, 307, "%s %s :is a registered nick", source->nick, dest->nick);
		}
	}


	virtual void OnUserPostNick(User* user, const std::string &oldnick)
	{
		/* On nickchange, if they have +r, remove it */
		if (user->IsModeSet('r') && irc::string(user->nick) != oldnick)
		{
			const char* modechange[2];
			modechange[0] = user->nick;
			modechange[1] = "-r";
			kludgeme = true;
			ServerInstance->SendMode(modechange,2,user);
			kludgeme = false;
		}
	}
	
	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)dest;
			if ((c->IsModeSet('M')) && (!user->IsModeSet('r')))
			{
				if ((ServerInstance->ULine(user->nick)) || (ServerInstance->ULine(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user messaging a +M channel and is not registered
				user->WriteServ("477 %s %s :You need a registered nickname to speak on this channel", user->nick, c->name);
				return 1;
			}
		}
		if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;
			if ((u->IsModeSet('R')) && (!user->IsModeSet('r')))
			{
				if ((ServerInstance->ULine(user->nick)) || (ServerInstance->ULine(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user messaging a +R user and is not registered
				user->WriteServ("477 %s %s :You need a registered nickname to message this user", user->nick, u->nick);
				return 1;
			}
		}
		return 0;
	}
 	
	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status, exempt_list);
	}
 	
	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs)
	{
		if (chan)
		{
			if (chan->IsModeSet('R'))
			{
				if (!user->IsModeSet('r'))
				{
					if ((ServerInstance->ULine(user->nick)) || (ServerInstance->ULine(user->server)))
					{
						// user is ulined, won't be stopped from joining
						return 0;
					}
					// joining a +R channel and not identified
					user->WriteServ("477 %s %s :You need a registered nickname to join this channel", user->nick, chan->name);
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleServices()
	{
		kludgeme = true;
		ServerInstance->Modes->DelMode(m1);
		ServerInstance->Modes->DelMode(m2);
		ServerInstance->Modes->DelMode(m3);
		ServerInstance->Modes->DelMode(m4);
		ServerInstance->Modes->DelMode(m5);
		delete m1;
		delete m2;
		delete m3;
		delete m4;
		delete m5;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};


MODULE_INIT(ModuleServices)
