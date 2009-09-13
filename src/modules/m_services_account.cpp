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
#include "account.h"

/* $ModDesc: Povides support for ircu-style services accounts, including chmode +R, etc. */

/** Channel mode +r - mark a channel as identified
 */
class Channel_r : public ModeHandler
{

 public:
	Channel_r(InspIRCd* Instance, Module* Creator) : ModeHandler(Creator, 'r', PARAM_NONE, MODETYPE_CHANNEL) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		// only a u-lined server may add or remove the +r mode.
		if (IS_REMOTE(source) || ServerInstance->ULine(source->nick.c_str()) || ServerInstance->ULine(source->server))
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
	User_r(InspIRCd* Instance, Module* Creator) : ModeHandler(Creator, 'r', PARAM_NONE, MODETYPE_USER) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (IS_REMOTE(source) || ServerInstance->ULine(source->nick.c_str()) || ServerInstance->ULine(source->server))
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

/** Channel mode +R - unidentified users cannot join
 */
class AChannel_R : public SimpleChannelModeHandler
{
 public:
	AChannel_R(InspIRCd* Instance, Module* Creator) : SimpleChannelModeHandler(Instance, Creator, 'R') { }
};

/** User mode +R - unidentified users cannot message
 */
class AUser_R : public SimpleUserModeHandler
{
 public:
	AUser_R(InspIRCd* Instance, Module* Creator) : SimpleUserModeHandler(Creator, 'R') { }
};

/** Channel mode +M - unidentified users cannot message channel
 */
class AChannel_M : public SimpleChannelModeHandler
{
 public:
	AChannel_M(InspIRCd* Instance, Module* Creator) : SimpleChannelModeHandler(Instance, Creator, 'M') { }
};

class ModuleServicesAccount : public Module
{
	AChannel_R m1;
	AChannel_M m2;
	AUser_R m3;
	Channel_r m4;
	User_r m5;
	StringExtItem accountname;
 public:
	ModuleServicesAccount(InspIRCd* Me) : Module(Me),
		m1(Me, this), m2(Me, this), m3(Me, this), m4(Me, this), m5(Me, this),
		accountname("accountname", this)
	{

		if (!ServerInstance->Modes->AddMode(&m1) || !ServerInstance->Modes->AddMode(&m2) ||
			!ServerInstance->Modes->AddMode(&m3) || !ServerInstance->Modes->AddMode(&m4) ||
			!ServerInstance->Modes->AddMode(&m5))
			throw ModuleException("Some other module has claimed our modes!");

		Implementation eventlist[] = { I_OnWhois, I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserPreJoin, I_OnCheckBan,
			I_OnSyncUser, I_OnUserQuit, I_OnCleanup, I_OnDecodeMetaData, I_On005Numeric, I_OnUserPostNick };

		ServerInstance->Modules->Attach(eventlist, this, 10);
	}

	virtual void On005Numeric(std::string &t)
	{
		ServerInstance->AddExtBanChar('R');
		ServerInstance->AddExtBanChar('M');
	}

	/* <- :twisted.oscnet.org 330 w00t2 w00t2 w00t :is logged in as */
	virtual void OnWhois(User* source, User* dest)
	{
		std::string *account = accountname.get(dest);

		if (account)
		{
			ServerInstance->SendWhoisLine(source, dest, 330, "%s %s %s :is logged in as", source->nick.c_str(), dest->nick.c_str(), account->c_str());
		}

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
			ServerInstance->SendMode(modechange, user);
		}
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		std::string *account = accountname.get(user);
		bool is_registered = account && !account->empty();

		if ((ServerInstance->ULine(user->nick.c_str())) || (ServerInstance->ULine(user->server)))
		{
			// user is ulined, can speak regardless
			return MOD_RES_PASSTHRU;
		}

		if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)dest;

			if (c->IsModeSet('M') && !is_registered)
			{
				// user messaging a +M channel and is not registered
				user->WriteNumeric(477, ""+std::string(user->nick)+" "+std::string(c->name)+" :You need to be identified to a registered account to message this channel");
				return MOD_RES_DENY;
			}

			if (account)
			{
				if (c->GetExtBanStatus(*account, 'M') == MOD_RES_DENY)
				{
					// may not speak (text is deliberately vague, so they don't know which restriction to evade)
					user->WriteNumeric(477, ""+std::string(user->nick)+" "+std::string(c->name)+" :You may not speak in this channel");
					return MOD_RES_DENY;
				}
			}
		}
		else if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;

			if (u->IsModeSet('R') && !is_registered)
			{
				// user messaging a +R user and is not registered
				user->WriteNumeric(477, ""+ user->nick +" "+ u->nick +" :You need to be identified to a registered account to message this user");
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnCheckBan(User* user, Channel* chan)
	{
		std::string *account = accountname.get(user);
		if (!account)
			return MOD_RES_PASSTHRU;
		return chan->GetExtBanStatus(*account, 'R');
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}

	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		std::string *account = accountname.get(user);
		bool is_registered = account && !account->empty();

		if (chan)
		{
			if ((ServerInstance->ULine(user->nick.c_str())) || (ServerInstance->ULine(user->server)))
			{
				// user is ulined, won't be stopped from joining
				return MOD_RES_PASSTHRU;
			}

			if (chan->IsModeSet('R'))
			{
				if (!is_registered)
				{
					// joining a +R channel and not identified
					user->WriteNumeric(477, user->nick + " " + chan->name + " :You need to be identified to a registered account to join this channel");
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	// Whenever the linking module receives metadata from another server and doesnt know what
	// to do with it (of course, hence the 'meta') it calls this method, and it is up to each
	// module in turn to figure out if this metadata key belongs to them, and what they want
	// to do with it.
	// In our case we're only sending a single string around, so we just construct a std::string.
	// Some modules will probably get much more complex and format more detailed structs and classes
	// in a textual way for sending over the link.
	virtual void OnDecodeMetaData(Extensible* target, const std::string &extname, const std::string &extdata)
	{
		User* dest = dynamic_cast<User*>(target);
		// check if its our metadata key, and its associated with a user
		if (dest && (extname == "accountname"))
		{
			if (!extdata.empty())
			{
				std::string *account = accountname.get(dest);
				trim(*account);

				if (IS_LOCAL(dest))
					dest->WriteNumeric(900, "%s %s %s :You are now logged in as %s",
						dest->nick.c_str(), dest->GetFullHost().c_str(), account->c_str(), account->c_str());

				AccountData ac;
				ac.user = dest;
				ac.account = *account;
				Event n((char*)&ac, this, "account_login");
				n.Send(ServerInstance);
			}
		}
	}

	virtual ~ModuleServicesAccount()
	{
		ServerInstance->Modes->DelMode(&m1);
		ServerInstance->Modes->DelMode(&m2);
		ServerInstance->Modes->DelMode(&m3);
		ServerInstance->Modes->DelMode(&m4);
		ServerInstance->Modes->DelMode(&m5);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleServicesAccount)
