/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "cull_list.h"
#include <cstdarg>
#include "mode.h"

Channel::Channel(const std::string &cname, time_t ts)
	: Extensible(EXTENSIBLE_CHANNEL), name(cname), age(ts)
{
	if (!age)
		throw CoreException("Cannot create channel with zero timestamp");

	chan_hash::iterator findchan = ServerInstance->chanlist->find(name);
	if (findchan != ServerInstance->chanlist->end())
		throw CoreException("Cannot create duplicate channel " + name);


	ServerInstance->chanlist->insert(std::make_pair(name, this));

	maxbans = topicset = 0;
	modebits.reset();
}

bool Channel::IsModeSet(char mode)
{
	ModeHandler* mh = ServerInstance->Modes->FindMode(mode, MODETYPE_CHANNEL);
	if (!mh)
		return false;
	return modebits[mh->id.GetID()];
}

bool Channel::IsModeSet(const std::string& mode)
{
	ModeHandler* mh = ServerInstance->Modes->FindMode(mode);
	if (!mh)
		return false;
	return modebits[mh->id.GetID()];
}

void Channel::SetMode(ModeHandler* mh, bool on)
{
	modebits[mh->id.GetID()] = on;
}

void Channel::SetModeParam(ModeHandler* mode, const std::string& parameter)
{
	CustomModeList::iterator n = custom_mode_params.find(mode->id);
	// always erase, even if changing, so that the map gets the new value
	if (n != custom_mode_params.end())
		custom_mode_params.erase(n);
	SetMode(mode, !parameter.empty());
	if (!parameter.empty())
		custom_mode_params[mode->id] = parameter;
}

std::string Channel::GetModeParameter(char mode)
{
	ModeHandler* mh = ServerInstance->Modes->FindMode(mode, MODETYPE_CHANNEL);
	if (!mh)
		return "";
	return GetModeParameter(mh);
}

std::string Channel::GetModeParameter(ModeHandler* mode)
{
	CustomModeList::iterator n = custom_mode_params.find(mode->id);
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
		Membership* memb = GetUser(u);
		if (!memb)
		{
			u->WriteNumeric(442, "%s %s :You're not on that channel!",u->nick.c_str(), this->name.c_str());
			return CMD_FAILURE;
		}
		ModResult res = ServerInstance->CheckExemption(u,this,"topiclock");
		if (IsModeSet('t') && !res.check(memb->GetAccessRank() >= HALFOP_VALUE))
		{
			u->WriteNumeric(482, "%s %s :You do not have access to change the topic on this channel", u->nick.c_str(), this->name.c_str());
			return CMD_FAILURE;
		}
	}

	topic = ntopic;
	topicset = ServerInstance->Time();
	setby = ServerInstance->Config->FullHostInTopic ? u->GetFullHost() : u->nick;

	this->WriteChannel(u, "TOPIC %s :%s", this->name.c_str(), this->topic.c_str());

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

	if (!userlist.empty())
		return;

	ModResult res;
	FIRST_MOD_RESULT(OnChannelPreDelete, res, (this));
	if (res == MOD_RES_DENY)
		return;
	/* kill the record */
	chan_hash::iterator iter = ServerInstance->chanlist->find(this->name);
	if (iter != ServerInstance->chanlist->end() && iter->second == this)
	{
		FOREACH_MOD(I_OnChannelDelete, OnChannelDelete(this));
		ServerInstance->chanlist->erase(iter);
	}
	ServerInstance->GlobalCulls->AddItem(this);
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
			if (mode->GetPrefixRank())
				continue;
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
Channel* Channel::JoinUser(User *user, const std::string& cn, bool override, const std::string& key, bool bursting, time_t TS)
{
	// Fix: unregistered users could be joined using /SAJOIN
	if (!user || user->registered != REG_ALL)
		return NULL;

	Channel *Ptr;

	/*
	 * We don't restrict the number of channels that remote users or users that are override-joining may be in.
	 * We restrict local users to the maximum defined in their <connect> block
	 * This is a lot more logical than how it was formerly. -- w00t
	 */
	if (IS_LOCAL(user) && !override)
	{
		unsigned int maxchans = IS_LOCAL(user)->MyClass->maxchans;
		// default if not set in <connect> is 20
		if (!maxchans)
			maxchans = 20;
		if (user->chans.size() >= maxchans)
		{
			user->WriteNumeric(ERR_TOOMANYCHANNELS, "%s %s :You are on too many channels",user->nick.c_str(), cn.c_str());
			return NULL;
		}
	}

	Ptr = ServerInstance->FindChan(cn);
	bool created_by_local = false;

	ChannelPermissionData perm(user, Ptr, cn, key);
	if (override)
		perm.result = MOD_RES_ALLOW;
	if (IS_LOCAL(user))
		perm.invited = IS_LOCAL(user)->IsInvited(cn);

	if (Ptr == NULL)
	{
		if (IS_LOCAL(user))
		{
			// for local users only, creating the channel gets default modes
			perm.privs = ServerInstance->Config->DefaultModes.substr(0, ServerInstance->Config->DefaultModes.find(' '));
			created_by_local = true;

			FOR_EACH_MOD(OnCheckJoin, (perm));
			FOR_EACH_MOD(OnPermissionCheck, (perm));
			if (perm.result == MOD_RES_DENY)
			{
				if (!perm.reason.empty())
					user->SendText(perm.reason);
				return NULL;
			}
		}
		if (TS == 0)
			TS = ServerInstance->Time();

		Ptr = new Channel(cn, TS);
	}
	else
	{
		/* Already on the channel */
		if (Ptr->HasUser(user))
			return NULL;

		perm.needs_invite = Ptr->IsModeSet('i');

		/*
		 * remote users are allowed us to bypass channel modes
		 * and bans (used by servers)
		 */
		if (IS_LOCAL(user))
		{
			FOR_EACH_MOD(OnCheckJoin, (perm));
			if (perm.result == MOD_RES_PASSTHRU)
			{
				std::string ckey = Ptr->GetModeParameter('k');
				// invites will bypass +iklb because someone explicitly approved it
				bool inv_bypass = ServerInstance->Config->InvBypassModes && perm.invited;
				// key use just bypasses +i, since it's static and is known by all valid members
				bool key_bypass = ServerInstance->Config->InvBypassModes && !ckey.empty() && key == ckey;

				if (!ckey.empty() && ckey != key && !inv_bypass)
				{
					perm.result = MOD_RES_DENY;
					perm.ErrorNumeric(ERR_BADCHANNELKEY, "%s :Cannot join channel (Incorrect channel key)", Ptr->name.c_str());
				}

				if (perm.needs_invite && !perm.invited && !key_bypass)
				{
					perm.result = MOD_RES_DENY;
					perm.ErrorNumeric(ERR_INVITEONLYCHAN, "%s :Cannot join channel (Invite only)", Ptr->name.c_str());
				}

				std::string limit = Ptr->GetModeParameter('l');
				if (!limit.empty() && Ptr->GetUserCounter() >= atol(limit.c_str()) && !inv_bypass)
				{
					perm.result = MOD_RES_DENY;
					perm.ErrorNumeric(ERR_CHANNELISFULL, "%s :Cannot join channel (Channel is full)", Ptr->name.c_str());
				}

				if (Ptr->IsBanned(user) && !inv_bypass)
				{
					perm.result = MOD_RES_DENY;
					perm.ErrorNumeric(ERR_BANNEDFROMCHAN, "%s :Cannot join channel (You're banned)", Ptr->name.c_str());
				}
			}

			FOR_EACH_MOD(OnPermissionCheck, (perm));
			if (perm.result == MOD_RES_DENY)
			{
				if (!perm.reason.empty())
					user->SendText(perm.reason);
				return NULL;
			}
		}
	}
	/*
	 * If the user has invites for this channel, remove them now after a successful join so they
	 * don't build up. This is harmless if it was changed to true; if changed to false, we didn't
	 * "use" the invite.
	 */
	if (IS_LOCAL(user) && perm.invited)
		IS_LOCAL(user)->RemoveInvite(Ptr->name);

	if (created_by_local)
		Ptr->SetDefaultModes();

	/* Now actually do the work of the join */
	Membership* memb = Ptr->AddUser(user);
	user->chans.insert(memb);

	for (std::string::const_iterator x = perm.privs.begin(); x != perm.privs.end(); x++)
	{
		const char status = *x;
		ModeHandler* mh = ServerInstance->Modes->FindMode(status, MODETYPE_CHANNEL);
		if (mh && mh->GetPrefixRank())
		{
			/* Set, and make sure that the mode handler knows this mode was now set */
			Ptr->SetPrefix(user, mh->GetModeChar(), true);
			mh->OnModeChange(ServerInstance->FakeClient, ServerInstance->FakeClient, Ptr, user->nick, true);
		}
	}

	CUList except_list;
	FOREACH_MOD(I_OnUserJoin,OnUserJoin(memb, bursting, created_by_local, except_list));

	Ptr->WriteAllExcept(user, false, 0, except_list, "JOIN :%s", Ptr->name.c_str());

	/* Show the on-join modes to everyone else */
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

	ModeHandler* ban = ServerInstance->Modes->FindMode("ban");
	if (!ban)
		return false;
	const modelist *bans = ban->GetList(this);
	if (bans)
	{
		for (modelist::const_iterator it = bans->begin(); it != bans->end(); it++)
		{
			if (CheckBan(user, it->mask))
				return true;
		}
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

	ModeHandler* ban = ServerInstance->Modes->FindMode("ban");
	if (!ban)
		return MOD_RES_PASSTHRU;
	const modelist *bans = ban->GetList(this);
	if (bans)
	{
		for (modelist::const_iterator it = bans->begin(); it != bans->end(); it++)
		{
			if (it->mask[0] == type && it->mask[1] == ':')
			{
				std::string val = it->mask.substr(2);
				if (CheckBan(user, val))
					return MOD_RES_DENY;
			}
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

		user->chans.erase(memb);
		this->RemoveAllPrefixes(user);
	}

	this->DelUser(user);
}

void Channel::KickUser(User *src, User *user, const std::string& reason)
{
	if (!src || !user)
		return;

	Membership* memb = GetUser(user);
	if (IS_LOCAL(src))
	{
		if (!memb)
		{
			src->WriteNumeric(ERR_USERNOTINCHANNEL, "%s %s %s :They are not on that channel",src->nick.c_str(), user->nick.c_str(), this->name.c_str());
			return;
		}
		if (ServerInstance->ULine(user->server))
		{
			src->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You may not kick a u-lined client.",src->nick.c_str(), this->name.c_str());
			return;
		}

		PermissionData perm(src, "kick", this, memb->user, false);
		// pre-populate the error message to something sensible
		perm.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :You do not have access to kick in this channel", this->name.c_str());
		unsigned int rankKicker = GetAccessRank(src);
		unsigned int rankKickee = memb->GetProtectRank();
		if (rankKicker < rankKickee)
		{
			perm.result = MOD_RES_DENY;
			perm.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :They have a higher prefix set", this->name.c_str());
		}

		FOR_EACH_MOD(OnPermissionCheck, (perm));

		if (!perm.result.check(rankKicker >= HALFOP_VALUE))
		{
			if (!perm.reason.empty())
				src->SendText(perm.reason);
			return;
		}
	}

	if (memb)
	{
		CUList except_list;
		FOREACH_MOD(I_OnUserKick,OnUserKick(src, memb, reason, except_list));

		WriteAllExcept(src, false, 0, except_list, "KICK %s %s :%s", name.c_str(), user->nick.c_str(), reason.c_str());

		user->chans.erase(memb);
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
			if (minrank && i->second->GetAccessRank() < minrank)
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

void Channel::ChanModes(irc::modestacker& cmodes, ModeListType type)
{
	for(ModeIDIter id; id; id++)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(id);
		if (!mh || mh->GetModeType() != MODETYPE_CHANNEL)
			continue;
		if (mh->IsListMode() && type == MODELIST_FULL)
		{
			mh->PopulateChanModes(this, cmodes);
		}
		else if (this->IsModeSet(mh))
		{
			irc::modechange mc(id, this->GetModeParameter(mh), true);
			if (type == MODELIST_PUBLIC && mh->name == "key")
				mc.value = "<key>";
			cmodes.push(mc);
		}
	}
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
			if (mh && mh->GetPrefixRank() > bestrank && mh->GetPrefix())
			{
				bestrank = mh->GetPrefixRank();
				pf[0] = mh->GetPrefix();
			}
		}
	}
	return pf;
}

unsigned int Membership::GetAccessRank()
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

unsigned int Membership::GetProtectRank()
{
	const char* mchar = modes.c_str();
	unsigned int rv = 0;
	while (*mchar)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(*mchar, MODETYPE_CHANNEL);
		if (mh && rv < mh->GetLevelRequired())
			rv = mh->GetLevelRequired();
		mchar++;
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

Channel* Channel::Nuke(Channel* old, const std::string& channel, time_t newTS)
{
	time_t oldTS = old->age;
	ServerInstance->SNO->WriteToSnoMask('d', "Recreating channel " + channel);
	if (ServerInstance->Config->AnnounceTSChange)
		old->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :TS for %s changed from %lu to %lu",
			old->name.c_str(), channel.c_str(), (unsigned long) oldTS, (unsigned long) newTS);

	// prepare a mode change that removes all modes on the channel
	irc::modestacker stack;
	for (ModeIDIter id; id; id++)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(id);

		if (mh && mh->GetModeType() == MODETYPE_CHANNEL)
			mh->RemoveMode(old, &stack);
	}

	// don't process the change, just send it to clients
	ServerInstance->Modes->Send(ServerInstance->FakeClient, old, stack);

	// unhook the old channel
	chan_hash::iterator iter = ServerInstance->chanlist->find(old->name);
	ServerInstance->chanlist->erase(iter);

	// create the new channel (which inserts itself in chanlist)
	Channel* chan = new Channel(channel, newTS);

	// migrate all the users to the new channel
	// This has the side effect of dropping their permissions (op/voice/etc)
	for(UserMembIter i = old->userlist.begin(); i != old->userlist.end(); i++)
	{
		User* u = i->first;
		Membership* memb = i->second;
		u->chans.erase(memb);
		memb->cull();
		delete memb;
		memb = chan->AddUser(u);
		u->chans.insert(memb);
		FOREACH_MOD(I_OnPostJoin,OnPostJoin(memb));
	}
	// nuke the old channel
	old->userlist.clear();
	old->cull();
	delete old;

	return chan;
}
