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

/* $Core */

#include "inspircd.h"
#include <cstdarg>
#include "mode.h"

Channel::Channel(const std::string &cname, time_t ts)
{
	chan_hash::iterator findchan = ServerInstance->chanlist->find(cname);
	if (findchan != ServerInstance->chanlist->end())
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

void Channel::SetMode(ModeHandler* mh, bool on)
{
	modes[mh->GetModeChar() - 65] = on;
}

void Channel::SetModeParam(char mode, const std::string& parameter)
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

void Channel::SetModeParam(ModeHandler* mode, const std::string& parameter)
{
	SetModeParam(mode->GetModeChar(), parameter);
}

std::string Channel::GetModeParameter(char mode)
{
	CustomModeList::iterator n = custom_mode_params.find(mode);
	if (n != custom_mode_params.end())
		return n->second;
	return "";
}

std::string Channel::GetModeParameter(ModeHandler* mode)
{
	CustomModeList::iterator n = custom_mode_params.find(mode->GetModeChar());
	if (n != custom_mode_params.end())
		return n->second;
	return "";
}

int Channel::SetTopic(User *u, std::string &ntopic, bool forceset)
{
	if (!u)
		u = ServerInstance->FakeClient;
	if (IS_LOCAL(u) && !forceset)
	{
		ModResult res;
		FIRST_MOD_RESULT(OnPreTopicChange, res, (u,this,ntopic));

		if (res == MOD_RES_DENY)
			return CMD_FAILURE;
		if (res != MOD_RES_ALLOW)
		{
			if (!this->HasUser(u))
			{
				u->WriteNumeric(442, "%s %s :You're not on that channel!",u->nick.c_str(), this->name.c_str());
				return CMD_FAILURE;
			}
			if (IsModeSet('t') && !ServerInstance->OnCheckExemption(u,this,"topiclock").check(GetPrefixValue(u) >= HALFOP_VALUE))
			{
				u->WriteNumeric(482, "%s %s :You do not have access to change the topic on this channel", u->nick.c_str(), this->name.c_str());
				return CMD_FAILURE;
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

	FOREACH_MOD(I_OnPostTopicChange,OnPostTopicChange(u, this, this->topic));

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

void Channel::DelUser(User* user)
{
	UserMembIter a = userlist.find(user);

	if (a != userlist.end())
	{
		a->second->cull();
		delete a->second;
		userlist.erase(a);
	}

	if (userlist.empty())
	{
		ModResult res;
		FIRST_MOD_RESULT(OnChannelPreDelete, res, (this));
		if (res == MOD_RES_DENY)
			return;
		chan_hash::iterator iter = ServerInstance->chanlist->find(this->name);
		/* kill the record */
		if (iter != ServerInstance->chanlist->end())
		{
			FOREACH_MOD(I_OnChannelDelete, OnChannelDelete(this));
			ServerInstance->chanlist->erase(iter);
		}
		ServerInstance->GlobalCulls.AddItem(this);
	}
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
	ServerInstance->Logs->Log("CHANNELS", DEBUG, "SetDefaultModes %s",
		ServerInstance->Config->DefaultModes.c_str());
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
Channel* Channel::JoinUser(User *user, const char* cn, bool override, const char* key, bool bursting, time_t TS)
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
		if (user->HasPrivPermission("channels/high-join-limit"))
		{
			if (user->chans.size() >= ServerInstance->Config->OperMaxChans)
			{
				user->WriteNumeric(ERR_TOOMANYCHANNELS, "%s %s :You are on too many channels",user->nick.c_str(), cn);
				return NULL;
			}
		}
		else
		{
			unsigned int maxchans = user->GetClass()->maxchans;
			if (!maxchans)
				maxchans = ServerInstance->Config->MaxChans;
			if (user->chans.size() >= maxchans)
			{
				user->WriteNumeric(ERR_TOOMANYCHANNELS, "%s %s :You are on too many channels",user->nick.c_str(), cn);
				return NULL;
			}
		}
	}

	strlcpy(cname, cn, ServerInstance->Config->Limits.ChanMax);
	Ptr = ServerInstance->FindChan(cname);
	bool created_by_local = false;

	if (!Ptr)
	{
		/*
		 * Fix: desync bug was here, don't set @ on remote users - spanningtree handles their permissions. bug #358. -- w00t
		 */
		if (!IS_LOCAL(user))
		{
			if (!TS)
				ServerInstance->Logs->Log("CHANNEL",DEBUG,"*** BUG *** Channel::JoinUser called for REMOTE user '%s' on channel '%s' but no TS given!", user->nick.c_str(), cn);
		}
		else
		{
			privs = "o";
			created_by_local = true;
		}

		if (IS_LOCAL(user) && override == false)
		{
			ModResult MOD_RESULT;
			FIRST_MOD_RESULT(OnUserPreJoin, MOD_RESULT, (user, NULL, cname, privs, key ? key : ""));
			if (MOD_RESULT == MOD_RES_DENY)
				return NULL;
		}

		Ptr = new Channel(cname, TS);
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
			FIRST_MOD_RESULT(OnUserPreJoin, MOD_RESULT, (user, Ptr, cname, privs, key ? key : ""));
			if (MOD_RESULT == MOD_RES_DENY)
			{
				return NULL;
			}
			else if (MOD_RESULT == MOD_RES_PASSTHRU)
			{
				std::string ckey = Ptr->GetModeParameter('k');
				bool invited = IS_LOCAL(user)->IsInvited(Ptr->name.c_str());
				bool can_bypass = ServerInstance->Config->InvBypassModes && invited;

				if (!ckey.empty())
				{
					FIRST_MOD_RESULT(OnCheckKey, MOD_RESULT, (user, Ptr, key ? key : ""));
					if (!MOD_RESULT.check((key && ckey == key) || can_bypass))
					{
						// If no key provided, or key is not the right one, and can't bypass +k (not invited or option not enabled)
						user->WriteNumeric(ERR_BADCHANNELKEY, "%s %s :Cannot join channel (Incorrect channel key)",user->nick.c_str(), Ptr->name.c_str());
						return NULL;
					}
				}

				if (Ptr->IsModeSet('i'))
				{
					FIRST_MOD_RESULT(OnCheckInvite, MOD_RESULT, (user, Ptr));
					if (!MOD_RESULT.check(invited))
					{
						user->WriteNumeric(ERR_INVITEONLYCHAN, "%s %s :Cannot join channel (Invite only)",user->nick.c_str(), Ptr->name.c_str());
						return NULL;
					}
				}

				std::string limit = Ptr->GetModeParameter('l');
				if (!limit.empty())
				{
					FIRST_MOD_RESULT(OnCheckLimit, MOD_RESULT, (user, Ptr));
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
					IS_LOCAL(user)->RemoveInvite(Ptr->name.c_str());
				}
			}
		}
	}

	if (created_by_local)
	{
		/* As spotted by jilles, dont bother to set this on remote users */
		Ptr->SetDefaultModes();
	}

	return Channel::ForceChan(Ptr, user, privs, bursting, created_by_local);
}

Channel* Channel::ForceChan(Channel* Ptr, User* user, const std::string &privs, bool bursting, bool created)
{
	std::string nick = user->nick;

	Membership* memb = Ptr->AddUser(user);
	user->chans.insert(Ptr);

	for (std::string::const_iterator x = privs.begin(); x != privs.end(); x++)
	{
		const char status = *x;
		ModeHandler* mh = ServerInstance->Modes->FindMode(status, MODETYPE_CHANNEL);
		if (mh)
		{
			/* Set, and make sure that the mode handler knows this mode was now set */
			Ptr->SetPrefix(user, mh->GetModeChar(), true);
			mh->OnModeChange(ServerInstance->FakeClient, ServerInstance->FakeClient, Ptr, nick, true);
		}
	}

	CUList except_list;
	FOREACH_MOD(I_OnUserJoin,OnUserJoin(memb, bursting, created, except_list));

	Ptr->WriteAllExcept(user, false, 0, except_list, "JOIN :%s", Ptr->name.c_str());

	/* Theyre not the first ones in here, make sure everyone else sees the modes we gave the user */
	std::string ms = memb->modes;
	for(unsigned int i=0; i < memb->modes.length(); i++)
		ms.append(" ").append(user->nick);
	if ((Ptr->GetUserCounter() > 1) && (ms.length()))
		Ptr->WriteAllExceptSender(user, ServerInstance->Config->CycleHostsFromUser, 0, "MODE %s +%s", Ptr->name.c_str(), ms.c_str());

	if (IS_LOCAL(user))
	{
		if (Ptr->topicset)
		{
			user->WriteNumeric(RPL_TOPIC, "%s %s :%s", user->nick.c_str(), Ptr->name.c_str(), Ptr->topic.c_str());
			user->WriteNumeric(RPL_TOPICTIME, "%s %s %s %lu", user->nick.c_str(), Ptr->name.c_str(), Ptr->setby.c_str(), (unsigned long)Ptr->topicset);
		}
		Ptr->UserList(user);
	}
	FOREACH_MOD(I_OnPostJoin,OnPostJoin(memb));
	return Ptr;
}

bool Channel::IsBanned(User* user)
{
	ModResult result;
	FIRST_MOD_RESULT(OnCheckChannelBan, result, (user, this));

	if (result != MOD_RES_PASSTHRU)
		return (result == MOD_RES_DENY);

	for (BanList::iterator i = this->bans.begin(); i != this->bans.end(); i++)
	{
		if (CheckBan(user, i->data))
			return true;
	}
	return false;
}

bool Channel::CheckBan(User* user, const std::string& mask)
{
	ModResult result;
	FIRST_MOD_RESULT(OnCheckBan, result, (user, this, mask));
	if (result != MOD_RES_PASSTHRU)
		return (result == MOD_RES_DENY);

	// extbans were handled above, if this is one it obviously didn't match
	if (mask[1] == ':')
		return false;

	std::string::size_type at = mask.find('@');
	if (at == std::string::npos)
		return false;

	char tomatch[MAXBUF];
	snprintf(tomatch, MAXBUF, "%s!%s", user->nick.c_str(), user->ident.c_str());
	std::string prefix = mask.substr(0, at);
	if (InspIRCd::Match(tomatch, prefix, NULL))
	{
		std::string suffix = mask.substr(at + 1);
		if (InspIRCd::Match(user->host, suffix, NULL) ||
			InspIRCd::Match(user->dhost, suffix, NULL) ||
			InspIRCd::MatchCIDR(user->GetIPString(), suffix, NULL))
			return true;
	}
	return false;
}

ModResult Channel::GetExtBanStatus(User *user, char type)
{
	ModResult rv;
	FIRST_MOD_RESULT(OnExtBanCheck, rv, (user, this, type));
	if (rv != MOD_RES_PASSTHRU)
		return rv;
	for (BanList::iterator i = this->bans.begin(); i != this->bans.end(); i++)
	{
		if (i->data[0] == type && i->data[1] == ':')
		{
			std::string val = i->data.substr(2);
			if (CheckBan(user, val))
				return MOD_RES_DENY;
		}
	}
	return MOD_RES_PASSTHRU;
}

/* Channel::PartUser
 * remove a channel from a users record, and return the number of users left.
 * Therefore, if this function returns 0 the caller should delete the Channel.
 */
void Channel::PartUser(User *user, std::string &reason)
{
	if (!user)
		return;

	Membership* memb = GetUser(user);

	if (memb)
	{
		CUList except_list;
		FOREACH_MOD(I_OnUserPart,OnUserPart(memb, reason, except_list));

		WriteAllExcept(user, false, 0, except_list, "PART %s%s%s", this->name.c_str(), reason.empty() ? "" : " :", reason.c_str());

		user->chans.erase(this);
		this->RemoveAllPrefixes(user);
	}

	this->DelUser(user);
}

void Channel::KickUser(User *src, User *user, const char* reason)
{
	if (!src || !user || !reason)
		return;

	Membership* memb = GetUser(user);
	if (IS_LOCAL(src))
	{
		if (!memb)
		{
			src->WriteNumeric(ERR_USERNOTINCHANNEL, "%s %s %s :They are not on that channel",src->nick.c_str(), user->nick.c_str(), this->name.c_str());
			return;
		}
		if ((ServerInstance->ULine(user->server)) && (!ServerInstance->ULine(src->server)))
		{
			src->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Only a u-line may kick a u-line from a channel.",src->nick.c_str(), this->name.c_str());
			return;
		}

		ModResult res;
		if (ServerInstance->ULine(src->server))
			res = MOD_RES_ALLOW;
		else
			FIRST_MOD_RESULT(OnUserPreKick, res, (src,memb,reason));

		if (res == MOD_RES_DENY)
			return;

		if (res == MOD_RES_PASSTHRU)
		{
			unsigned int them = this->GetPrefixValue(src);
			unsigned int req = HALFOP_VALUE;
			for (std::string::size_type i = 0; i < memb->modes.length(); i++)
			{
				ModeHandler* mh = ServerInstance->Modes->FindMode(memb->modes[i], MODETYPE_CHANNEL);
				if (mh && mh->GetLevelRequired() > req)
					req = mh->GetLevelRequired();
			}

			if (them < req)
			{
				src->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must be a channel %soperator",
					src->nick.c_str(), this->name.c_str(), req > HALFOP_VALUE ? "" : "half-");
				return;
			}
		}
	}

	if (memb)
	{
		CUList except_list;
		FOREACH_MOD(I_OnUserKick,OnUserKick(src, memb, reason, except_list));

		WriteAllExcept(src, false, 0, except_list, "KICK %s %s :%s", name.c_str(), user->nick.c_str(), reason);

		user->chans.erase(this);
		this->RemoveAllPrefixes(user);
	}

	this->DelUser(user);
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

void Channel::WriteChannelWithServ(const std::string& ServName, const char* text, ...)
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

void Channel::WriteChannelWithServ(const std::string& ServName, const std::string &text)
{
	char tb[MAXBUF];

	snprintf(tb,MAXBUF,":%s %s", ServName.empty() ? ServerInstance->Config->ServerName.c_str() : ServName.c_str(), text.c_str());
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

	snprintf(tb,MAXBUF,":%s %s", serversource ? ServerInstance->Config->ServerName.c_str() : user->GetFullHost().c_str(), text.c_str());
	std::string out = tb;

	this->RawWriteAllExcept(user, serversource, status, except_list, std::string(tb));
}

void Channel::RawWriteAllExcept(User* user, bool serversource, char status, CUList &except_list, const std::string &out)
{
	unsigned int minrank = 0;
	if (status)
	{
		ModeHandler* mh = ServerInstance->Modes->FindPrefix(status);
		if (mh)
			minrank = mh->GetPrefixRank();
	}
	for (UserMembIter i = userlist.begin(); i != userlist.end(); i++)
	{
		if (IS_LOCAL(i->first) && (except_list.find(i->first) == except_list.end()))
		{
			/* User doesn't have the status we're after */
			if (minrank && i->second->getRank() < minrank)
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
			if (n == 'k' - 65 && !showkey)
			{
				extparam = "<key>";
			}
			else
			{
				extparam = this->GetModeParameter(n + 65);
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

	if (!IS_LOCAL(user))
		return;

	if (this->IsModeSet('s') && !this->HasUser(user) && !user->HasPrivPermission("channels/auspex"))
	{
		user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(), this->name.c_str());
		return;
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

		FOREACH_MOD(I_OnNamesListItem, OnNamesListItem(user, i->second, prefixlist, nick));

		/* Nick was nuked, a module wants us to skip it */
		if (nick.empty())
			continue;

		size_t ptrlen = 0;

		if (curlen + prefixlist.length() + nick.length() + 1 > 480)
		{
			/* list overflowed into multiple numerics */
			user->WriteNumeric(RPL_NAMREPLY, std::string(list));

			/* reset our lengths */
			dlen = curlen = snprintf(list,MAXBUF,"%s %c %s :", user->nick.c_str(), this->IsModeSet('s') ? '@' : this->IsModeSet('p') ? '*' : '=', this->name.c_str());
			ptr = list + dlen;

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
			if (mh && mh->GetPrefixRank() > bestrank && mh->GetPrefix())
			{
				bestrank = mh->GetPrefixRank();
				pf[0] = mh->GetPrefix();
			}
		}
	}
	return pf;
}

unsigned int Membership::getRank()
{
	char mchar = modes.c_str()[0];
	unsigned int rv = 0;
	if (mchar)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(mchar, MODETYPE_CHANNEL);
		if (mh)
			rv = mh->GetPrefixRank();
	}
	return rv;
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
	UserMembIter m = userlist.find(user);
	if (m == userlist.end())
		return 0;
	return m->second->getRank();
}

bool Channel::SetPrefix(User* user, char prefix, bool adding)
{
	ModeHandler* delta_mh = ServerInstance->Modes->FindMode(prefix, MODETYPE_CHANNEL);
	if (!delta_mh)
		return false;
	UserMembIter m = userlist.find(user);
	if (m == userlist.end())
		return false;
	for(unsigned int i=0; i < m->second->modes.length(); i++)
	{
		char mchar = m->second->modes[i];
		ModeHandler* mh = ServerInstance->Modes->FindMode(mchar, MODETYPE_CHANNEL);
		if (mh && mh->GetPrefixRank() <= delta_mh->GetPrefixRank())
		{
			m->second->modes =
				m->second->modes.substr(0,i) +
				(adding ? std::string(1, prefix) : "") +
				m->second->modes.substr(mchar == prefix ? i+1 : i);
			return adding != (mchar == prefix);
		}
	}
	if (adding)
		m->second->modes += std::string(1, prefix);
	return adding;
}

void Channel::RemoveAllPrefixes(User* user)
{
	UserMembIter m = userlist.find(user);
	if (m != userlist.end())
	{
		m->second->modes.clear();
	}
}
