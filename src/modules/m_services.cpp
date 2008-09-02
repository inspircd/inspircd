/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
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

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		// only a u-lined server may add or remove the +r mode.
		if ((ServerInstance->ULine(source->nick.c_str())) || (ServerInstance->ULine(source->server)) || (!*source->server || (source->nick.find('.') != std::string::npos)))
		{
			// Only change the mode if it's not redundant
			if ((adding && !channel->IsModeSet('r')) || (!adding && channel->IsModeSet('r')))
			{
				channel->SetMode('r',adding);
				return MODEACTION_ALLOW;
			}

			return MODEACTION_DENY;
		}
		else
		{
			source->WriteNumeric(500, "%s :Only a server may modify the +r channel mode", source->nick.c_str());
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

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		if ((kludgeme) || (ServerInstance->ULine(source->nick.c_str())) || (ServerInstance->ULine(source->server)) || (!*source->server || (source->nick.find('.') != std::string::npos)))
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
			source->WriteNumeric(500, "%s :Only a server may modify the +r user mode", source->nick.c_str());
			return MODEACTION_DENY;
		}
	}
};

/** Channel mode +R - registered users only
 */
class Channel_R : public SimpleChannelModeHandler
{
 public:
	Channel_R(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'R') { }
};

/** User mode +R - only allow PRIVMSG and NOTICE from registered users
 */
class User_R : public SimpleUserModeHandler
{
 public:
	User_R(InspIRCd* Instance) : SimpleUserModeHandler(Instance, 'R') { }
};

/** Channel mode +M - only allow privmsg and notice to channel from registered users
 */
class Channel_M : public SimpleChannelModeHandler
{
 public:
	Channel_M(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'M') { }
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

		if (!ServerInstance->Modes->AddMode(m1) || !ServerInstance->Modes->AddMode(m2) || !ServerInstance->Modes->AddMode(m3)
			|| !ServerInstance->Modes->AddMode(m4) || !ServerInstance->Modes->AddMode(m5))
		{
			throw ModuleException("You cannot load m_services.so and m_services_account.so at the same time (or some other module has claimed our modes)!");
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
			ServerInstance->SendWhoisLine(source, dest, 307, "%s %s :is a registered nick", source->nick.c_str(), dest->nick.c_str());
		}
	}


	virtual void OnUserPostNick(User* user, const std::string &oldnick)
	{
		/* On nickchange, if they have +r, remove it */
		if (user->IsModeSet('r') && assign(user->nick) != oldnick)
		{
			std::vector<std::string> modechange;
			modechange.push_back(user->nick);
			modechange.push_back("-r");
			kludgeme = true;
			ServerInstance->SendMode(modechange,user);
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
				if ((ServerInstance->ULine(user->nick.c_str())) || (ServerInstance->ULine(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user messaging a +M channel and is not registered
				user->WriteNumeric(477, "%s %s :You need a registered nickname to speak on this channel", user->nick.c_str(), c->name.c_str());
				return 1;
			}
		}
		if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;
			if ((u->IsModeSet('R')) && (!user->IsModeSet('r')))
			{
				if ((ServerInstance->ULine(user->nick.c_str())) || (ServerInstance->ULine(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}
				// user messaging a +R user and is not registered
				user->WriteNumeric(477, "%s %s :You need a registered nickname to message this user", user->nick.c_str(), u->nick.c_str());
				return 1;
			}
		}
		return 0;
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status, exempt_list);
	}

	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
		{
			if (chan->IsModeSet('R'))
			{
				if (!user->IsModeSet('r'))
				{
					if ((ServerInstance->ULine(user->nick.c_str())) || (ServerInstance->ULine(user->server)))
					{
						// user is ulined, won't be stopped from joining
						return 0;
					}
					// joining a +R channel and not identified
					user->WriteNumeric(477, "%s %s :You need a registered nickname to join this channel", user->nick.c_str(), chan->name.c_str());
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
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}
};


MODULE_INIT(ModuleServices)
