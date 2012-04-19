/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "account.h"
#include "u_listmode.h"

/* $ModDesc: Povides support for ircu-style services accounts, including chmode +R, etc. */

/** Channel mode +r - mark a channel as identified
 */
class Channel_r : public ModeHandler
{

 public:
	Channel_r(InspIRCd* Instance) : ModeHandler(Instance, 'r', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
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
	User_r(InspIRCd* Instance) : ModeHandler(Instance, 'r', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode)
	{
		if (servermode || IS_REMOTE(source) || ServerInstance->ULine(source->nick.c_str()) || ServerInstance->ULine(source->server))
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
	AChannel_R(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'R') { }
};

/** User mode +R - unidentified users cannot message
 */
class AUser_R : public SimpleUserModeHandler
{
 public:
	AUser_R(InspIRCd* Instance) : SimpleUserModeHandler(Instance, 'R') { }
};

/** Channel mode +M - unidentified users cannot message channel
 */
class AChannel_M : public SimpleChannelModeHandler
{
 public:
	AChannel_M(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'M') { }
};

class ModuleServicesAccount : public Module
{
	AChannel_R* m1;
	AChannel_M* m2;
	AUser_R* m3;
	Channel_r *m4;
	User_r *m5;
 public:
	ModuleServicesAccount(InspIRCd* Me) : Module(Me)
	{
		m1 = new AChannel_R(ServerInstance);
		m2 = new AChannel_M(ServerInstance);
		m3 = new AUser_R(ServerInstance);
		m4 = new Channel_r(ServerInstance);
		m5 = new User_r(ServerInstance);

		if (!ServerInstance->Modes->AddMode(m1) || !ServerInstance->Modes->AddMode(m2) || !ServerInstance->Modes->AddMode(m3) || !ServerInstance->Modes->AddMode(m4) || !ServerInstance->Modes->AddMode(m5))
			throw ModuleException("Some other module has claimed our modes!");

		Implementation eventlist[] = { I_OnWhois, I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserPreJoin, I_OnCheckBan,
			I_OnSyncUserMetaData, I_OnUserQuit, I_OnCleanup, I_OnDecodeMetaData, I_On005Numeric, I_OnCheckInvite, I_OnUserPostNick };

		ServerInstance->Modules->Attach(eventlist, this, 11);
	}

	virtual void On005Numeric(std::string &t)
	{
		ServerInstance->AddExtBanChar('R');
		ServerInstance->AddExtBanChar('M');
	}

	/* <- :twisted.oscnet.org 330 w00t2 w00t2 w00t :is logged in as */
	virtual void OnWhois(User* source, User* dest)
	{
		std::string *account;
		dest->GetExt("accountname", account);

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

	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		std::string *account;
		bool is_registered = user->GetExt("accountname", account);
		is_registered = is_registered && !account->empty();

		if ((ServerInstance->ULine(user->nick.c_str())) || (ServerInstance->ULine(user->server)))
		{
			// user is ulined, can speak regardless
			return 0;
		}

		if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)dest;

			if (c->IsModeSet('M') && !is_registered)
			{
				// user messaging a +M channel and is not registered
				user->WriteNumeric(477, ""+std::string(user->nick)+" "+std::string(c->name)+" :You need to be identified to a registered account to message this channel");
				return 1;
			}

			if (account)
			{
				if (c->GetExtBanStatus(*account, 'M') < 0)
				{
					// may not speak (text is deliberately vague, so they don't know which restriction to evade)
					user->WriteNumeric(477, ""+std::string(user->nick)+" "+std::string(c->name)+" :You may not speak in this channel");
					return 1;
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
				return 1;
			}
		}
		return 0;
	}

	virtual int OnCheckBan(User* user, Channel* chan)
	{
		std::string* account;
		if (!user->GetExt("accountname", account))
			return 0;
		return chan->GetExtBanStatus(*account, 'R');
	}

	virtual int OnCheckInvite(User *user, Channel *c)
	{
		std::string* account;
		if (!IS_LOCAL(user) || !user->GetExt("accountname", account))
			return 0;

		Module* ExceptionModule = ServerInstance->Modules->Find("m_inviteexception.so");
		if (ExceptionModule)
		{
			if (ListModeRequest(this, ExceptionModule, *account, 'R', c).Send())
			{
				// account name is exempt
				return 1;
			}
		}

		return 0;
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}

	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (!IS_LOCAL(user))
			return 0;

		std::string *account;
		bool is_registered = user->GetExt("accountname", account);
		is_registered = is_registered && !account->empty();

		if (chan)
		{
			if ((ServerInstance->ULine(user->nick.c_str())) || (ServerInstance->ULine(user->server)))
			{
				// user is ulined, won't be stopped from joining
				return 0;
			}

			if (chan->IsModeSet('R'))
			{
				if (!is_registered)
				{
					// joining a +R channel and not identified
					user->WriteNumeric(477, user->nick + " " + chan->name + " :You need to be identified to a registered account to join this channel");
					return 1;
				}
			}
		}
		return 0;
	}

	// Whenever the linking module wants to send out data, but doesnt know what the data
	// represents (e.g. it is metadata, added to a User or Channel by a module) then
	// this method is called. We should use the ProtoSendMetaData function after we've
	// corrected decided how the data should look, to send the metadata on its way if
	// it is ours.
	virtual void OnSyncUserMetaData(User* user, Module* proto, void* opaque, const std::string &extname, bool displayable)
	{
		// check if the linking module wants to know about OUR metadata
		if (extname == "accountname")
		{
			// check if this user has an swhois field to send
			std::string* account;
			user->GetExt("accountname", account);
			if (account)
			{
				// remove any accidental leading/trailing spaces
				trim(*account);

				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque,TYPE_USER,user,extname,*account);
			}
		}
	}

	// when a user quits, tidy up their metadata
	virtual void OnUserQuit(User* user, const std::string &message, const std::string &oper_message)
	{
		std::string* account;
		user->GetExt("accountname", account);
		if (account)
		{
			user->Shrink("accountname");
			delete account;
		}
	}

	// if the module is unloaded, tidy up all our dangling metadata
	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			User* user = (User*)item;
			std::string* account;
			user->GetExt("accountname", account);
			if (account)
			{
				user->Shrink("accountname");
				delete account;
			}
		}
	}

	// Whenever the linking module receives metadata from another server and doesnt know what
	// to do with it (of course, hence the 'meta') it calls this method, and it is up to each
	// module in turn to figure out if this metadata key belongs to them, and what they want
	// to do with it.
	// In our case we're only sending a single string around, so we just construct a std::string.
	// Some modules will probably get much more complex and format more detailed structs and classes
	// in a textual way for sending over the link.
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_USER) && (extname == "accountname"))
		{
			User* dest = (User*)target;

			std::string* account;
			if (dest->GetExt("accountname", account)) {
				// remove old account so that we can set new (or leave unset)
				dest->Shrink("accountname");
				delete account;
			}

			if (!extdata.empty())
			{
				account = new std::string(extdata);
				// remove any accidental leading/trailing spaces
				trim(*account);
				dest->Extend("accountname", account);

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

MODULE_INIT(ModuleServicesAccount)
