/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006, 2008 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2003-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

bool Channel::IsModeSet(char mode)
{
	return modes[mode-65];
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
	if (u && IS_LOCAL(u))
	{
		if(!forceset)
		{
			int MOD_RESULT = 0;
			/* 0: check status, 1: don't, -1: disallow change silently */

			FOREACH_RESULT(I_OnLocalTopicChange,OnLocalTopicChange(u,this,ntopic));

			if (MOD_RESULT == 1)
				return CMD_FAILURE;
			else if (MOD_RESULT == 0)
			{
				if (!this->HasUser(u))
				{
					u->WriteNumeric(442, "%s %s :You're not on that channel!",u->nick.c_str(), this->name.c_str());
					return CMD_FAILURE;
				}
				if ((this->IsModeSet('t')) && (this->GetStatus(u) < STATUS_HOP))
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

	if (u && IS_LOCAL(u))
	{
		FOREACH_MOD(I_OnPostLocalTopicChange,OnPostLocalTopicChange(u, this, this->topic));
	}

	return CMD_SUCCESS;
}

long Channel::GetUserCounter()
{
	return (this->internal_userlist.size());
}

void Channel::AddUser(User* user)
{
	internal_userlist[user] = user->nick;
}

unsigned long Channel::DelUser(User* user)
{
	CUListIter a = internal_userlist.find(user);

	if (a != internal_userlist.end())
	{
		internal_userlist.erase(a);
		/* And tidy any others... */
		DelOppedUser(user);
		DelHalfoppedUser(user);
		DelVoicedUser(user);
		RemoveAllPrefixes(user);
	}

	unsigned long remaining_users = internal_userlist.size();
	if (remaining_users == 0)
	{
		chan_hash::iterator iter = ServerInstance->chanlist->find(this->name);
		/* kill the record */
		if (iter != ServerInstance->chanlist->end())
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT_I(ServerInstance, I_OnChannelPreDelete, OnChannelPreDelete(this));
			if (MOD_RESULT == 1)
				return 1; // delete halted by module

			// Uninvite every user who were invited but haven't joined
			ClearInvites();
			FOREACH_MOD(I_OnChannelDelete, OnChannelDelete(this));
			ServerInstance->chanlist->erase(iter);
		}
	}

	return remaining_users;
}

bool Channel::HasUser(User* user)
{
	return (internal_userlist.find(user) != internal_userlist.end());
}

void Channel::AddOppedUser(User* user)
{
	internal_op_userlist[user] = user->nick;
}

void Channel::DelOppedUser(User* user)
{
	CUListIter a = internal_op_userlist.find(user);
	if (a != internal_op_userlist.end())
	{
		internal_op_userlist.erase(a);
		return;
	}
}

void Channel::AddHalfoppedUser(User* user)
{
	internal_halfop_userlist[user] = user->nick;
}

void Channel::DelHalfoppedUser(User* user)
{
	CUListIter a = internal_halfop_userlist.find(user);

	if (a != internal_halfop_userlist.end())
	{
		internal_halfop_userlist.erase(a);
	}
}

void Channel::AddVoicedUser(User* user)
{
	internal_voice_userlist[user] = user->nick;
}

void Channel::DelVoicedUser(User* user)
{
	CUListIter a = internal_voice_userlist.find(user);

	if (a != internal_voice_userlist.end())
	{
		internal_voice_userlist.erase(a);
	}
}

CUList* Channel::GetUsers()
{
	return &internal_userlist;
}

CUList* Channel::GetOppedUsers()
{
	return &internal_op_userlist;
}

CUList* Channel::GetHalfoppedUsers()
{
	return &internal_halfop_userlist;
}

CUList* Channel::GetVoicedUsers()
{
	return &internal_voice_userlist;
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
	int MOD_RESULT = 0;
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
		unsigned int realmax = Instance->Config->MaxChans;
		if (user->MyClass && user->MyClass->maxchans)
			realmax = user->MyClass->maxchans;
		if (user->HasPrivPermission("channels/high-join-limit") && Instance->Config->OperMaxChans > realmax)
			realmax = Instance->Config->OperMaxChans;
		if (user->chans.size() >= realmax)
		{
			user->WriteNumeric(ERR_TOOMANYCHANNELS, "%s %s :You are on too many channels",user->nick.c_str(), cn);
			return NULL;
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
				Instance->Logs->Log("CHANNELS",DEBUG,"*** BUG *** Channel::JoinUser called for REMOTE user '%s' on channel '%s' but no TS given!", user->nick.c_str(), cn);
		}
		else
		{
			privs = "@";
			created_by_local = true;
		}

		if (IS_LOCAL(user) && override == false)
		{
			MOD_RESULT = 0;
			FOREACH_RESULT_I(Instance,I_OnUserPreJoin, OnUserPreJoin(user, NULL, cname, privs, key ? key : ""));
			if (MOD_RESULT == 1)
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
		bool invited = user->IsInvited(Ptr->name.c_str());
		if (IS_LOCAL(user) && override == false)
		{
			MOD_RESULT = 0;
			FOREACH_RESULT_I(Instance,I_OnUserPreJoin, OnUserPreJoin(user, Ptr, cname, privs, key ? key : ""));
			if (MOD_RESULT == 1)
			{
				return NULL;
			}
			else if (MOD_RESULT == 0)
			{
				std::string ckey = Ptr->GetModeParameter('k');
				bool can_bypass = Instance->Config->InvBypassModes && invited;

				if (!ckey.empty())
				{
					MOD_RESULT = 0;
					FOREACH_RESULT_I(Instance, I_OnCheckKey, OnCheckKey(user, Ptr, key ? key : ""));
					if (!MOD_RESULT)
					{
						// If no key provided, or key is not the right one, and can't bypass +k (not invited or option not enabled)
						if ((!key || ckey != key) && !can_bypass)
						{
							user->WriteNumeric(ERR_BADCHANNELKEY, "%s %s :Cannot join channel (Incorrect channel key)",user->nick.c_str(), Ptr->name.c_str());
							return NULL;
						}
					}
				}

				if (Ptr->IsModeSet('i'))
				{
					MOD_RESULT = 0;
					FOREACH_RESULT_I(Instance,I_OnCheckInvite,OnCheckInvite(user, Ptr));
					if (!MOD_RESULT)
					{
						if (!invited)
						{
							user->WriteNumeric(ERR_INVITEONLYCHAN, "%s %s :Cannot join channel (Invite only)",user->nick.c_str(), Ptr->name.c_str());
							return NULL;
						}
					}
				}

				std::string limit = Ptr->GetModeParameter('l');
				if (!limit.empty())
				{
					MOD_RESULT = 0;
					FOREACH_RESULT_I(Instance, I_OnCheckLimit, OnCheckLimit(user, Ptr));
					if (!MOD_RESULT)
					{
						long llimit = atol(limit.c_str());
						if (Ptr->GetUserCounter() >= llimit && !can_bypass)
						{
							user->WriteNumeric(ERR_CHANNELISFULL, "%s %s :Cannot join channel (Channel is full)",user->nick.c_str(), Ptr->name.c_str());
							return NULL;
						}
					}
				}

				if (Ptr->IsBanned(user) && !can_bypass)
				{
					user->WriteNumeric(ERR_BANNEDFROMCHAN, "%s %s :Cannot join channel (You're banned)",user->nick.c_str(), Ptr->name.c_str());
					return NULL;
				}
			}
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

	if (created_by_local)
	{
		/* As spotted by jilles, dont bother to set this on remote users */
		Ptr->SetDefaultModes();
	}

	return Channel::ForceChan(Instance, Ptr, user, privs, bursting);
}

Channel* Channel::ForceChan(InspIRCd* Instance, Channel* Ptr, User* user, const std::string &privs, bool bursting)
{
	std::string nick = user->nick;
	bool silent = false;

	Ptr->AddUser(user);

	/* Just in case they have no permissions */
	user->chans[Ptr] = 0;

	for (std::string::const_iterator x = privs.begin(); x != privs.end(); x++)
	{
		const char status = *x;
		ModeHandler* mh = Instance->Modes->FindPrefix(status);
		if (mh)
		{
			/* Set, and make sure that the mode handler knows this mode was now set */
			Ptr->SetPrefix(user, status, mh->GetPrefixRank(), true);
			mh->OnModeChange(Instance->FakeClient, Instance->FakeClient, Ptr, nick, true);

			switch (mh->GetPrefix())
			{
				/* These logic ops are SAFE IN THIS CASE because if the entry doesnt exist,
				 * addressing operator[] creates it. If they do exist, it points to it.
				 * At all other times where we dont want to create an item if it doesnt exist, we
				 * must stick to ::find().
				 */
				case '@':
					user->chans[Ptr] |= UCMODE_OP;
				break;
				case '%':
					user->chans[Ptr] |= UCMODE_HOP;
				break;
				case '+':
					user->chans[Ptr] |= UCMODE_VOICE;
				break;
			}
		}
	}

	FOREACH_MOD_I(Instance,I_OnUserJoin,OnUserJoin(user, Ptr, bursting, silent));

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
	int result = 0;
	FOREACH_RESULT_MAP(I_OnCheckBan, OnCheckBan(user, this),
		result = banmatch_reduce(result, MOD_RESULT);
	);

	if (result)
		return (result < 0);

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

int Channel::GetExtBanStatus(const std::string &str, char type)
{
	int result = 0;
	FOREACH_RESULT_MAP(I_OnCheckStringExtBan, OnCheckStringExtBan(str, this, type),
		result = banmatch_reduce(result, MOD_RESULT);
	);

	if (result)
		return result;

	// nobody decided for us, check the ban list
	for (BanList::iterator i = this->bans.begin(); i != this->bans.end(); i++)
	{
		if (i->data[0] != type || i->data[1] != ':')
			continue;

		std::string maskptr = i->data.substr(2);
		ServerInstance->Logs->Log("EXTBANS", DEBUG, "Checking %s against %s, type is %c", str.c_str(), maskptr.c_str(), type);

		if (InspIRCd::Match(str, maskptr, NULL))
			return -1;
	}

	return 0;
}

int Channel::GetExtBanStatus(User *user, char type)
{
	int result = 0;
	FOREACH_RESULT_MAP(I_OnCheckExtBan, OnCheckExtBan(user, this, type),
		result = banmatch_reduce(result, MOD_RESULT);
	);

	if (result)
		return result;

	char mask[MAXBUF];
	int rv = 0;
	snprintf(mask, MAXBUF, "%s!%s@%s", user->nick.c_str(), user->ident.c_str(), user->GetIPString());

	// XXX: we should probably hook cloaked hosts in here somehow too..
	rv = banmatch_reduce(rv, this->GetExtBanStatus(mask, type));
	rv = banmatch_reduce(rv, this->GetExtBanStatus(user->GetFullHost(), type));
	rv = banmatch_reduce(rv, this->GetExtBanStatus(user->GetFullRealHost(), type));
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
	}

	return this->DelUser(user);
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
		int MOD_RESULT = 0;

		if (!ServerInstance->ULine(src->server))
		{
			MOD_RESULT = 0;
			FOREACH_RESULT(I_OnUserPreKick,OnUserPreKick(src,user,this,reason));
			if (MOD_RESULT == 1)
				return this->GetUserCounter();
		}
		/* Set to -1 by OnUserPreKick if explicit allow was set */
		if (MOD_RESULT != -1)
		{
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(src,user,this,AC_KICK));
			if ((MOD_RESULT == ACR_DENY) && (!ServerInstance->ULine(src->server)))
				return this->GetUserCounter();

			if ((MOD_RESULT == ACR_DEFAULT) || (!ServerInstance->ULine(src->server)))
			{
				int them = this->GetStatus(src);
				int us = this->GetStatus(user);
   			 	if ((them < STATUS_HOP) || (them < us))
				{
					src->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must be a channel %soperator",src->nick.c_str(), this->name.c_str(), them == STATUS_HOP ? "" : "half-");
					return this->GetUserCounter();
				}
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
	}

	return this->DelUser(user);
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
	CUList *ulist = this->GetUsers();
	char tb[MAXBUF];

	if (!user)
		return;

	snprintf(tb,MAXBUF,":%s %s", user->GetFullHost().c_str(), text.c_str());
	std::string out = tb;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
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
	CUList *ulist = this->GetUsers();
	char tb[MAXBUF];

	snprintf(tb,MAXBUF,":%s %s", ServName ? ServName : ServerInstance->Config->ServerName, text.c_str());
	std::string out = tb;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
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

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteAllExcept(user, serversource, status, except_list, std::string(textbuffer));
}

void Channel::WriteAllExcept(User* user, bool serversource, char status, CUList &except_list, const std::string &text)
{
	CUList *ulist = this->GetUsers();
	char tb[MAXBUF];

	snprintf(tb,MAXBUF,":%s %s", user->GetFullHost().c_str(), text.c_str());
	std::string out = tb;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((IS_LOCAL(i->first)) && (except_list.find(i->first) == except_list.end()))
		{
			/* User doesnt have the status we're after */
			if (status && !strchr(this->GetAllPrefixChars(i->first), status))
				continue;

			if (serversource)
				i->first->WriteServ(text);
			else
				i->first->Write(out);
		}
	}
}

void Channel::WriteAllExceptSender(User* user, bool serversource, char status, const std::string& text)
{
	CUList except_list;
	except_list[user] = user->nick;
	this->WriteAllExcept(user, serversource, status, except_list, std::string(text));
}

/*
 * return a count of the users on a specific channel accounting for
 * invisible users who won't increase the count. e.g. for /LIST
 */
int Channel::CountInvisible()
{
	int count = 0;
	CUList *ulist= this->GetUsers();
	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
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
void Channel::UserList(User *user, CUList *ulist)
{
	char list[MAXBUF];
	size_t dlen, curlen;
	int MOD_RESULT = 0;
	bool call_modules = true;

	if (!IS_LOCAL(user))
		return;

	FOREACH_RESULT(I_OnUserList,OnUserList(user, this, ulist));
	if (MOD_RESULT == 1)
		call_modules = false;

	if (MOD_RESULT != -1)
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

	if (!ulist)
		ulist = this->GetUsers();

	/* Improvement by Brain - this doesnt change in value, so why was it inside
	 * the loop?
	 */
	bool has_user = this->HasUser(user);

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((!has_user) && (i->first->IsModeSet('i')))
		{
			/*
			 * user is +i, and source not on the channel, does not show
			 * nick in NAMES list
			 */
			continue;
		}

		if (i->first->Visibility && !i->first->Visibility->VisibleTo(user))
			continue;

		std::string prefixlist = this->GetPrefixChar(i->first);
		std::string nick = i->first->nick;

		if (call_modules)
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

	prefixlist::iterator n = prefixes.find(user);
	if (n != prefixes.end())
	{
		if (n->second.size())
		{
			/* If the user has any prefixes, their highest prefix
			 * will always be at the head of the list, as the list is
			 * sorted in rank order highest first (see SetPrefix()
			 * for reasons why)
			 */
			*pf = n->second.begin()->first;
			return pf;
		}
	}

	*pf = 0;
	return pf;
}


const char* Channel::GetAllPrefixChars(User* user)
{
	static char prefix[MAXBUF];
	int ctr = 0;
	*prefix = 0;

	prefixlist::iterator n = prefixes.find(user);
	if (n != prefixes.end())
	{
		for (std::vector<prefixtype>::iterator x = n->second.begin(); x != n->second.end(); x++)
		{
			prefix[ctr++] = x->first;
		}
	}

	prefix[ctr] = 0;

	return prefix;
}

unsigned int Channel::GetPrefixValue(User* user)
{
	prefixlist::iterator n = prefixes.find(user);
	if (n != prefixes.end())
	{
		if (n->second.size())
			return n->second.begin()->second;
	}
	return 0;
}

int Channel::GetStatusFlags(User *user)
{
	UCListIter i = user->chans.find(this);
	if (i != user->chans.end())
	{
		return i->second;
	}
	return 0;
}

int Channel::GetStatus(User *user)
{
	if (ServerInstance->ULine(user->server))
		return STATUS_OP;

	UCListIter i = user->chans.find(this);
	if (i != user->chans.end())
	{
		if ((i->second & UCMODE_OP) > 0)
		{
			return STATUS_OP;
		}
		if ((i->second & UCMODE_HOP) > 0)
		{
			return STATUS_HOP;
		}
		if ((i->second & UCMODE_VOICE) > 0)
		{
			return STATUS_VOICE;
		}
		return STATUS_NORMAL;
	}
	return STATUS_NORMAL;
}

void Channel::SetPrefix(User* user, char prefix, unsigned int prefix_value, bool adding)
{
	prefixlist::iterator n = prefixes.find(user);
	prefixtype pfx = std::make_pair(prefix,prefix_value);
	if (adding)
	{
		if (n != prefixes.end())
		{
			if (std::find(n->second.begin(), n->second.end(), pfx) == n->second.end())
			{
				n->second.push_back(pfx);
				/* We must keep prefixes in rank order, largest first.
				 * This is for two reasons, firstly because x-chat *ass-u-me's* this
				 * state, and secondly it turns out to be a benefit to us later.
				 * See above in GetPrefix().
				 */
				std::sort(n->second.begin(), n->second.end(), ModeParser::PrefixComparison);
			}
		}
		else
		{
			pfxcontainer one;
			one.push_back(pfx);
			prefixes.insert(std::make_pair<User*,pfxcontainer>(user, one));
		}
	}
	else
	{
		if (n != prefixes.end())
		{
			pfxcontainer::iterator x = std::find(n->second.begin(), n->second.end(), pfx);
			if (x != n->second.end())
				n->second.erase(x);
		}
	}
}

void Channel::RemoveAllPrefixes(User* user)
{
	prefixlist::iterator n = prefixes.find(user);
	if (n != prefixes.end())
	{
		prefixes.erase(n);
	}
}

void Channel::AddInvitedUser(User* user)
{
	invitedusers.push_back(user);
}

void Channel::RemoveInvitedUser(User* user)
{
	InvitedUserList::iterator it = std::find(invitedusers.begin(), invitedusers.end(), user);
	if (it != invitedusers.end())
		invitedusers.erase(it);
}

void Channel::ClearInvites()
{
	InvitedUserList inv;

	/* This causes Channel::RemoveInvitedUser() to do nothing when
	 * User::RemoveInvite() calls it
	 */
	inv.swap(invitedusers);

	for (InvitedUserList::iterator i = inv.begin(); i != inv.end(); ++i)
	{
		(*i)->RemoveInvite(this->name.c_str());
	}
}
