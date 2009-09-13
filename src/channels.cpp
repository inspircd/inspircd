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

/* $Core */

#include "inspircd.h"
#include <cstdarg>
#include "mode.h"

Channel::Channel(InspIRCd* Instance, const std::string &cname, time_t ts) : ServerInstance(Instance)
{
	chan_hash::iterator findchan = ServerInstance->chanlist->find(cname);
	if (findchan != Instance->chanlist->end())
		throw CoreException("Cannot create duplicate channel " + cname);

	(*(ServerInstance->chanlist))[cname.c_str()] = this;
	this->name.assign(cname, 0, ServerInstance->Config->Limits.ChanMax);
	this->age = ts ? ts : ServerInstance->Time();

	maxbans = topicset = 0;
	modes.reset();
}

void Channel::SetMode(char mode,bool mode_on)
{
	modes[mode-65] = mode_on;
}

void Channel::SetModeParam(char mode, std::string parameter)
{
	CustomModeList::iterator n = custom_mode_params.find(mode);
	// always erase, even if changing, so that the map gets the new value
	if (n != custom_mode_params.end())
		custom_mode_params.erase(n);
	if (parameter.empty())
	{
		modes[mode-65] = false;
	}
	else
	{
		custom_mode_params[mode] = parameter;
		modes[mode-65] = true;
	}
}

std::string Channel::GetModeParameter(char mode)
{
	CustomModeList::iterator n = custom_mode_params.find(mode);
	if (n != custom_mode_params.end())
		return n->second;
	return "";
}

int Channel::SetTopic(User *u, std::string &ntopic, bool forceset)
{
	if (u)
	{
		if(!forceset)
		{
			ModResult res;
			/* 0: check status, 1: don't, -1: disallow change silently */

			FIRST_MOD_RESULT(ServerInstance, OnPreTopicChange, res, (u,this,ntopic));

			if (res == MOD_RES_DENY)
				return CMD_FAILURE;
			if (res != MOD_RES_ALLOW)
			{
				if (!this->HasUser(u))
				{
					u->WriteNumeric(442, "%s %s :You're not on that channel!",u->nick.c_str(), this->name.c_str());
					return CMD_FAILURE;
				}
				if ((this->IsModeSet('t')) && (this->GetPrefixValue(u) < HALFOP_VALUE))
				{
					u->WriteNumeric(482, "%s %s :You must be at least a half-operator to change the topic on this channel", u->nick.c_str(), this->name.c_str());
					return CMD_FAILURE;
				}
			}
		}
	}

	this->topic.assign(ntopic, 0, ServerInstance->Config->Limits.MaxTopic);
	if (u)
	{
		this->setby.assign(ServerInstance->Config->FullHostInTopic ? u->GetFullHost() : u->nick, 0, 128);
		this->WriteChannel(u, "TOPIC %s :%s", this->name.c_str(), this->topic.c_str());
	}
	else
	{
		this->setby.assign(ServerInstance->Config->ServerName);
		this->WriteChannelWithServ(ServerInstance->Config->ServerName, "TOPIC %s :%s", this->name.c_str(), this->topic.c_str());
	}

	this->topicset = ServerInstance->Time();

	// XXX: this check for 'u' is probably pre-fake-user, and it fucking sucks anyway. we need to change this.
	if (u)
	{
		FOREACH_MOD(I_OnPostTopicChange,OnPostTopicChange(u, this, this->topic));
	}

	return CMD_SUCCESS;
}

long Channel::GetUserCounter()
{
	return userlist.size();
}

Membership* Channel::AddUser(User* user)
{
	Membership* memb = new Membership(user, this);
	userlist[user] = memb;
	return memb;
}

unsigned long Channel::DelUser(User* user)
{
	UserMembIter a = userlist.find(user);

	if (a != userlist.end())
	{
		delete a->second;
		userlist.erase(a);
	}

	return userlist.size();
}

bool Channel::HasUser(User* user)
{
	return (userlist.find(user) != userlist.end());
}

Membership* Channel::GetUser(User* user)
{
	UserMembIter i = userlist.find(user);
	if (i == userlist.end())
		return NULL;
	return i->second;
}

const UserMembList* Channel::GetUsers()
{
	return &userlist;
}

void Channel::SetDefaultModes()
{
	ServerInstance->Logs->Log("CHANNELS", DEBUG, "SetDefaultModes %s", ServerInstance->Config->DefaultModes);
	irc::spacesepstream list(ServerInstance->Config->DefaultModes);
	std::string modeseq;
	std::string parameter;

	list.GetToken(modeseq);

	for (std::string::iterator n = modeseq.begin(); n != modeseq.end(); ++n)
	{
		ModeHandler* mode = ServerInstance->Modes->FindMode(*n, MODETYPE_CHANNEL);
		if (mode)
		{
			if (mode->GetNumParams(true))
				list.GetToken(parameter);
			else
				parameter.clear();

			mode->OnModeChange(ServerInstance->FakeClient, ServerInstance->FakeClient, this, parameter, true);
		}
	}
}

/*
 * add a channel to a user, creating the record for it if needed and linking
 * it to the user record
 */
Channel* Channel::JoinUser(InspIRCd* Instance, User *user, const char* cn, bool override, const char* key, bool bursting, time_t TS)
{
	// Fix: unregistered users could be joined using /SAJOIN
	if (!user || !cn || user->registered != REG_ALL)
		return NULL;

	char cname[MAXBUF];
	std::string privs;
	Channel *Ptr;

	/*
	 * We don't restrict the number of channels that remote users or users that are override-joining may be in.
	 * We restrict local users to MaxChans channels.
	 * We restrict local operators to OperMaxChans channels.
	 * This is a lot more logical than how it was formerly. -- w00t
	 */
	if (IS_LOCAL(user) && !override)
	{
		// Checking MyClass exists because we *may* get here with NULL, not 100% sure.
		if (user->MyClass && user->MyClass->maxchans)
		{
			if (user->HasPrivPermission("channels/high-join-limit"))
			{
				if (user->chans.size() >= Instance->Config->OperMaxChans)
				{
					user->WriteNumeric(ERR_TOOMANYCHANNELS, "%s %s :You are on too many channels",user->nick.c_str(), cn);
					return NULL;
				}
			}
			else
			{
				if (user->chans.size() >= user->MyClass->maxchans)
				{
					user->WriteNumeric(ERR_TOOMANYCHANNELS, "%s %s :You are on too many channels",user->nick.c_str(), cn);
					return NULL;
				}
			}
		}
	}

	strlcpy(cname, cn, Instance->Config->Limits.ChanMax);
	Ptr = Instance->FindChan(cname);
	bool created_by_local = false;

	if (!Ptr)
	{
		/*
		 * Fix: desync bug was here, don't set @ on remote users - spanningtree handles their permissions. bug #358. -- w00t
		 */
		if (!IS_LOCAL(user))
		{
			if (!TS)
				Instance->Logs->Log("CHANNEL",DEBUG,"*** BUG *** Channel::JoinUser called for REMOTE user '%s' on channel '%s' but no TS given!", user->nick.c_str(), cn);
		}
		else
		{
			privs = "@";
			created_by_local = true;
		}

		if (IS_LOCAL(user) && override == false)
		{
			ModResult MOD_RESULT;
			FIRST_MOD_RESULT(Instance, OnUserPreJoin, MOD_RESULT, (user, NULL, cname, privs, key ? key : ""));
			if (MOD_RESULT == MOD_RES_DENY)
				return NULL;
		}

		Ptr = new Channel(Instance, cname, TS);
	}
	else
	{
		/* Already on the channel */
		if (Ptr->HasUser(user))
			return NULL;

		/*
		 * remote users are allowed us to bypass channel modes
		 * and bans (used by servers)
		 */
		if (IS_LOCAL(user) && override == false)
		{
			ModResult MOD_RESULT;
			FIRST_MOD_RESULT(Instance, OnUserPreJoin, MOD_RESULT, (user, Ptr, cname, privs, key ? key : ""));
			if (MOD_RESULT == MOD_RES_DENY)
			{
				return NULL;
			}
			else if (MOD_RESULT == MOD_RES_PASSTHRU)
			{
				std::string ckey = Ptr->GetModeParameter('k');
				bool invited = user->IsInvited(Ptr->name.c_str());
				bool can_bypass = Instance->Config->InvBypassModes && invited;

				if (!ckey.empty())
				{
					FIRST_MOD_RESULT(Instance, OnCheckKey, MOD_RESULT, (user, Ptr, key ? key : ""));
					if (!MOD_RESULT.check((key && ckey == key) || can_bypass))
					{
						// If no key provided, or key is not the right one, and can't bypass +k (not invited or option not enabled)
						user->WriteNumeric(ERR_BADCHANNELKEY, "%s %s :Cannot join channel (Incorrect channel key)",user->nick.c_str(), Ptr->name.c_str());
						return NULL;
					}
				}

				if (Ptr->IsModeSet('i'))
				{
					FIRST_MOD_RESULT(Instance, OnCheckInvite, MOD_RESULT, (user, Ptr));
					if (!MOD_RESULT.check(invited))
					{
						user->WriteNumeric(ERR_INVITEONLYCHAN, "%s %s :Cannot join channel (Invite only)",user->nick.c_str(), Ptr->name.c_str());
						return NULL;
					}
				}

				std::string limit = Ptr->GetModeParameter('l');
				if (!limit.empty())
				{
					FIRST_MOD_RESULT(Instance, OnCheckLimit, MOD_RESULT, (user, Ptr));
					if (!MOD_RESULT.check((Ptr->GetUserCounter() < atol(limit.c_str()) || can_bypass)))
					{
						user->WriteNumeric(ERR_CHANNELISFULL, "%s %s :Cannot join channel (Channel is full)",user->nick.c_str(), Ptr->name.c_str());
						return NULL;
					}
				}

				if (Ptr->IsBanned(user) && !can_bypass)
				{
					user->WriteNumeric(ERR_BANNEDFROMCHAN, "%s %s :Cannot join channel (You're banned)",user->nick.c_str(), Ptr->name.c_str());
					return NULL;
				}

				/*
				 * If the user has invites for this channel, remove them now
				 * after a successful join so they don't build up.
				 */
				if (invited)
				{
					user->RemoveInvite(Ptr->name.c_str());
				}
			}
		}
	}

	if (created_by_local)
	{
		/* As spotted by jilles, dont bother to set this on remote users */
		Ptr->SetDefaultModes();
	}

	return Channel::ForceChan(Instance, Ptr, user, privs, bursting, created_by_local);
}

Channel* Channel::ForceChan(InspIRCd* Instance, Channel* Ptr, User* user, const std::string &privs, bool bursting, bool created)
{
	std::string nick = user->nick;
	bool silent = false;

	Ptr->AddUser(user);
	user->chans.insert(Ptr);

	for (std::string::const_iterator x = privs.begin(); x != privs.end(); x++)
	{
		const char status = *x;
		ModeHandler* mh = Instance->Modes->FindPrefix(status);
		if (mh)
		{
			/* Set, and make sure that the mode handler knows this mode was now set */
			Ptr->SetPrefix(user, mh->GetModeChar(), mh->GetPrefixRank(), true);
			mh->OnModeChange(Instance->FakeClient, Instance->FakeClient, Ptr, nick, true);
		}
	}

	FOREACH_MOD_I(Instance,I_OnUserJoin,OnUserJoin(user, Ptr, bursting, silent, created));

	if (!silent)
		Ptr->WriteChannel(user,"JOIN :%s",Ptr->name.c_str());

	/* Theyre not the first ones in here, make sure everyone else sees the modes we gave the user */
	std::string ms = Instance->Modes->ModeString(user, Ptr);
	if ((Ptr->GetUserCounter() > 1) && (ms.length()))
		Ptr->WriteAllExceptSender(user, true, 0, "MODE %s +%s", Ptr->name.c_str(), ms.c_str());

	/* Major improvement by Brain - we dont need to be calculating all this pointlessly for remote users */
	if (IS_LOCAL(user))
	{
		if (Ptr->topicset)
		{
			user->WriteNumeric(RPL_TOPIC, "%s %s :%s", user->nick.c_str(), Ptr->name.c_str(), Ptr->topic.c_str());
			user->WriteNumeric(RPL_TOPICTIME, "%s %s %s %lu", user->nick.c_str(), Ptr->name.c_str(), Ptr->setby.c_str(), (unsigned long)Ptr->topicset);
		}
		Ptr->UserList(user);
	}
	FOREACH_MOD_I(Instance,I_OnPostJoin,OnPostJoin(user, Ptr));
	return Ptr;
}

bool Channel::IsBanned(User* user)
{
	ModResult result;
	DO_EACH_HOOK(ServerInstance, OnCheckBan, ModResult modresult, (user, this))
	{
		result = result + modresult;
	}
	WHILE_EACH_HOOK(ServerInstance, OnCheckBan);

	if (result != MOD_RES_PASSTHRU)
		return (result == MOD_RES_DENY);

	char mask[MAXBUF];
	snprintf(mask, MAXBUF, "%s!%s@%s", user->nick.c_str(), user->ident.c_str(), user->GetIPString());
	for (BanList::iterator i = this->bans.begin(); i != this->bans.end(); i++)
	{
		if ((InspIRCd::Match(user->GetFullHost(),i->data, NULL)) || // host
			(InspIRCd::Match(user->GetFullRealHost(),i->data, NULL)) || // uncloaked host
			(InspIRCd::MatchCIDR(mask, i->data, NULL))) // ip
		{
			return true;
		}
	}
	return false;
}

ModResult Channel::GetExtBanStatus(const std::string &str, char type)
{
	ModResult result;
	DO_EACH_HOOK(ServerInstance, OnCheckStringExtBan, ModResult modresult, (str, this, type))
	{
		result = result + modresult;
	}
	WHILE_EACH_HOOK(ServerInstance, OnCheckStringExtBan);

	if (result != MOD_RES_PASSTHRU)
		return result;

	// nobody decided for us, check the ban list
	for (BanList::iterator i = this->bans.begin(); i != this->bans.end(); i++)
	{
		if (i->data[0] != type || i->data[1] != ':')
			continue;

		std::string maskptr = i->data.substr(2);
		ServerInstance->Logs->Log("EXTBANS", DEBUG, "Checking %s against %s, type is %c", str.c_str(), maskptr.c_str(), type);

		if (InspIRCd::Match(str, maskptr, NULL))
			return MOD_RES_DENY;
	}

	return MOD_RES_PASSTHRU;
}

ModResult Channel::GetExtBanStatus(User *user, char type)
{
	ModResult rv;
	DO_EACH_HOOK(ServerInstance, OnCheckExtBan, ModResult modresult, (user, this, type))
	{
		rv = rv + modresult;
	}
	WHILE_EACH_HOOK(ServerInstance, OnCheckExtBan);

	if (rv != MOD_RES_PASSTHRU)
		return rv;

	char mask[MAXBUF];
	snprintf(mask, MAXBUF, "%s!%s@%s", user->nick.c_str(), user->ident.c_str(), user->GetIPString());

	rv = rv + this->GetExtBanStatus(mask, type);
	rv = rv + this->GetExtBanStatus(user->GetFullHost(), type);
	rv = rv + this->GetExtBanStatus(user->GetFullRealHost(), type);
	return rv;
}

/* Channel::PartUser
 * remove a channel from a users record, and return the number of users left.
 * Therefore, if this function returns 0 the caller should delete the Channel.
 */
long Channel::PartUser(User *user, std::string &reason)
{
	bool silent = false;

	if (!user)
		return this->GetUserCounter();

	UCListIter i = user->chans.find(this);
	if (i != user->chans.end())
	{
		FOREACH_MOD(I_OnUserPart,OnUserPart(user, this, reason, silent));

		if (!silent)
			this->WriteChannel(user, "PART %s%s%s", this->name.c_str(), reason.empty() ? "" : " :", reason.c_str());

		user->chans.erase(i);
		this->RemoveAllPrefixes(user);
	}

	if (!this->DelUser(user)) /* if there are no users left on the channel... */
	{
		chan_hash::iterator iter = ServerInstance->chanlist->find(this->name);
		/* kill the record */
		if (iter != ServerInstance->chanlist->end())
		{
			ModResult res;
			FIRST_MOD_RESULT(ServerInstance, OnChannelPreDelete, res, (this));
			if (res == MOD_RES_DENY)
				return 1; // delete halted by module
			FOREACH_MOD(I_OnChannelDelete, OnChannelDelete(this));
			ServerInstance->chanlist->erase(iter);
		}
		return 0;
	}

	return this->GetUserCounter();
}

long Channel::ServerKickUser(User* user, const char* reason, const char* servername)
{
	if (servername == NULL || *ServerInstance->Config->HideWhoisServer)
		servername = ServerInstance->Config->ServerName;

	ServerInstance->FakeClient->server = servername;
	return this->KickUser(ServerInstance->FakeClient, user, reason);
}

long Channel::KickUser(User *src, User *user, const char* reason)
{
	bool silent = false;

	if (!src || !user || !reason)
		return this->GetUserCounter();

	if (IS_LOCAL(src))
	{
		if (!this->HasUser(user))
		{
			src->WriteNumeric(ERR_USERNOTINCHANNEL, "%s %s %s :They are not on that channel",src->nick.c_str(), user->nick.c_str(), this->name.c_str());
			return this->GetUserCounter();
		}
		if ((ServerInstance->ULine(user->server)) && (!ServerInstance->ULine(src->server)))
		{
			src->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Only a u-line may kick a u-line from a channel.",src->nick.c_str(), this->name.c_str());
			return this->GetUserCounter();
		}

		ModResult res;
		if (ServerInstance->ULine(src->server))
			res = MOD_RES_ALLOW;
		if (res == MOD_RES_PASSTHRU)
			FIRST_MOD_RESULT(ServerInstance, OnUserPreKick, res, (src,user,this,reason));
		if (res == MOD_RES_PASSTHRU)
			FIRST_MOD_RESULT(ServerInstance, OnAccessCheck, res, (src,user,this,AC_KICK));

		if (res == MOD_RES_DENY)
			return this->GetUserCounter();

		if (res == MOD_RES_PASSTHRU)
		{
			int them = this->GetPrefixValue(src);
			int us = this->GetPrefixValue(user);
			if ((them < HALFOP_VALUE) || (them < us))
			{
				src->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must be a channel %soperator",src->nick.c_str(), this->name.c_str(), them >= HALFOP_VALUE ? "" : "half-");
				return this->GetUserCounter();
			}
		}
	}

	FOREACH_MOD(I_OnUserKick,OnUserKick(src, user, this, reason, silent));

	UCListIter i = user->chans.find(this);
	if (i != user->chans.end())
	{
		/* zap it from the channel list of the user */
		if (!silent)
			this->WriteChannel(src, "KICK %s %s :%s", this->name.c_str(), user->nick.c_str(), reason);

		user->chans.erase(i);
		this->RemoveAllPrefixes(user);
	}

	if (!this->DelUser(user))
	/* if there are no users left on the channel */
	{
		chan_hash::iterator iter = ServerInstance->chanlist->find(this->name.c_str());

		/* kill the record */
		if (iter != ServerInstance->chanlist->end())
		{
			ModResult res;
			FIRST_MOD_RESULT(ServerInstance, OnChannelPreDelete, res, (this));
			if (res == MOD_RES_DENY)
				return 1; // delete halted by module
			FOREACH_MOD(I_OnChannelDelete, OnChannelDelete(this));
			ServerInstance->chanlist->erase(iter);
		}
		return 0;
	}

	return this->GetUserCounter();
}

void Channel::WriteChannel(User* user, const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if (!user || !text)
		return;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteChannel(user, std::string(textbuffer));
}

void Channel::WriteChannel(User* user, const std::string &text)
{
	char tb[MAXBUF];

	if (!user)
		return;

	snprintf(tb,MAXBUF,":%s %s", user->GetFullHost().c_str(), text.c_str());
	std::string out = tb;

	for (UserMembIter i = userlist.begin(); i != userlist.end(); i++)
	{
		if (IS_LOCAL(i->first))
			i->first->Write(out);
	}
}

void Channel::WriteChannelWithServ(const char* ServName, const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if (!text)
		return;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteChannelWithServ(ServName, std::string(textbuffer));
}

void Channel::WriteChannelWithServ(const char* ServName, const std::string &text)
{
	char tb[MAXBUF];

	snprintf(tb,MAXBUF,":%s %s", ServName ? ServName : ServerInstance->Config->ServerName, text.c_str());
	std::string out = tb;

	for (UserMembIter i = userlist.begin(); i != userlist.end(); i++)
	{
		if (IS_LOCAL(i->first))
			i->first->Write(out);
	}
}

/* write formatted text from a source user to all users on a channel except
 * for the sender (for privmsg etc) */
void Channel::WriteAllExceptSender(User* user, bool serversource, char status, const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if (!text)
		return;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteAllExceptSender(user, serversource, status, std::string(textbuffer));
}

void Channel::WriteAllExcept(User* user, bool serversource, char status, CUList &except_list, const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if (!text)
		return;

	int offset = snprintf(textbuffer,MAXBUF,":%s ", user->GetFullHost().c_str());

	va_start(argsPtr, text);
	vsnprintf(textbuffer + offset, MAXBUF - offset, text, argsPtr);
	va_end(argsPtr);

	this->RawWriteAllExcept(user, serversource, status, except_list, std::string(textbuffer));
}

void Channel::WriteAllExcept(User* user, bool serversource, char status, CUList &except_list, const std::string &text)
{
	char tb[MAXBUF];

	snprintf(tb,MAXBUF,":%s %s", serversource ? ServerInstance->Config->ServerName : user->GetFullHost().c_str(), text.c_str());
	std::string out = tb;

	this->RawWriteAllExcept(user, serversource, status, except_list, std::string(tb));
}

void Channel::RawWriteAllExcept(User* user, bool serversource, char status, CUList &except_list, const std::string &out)
{
	for (UserMembIter i = userlist.begin(); i != userlist.end(); i++)
	{
		if ((IS_LOCAL(i->first)) && (except_list.find(i->first) == except_list.end()))
		{
			/* User doesnt have the status we're after */
			if (status && !strchr(this->GetAllPrefixChars(i->first), status))
				continue;

			i->first->Write(out);
		}
	}
}

void Channel::WriteAllExceptSender(User* user, bool serversource, char status, const std::string& text)
{
	CUList except_list;
	except_list.insert(user);
	this->WriteAllExcept(user, serversource, status, except_list, std::string(text));
}

/*
 * return a count of the users on a specific channel accounting for
 * invisible users who won't increase the count. e.g. for /LIST
 */
int Channel::CountInvisible()
{
	int count = 0;
	for (UserMembIter i = userlist.begin(); i != userlist.end(); i++)
	{
		if (!(i->first->IsModeSet('i')))
			count++;
	}

	return count;
}

char* Channel::ChanModes(bool showkey)
{
	static char scratch[MAXBUF];
	static char sparam[MAXBUF];
	char* offset = scratch;
	std::string extparam;

	*scratch = '\0';
	*sparam = '\0';

	/* This was still iterating up to 190, Channel::modes is only 64 elements -- Om */
	for(int n = 0; n < 64; n++)
	{
		if(this->modes[n])
		{
			*offset++ = n + 65;
			extparam.clear();
			switch (n)
			{
				case CM_KEY:
					// Unfortunately this must be special-cased, as we definitely don't want to always display key.
					if (showkey)
					{
						extparam = this->GetModeParameter('k');
					}
					else
					{
						extparam = "<key>";
					}
					break;
				case CM_NOEXTERNAL:
				case CM_TOPICLOCK:
				case CM_INVITEONLY:
				case CM_MODERATED:
				case CM_SECRET:
				case CM_PRIVATE:
					/* We know these have no parameters */
				break;
				default:
					extparam = this->GetModeParameter(n + 65);
				break;
			}
			if (!extparam.empty())
			{
				charlcat(sparam,' ',MAXBUF);
				strlcat(sparam,extparam.c_str(),MAXBUF);
			}
		}
	}

	/* Null terminate scratch */
	*offset = '\0';
	strlcat(scratch,sparam,MAXBUF);
	return scratch;
}

/* compile a userlist of a channel into a string, each nick seperated by
 * spaces and op, voice etc status shown as @ and +, and send it to 'user'
 */
void Channel::UserList(User *user)
{
	char list[MAXBUF];
	size_t dlen, curlen;
	ModResult call_modules;

	if (!IS_LOCAL(user))
		return;

	FIRST_MOD_RESULT(ServerInstance, OnUserList, call_modules, (user, this));

	if (call_modules != MOD_RES_ALLOW)
	{
		if ((this->IsModeSet('s')) && (!this->HasUser(user)))
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(), this->name.c_str());
			return;
		}
	}

	dlen = curlen = snprintf(list,MAXBUF,"%s %c %s :", user->nick.c_str(), this->IsModeSet('s') ? '@' : this->IsModeSet('p') ? '*' : '=',  this->name.c_str());

	int numusers = 0;
	char* ptr = list + dlen;

	/* Improvement by Brain - this doesnt change in value, so why was it inside
	 * the loop?
	 */
	bool has_user = this->HasUser(user);

	for (UserMembIter i = userlist.begin(); i != userlist.end(); i++)
	{
		if ((!has_user) && (i->first->IsModeSet('i')))
		{
			/*
			 * user is +i, and source not on the channel, does not show
			 * nick in NAMES list
			 */
			continue;
		}

		std::string prefixlist = this->GetPrefixChar(i->first);
		std::string nick = i->first->nick;

		if (call_modules != MOD_RES_DENY)
		{
			FOREACH_MOD(I_OnNamesListItem, OnNamesListItem(user, i->first, this, prefixlist, nick));

			/* Nick was nuked, a module wants us to skip it */
			if (nick.empty())
				continue;
		}

		size_t ptrlen = 0;

		if (curlen + prefixlist.length() + nick.length() + 1 > 480)
		{
			/* list overflowed into multiple numerics */
			user->WriteNumeric(RPL_NAMREPLY, std::string(list));

			/* reset our lengths */
			dlen = curlen = snprintf(list,MAXBUF,"%s %c %s :", user->nick.c_str(), this->IsModeSet('s') ? '@' : this->IsModeSet('p') ? '*' : '=', this->name.c_str());
			ptr = list + dlen;

			ptrlen = 0;
			numusers = 0;
		}

		ptrlen = snprintf(ptr, MAXBUF, "%s%s ", prefixlist.c_str(), nick.c_str());

		curlen += ptrlen;
		ptr += ptrlen;

		numusers++;
	}

	/* if whats left in the list isnt empty, send it */
	if (numusers)
	{
		user->WriteNumeric(RPL_NAMREPLY, std::string(list));
	}

	user->WriteNumeric(RPL_ENDOFNAMES, "%s %s :End of /NAMES list.", user->nick.c_str(), this->name.c_str());
}

long Channel::GetMaxBans()
{
	/* Return the cached value if there is one */
	if (this->maxbans)
		return this->maxbans;

	/* If there isnt one, we have to do some O(n) hax to find it the first time. (ick) */
	for (std::map<std::string,int>::iterator n = ServerInstance->Config->maxbans.begin(); n != ServerInstance->Config->maxbans.end(); n++)
	{
		if (InspIRCd::Match(this->name, n->first, NULL))
		{
			this->maxbans = n->second;
			return n->second;
		}
	}

	/* Screw it, just return the default of 64 */
	this->maxbans = 64;
	return this->maxbans;
}

void Channel::ResetMaxBans()
{
	this->maxbans = 0;
}

/* returns the status character for a given user on a channel, e.g. @ for op,
 * % for halfop etc. If the user has several modes set, the highest mode
 * the user has must be returned.
 */
const char* Channel::GetPrefixChar(User *user)
{
	static char pf[2] = {0, 0};
	*pf = 0;
	unsigned int bestrank = 0;

	UserMembIter m = userlist.find(user);
	if (m != userlist.end())
	{
		for(unsigned int i=0; i < m->second->modes.length(); i++)
		{
			char mchar = m->second->modes[i];
			ModeHandler* mh = ServerInstance->Modes->FindMode(mchar, MODETYPE_CHANNEL);
			if (mh && mh->GetPrefixRank() > bestrank)
			{
				bestrank = mh->GetPrefixRank();
				pf[0] = mh->GetPrefix();
			}
		}
	}
	return pf;
}


const char* Channel::GetAllPrefixChars(User* user)
{
	static char prefix[64];
	int ctr = 0;

	UserMembIter m = userlist.find(user);
	if (m != userlist.end())
	{
		for(unsigned int i=0; i < m->second->modes.length(); i++)
		{
			char mchar = m->second->modes[i];
			ModeHandler* mh = ServerInstance->Modes->FindMode(mchar, MODETYPE_CHANNEL);
			if (mh && mh->GetPrefix())
				prefix[ctr++] = mh->GetPrefix();
		}
	}
	prefix[ctr] = 0;

	return prefix;
}

unsigned int Channel::GetPrefixValue(User* user)
{
	unsigned int bestrank = 0;

	UserMembIter m = userlist.find(user);
	if (m != userlist.end())
	{
		for(unsigned int i=0; i < m->second->modes.length(); i++)
		{
			char mchar = m->second->modes[i];
			ModeHandler* mh = ServerInstance->Modes->FindMode(mchar, MODETYPE_CHANNEL);
			if (mh && mh->GetPrefixRank() > bestrank)
				bestrank = mh->GetPrefixRank();
		}
	}
	return bestrank;
}

void Channel::SetPrefix(User* user, char prefix, unsigned int prefix_value, bool adding)
{
	UserMembIter m = userlist.find(user);
	if (m != userlist.end())
	{
		for(unsigned int i=0; i < m->second->modes.length(); i++)
		{
			char mchar = m->second->modes[i];
			ModeHandler* mh = ServerInstance->Modes->FindMode(mchar, MODETYPE_CHANNEL);
			if (mh && mh->GetPrefixRank() <= prefix_value)
			{
				m->second->modes =
					m->second->modes.substr(0,i-1) +
					(adding ? std::string(1, prefix) : "") +
					m->second->modes.substr(mchar == prefix ? i+1 : i);
				return;
			}
		}
		m->second->modes += std::string(1, prefix);
	}
}

void Channel::RemoveAllPrefixes(User* user)
{
	UserMembIter m = userlist.find(user);
	if (m != userlist.end())
	{
		m->second->modes.clear();
	}
}
