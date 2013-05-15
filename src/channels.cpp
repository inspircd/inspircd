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
#include "listmode.h"
#include <cstdarg>
#include "mode.h"

static ModeReference ban(NULL, "ban");

Channel::Channel(const std::string &cname, time_t ts)
{
	if (!ServerInstance->chanlist->insert(std::make_pair(cname, this)).second)
		throw CoreException("Cannot create duplicate channel " + cname);

	this->name = cname;
	this->age = ts ? ts : ServerInstance->Time();

	topicset = 0;
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
	Membership*& memb = userlist[user];
	if (memb)
		return NULL;

	memb = new Membership(user, this);
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

		ClearInvites();
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
	ServerInstance->Logs->Log("CHANNELS", LOG_DEBUG, "SetDefaultModes %s",
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
Channel* Channel::JoinUser(LocalUser* user, std::string cname, bool override, const std::string& key)
{
	if (user->registered != REG_ALL)
	{
		ServerInstance->Logs->Log("CHANNELS", LOG_DEBUG, "Attempted to join unregistered user " + user->uuid + " to channel " + cname);
		return NULL;
	}

	/*
	 * We don't restrict the number of channels that remote users or users that are override-joining may be in.
	 * We restrict local users to MaxChans channels.
	 * We restrict local operators to OperMaxChans channels.
	 * This is a lot more logical than how it was formerly. -- w00t
	 */
	if (!override)
	{
		if (user->HasPrivPermission("channels/high-join-limit"))
		{
			if (user->chans.size() >= ServerInstance->Config->OperMaxChans)
			{
				user->WriteNumeric(ERR_TOOMANYCHANNELS, "%s %s :You are on too many channels",user->nick.c_str(), cname.c_str());
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
				user->WriteNumeric(ERR_TOOMANYCHANNELS, "%s %s :You are on too many channels",user->nick.c_str(), cname.c_str());
				return NULL;
			}
		}
	}

	// Crop channel name if it's too long
	if (cname.length() > ServerInstance->Config->Limits.ChanMax)
		cname.resize(ServerInstance->Config->Limits.ChanMax);

	Channel* chan = ServerInstance->FindChan(cname);
	bool created_by_local = (chan == NULL); // Flag that will be passed to modules in the OnUserJoin() hook later
	std::string privs; // Prefix mode(letter)s to give to the joining user

	if (!chan)
	{
		privs = "o";

		if (override == false)
		{
			// Ask the modules whether they're ok with the join, pass NULL as Channel* as the channel is yet to be created
			ModResult MOD_RESULT;
			FIRST_MOD_RESULT(OnUserPreJoin, MOD_RESULT, (user, NULL, cname, privs, key));
			if (MOD_RESULT == MOD_RES_DENY)
				return NULL; // A module wasn't happy with the join, abort
		}

		chan = new Channel(cname, ServerInstance->Time());
		// Set the default modes on the channel (<options:defaultmodes>)
		chan->SetDefaultModes();
	}
	else
	{
		/* Already on the channel */
		if (chan->HasUser(user))
			return NULL;

		if (override == false)
		{
			ModResult MOD_RESULT;
			FIRST_MOD_RESULT(OnUserPreJoin, MOD_RESULT, (user, chan, cname, privs, key));

			// A module explicitly denied the join and (hopefully) generated a message
			// describing the situation, so we may stop here without sending anything
			if (MOD_RESULT == MOD_RES_DENY)
				return NULL;

			// If no module returned MOD_RES_DENY or MOD_RES_ALLOW (which is the case
			// most of the time) then proceed to check channel modes +k, +i, +l and bans,
			// in this order.
			// If a module explicitly allowed the join (by returning MOD_RES_ALLOW),
			// then this entire section is skipped
			if (MOD_RESULT == MOD_RES_PASSTHRU)
			{
				std::string ckey = chan->GetModeParameter('k');
				bool invited = user->IsInvited(chan);
				bool can_bypass = ServerInstance->Config->InvBypassModes && invited;

				if (!ckey.empty())
				{
					FIRST_MOD_RESULT(OnCheckKey, MOD_RESULT, (user, chan, key));
					if (!MOD_RESULT.check((ckey == key) || can_bypass))
					{
						// If no key provided, or key is not the right one, and can't bypass +k (not invited or option not enabled)
						user->WriteNumeric(ERR_BADCHANNELKEY, "%s %s :Cannot join channel (Incorrect channel key)",user->nick.c_str(), chan->name.c_str());
						return NULL;
					}
				}

				if (chan->IsModeSet('i'))
				{
					FIRST_MOD_RESULT(OnCheckInvite, MOD_RESULT, (user, chan));
					if (!MOD_RESULT.check(invited))
					{
						user->WriteNumeric(ERR_INVITEONLYCHAN, "%s %s :Cannot join channel (Invite only)",user->nick.c_str(), chan->name.c_str());
						return NULL;
					}
				}

				std::string limit = chan->GetModeParameter('l');
				if (!limit.empty())
				{
					FIRST_MOD_RESULT(OnCheckLimit, MOD_RESULT, (user, chan));
					if (!MOD_RESULT.check((chan->GetUserCounter() < atol(limit.c_str()) || can_bypass)))
					{
						user->WriteNumeric(ERR_CHANNELISFULL, "%s %s :Cannot join channel (Channel is full)",user->nick.c_str(), chan->name.c_str());
						return NULL;
					}
				}

				if (chan->IsBanned(user) && !can_bypass)
				{
					user->WriteNumeric(ERR_BANNEDFROMCHAN, "%s %s :Cannot join channel (You're banned)",user->nick.c_str(), chan->name.c_str());
					return NULL;
				}

				/*
				 * If the user has invites for this channel, remove them now
				 * after a successful join so they don't build up.
				 */
				if (invited)
				{
					user->RemoveInvite(chan);
				}
			}
		}
	}

	// We figured that this join is allowed and also created the
	// channel if it didn't exist before, now do the actual join
	chan->ForceJoin(user, &privs, false, created_by_local);
	return chan;
}

void Channel::ForceJoin(User* user, const std::string* privs, bool bursting, bool created_by_local)
{
	Membership* memb = this->AddUser(user);
	if (!memb)
		return; // Already on the channel

	if (IS_SERVER(user))
	{
		ServerInstance->Logs->Log("CHANNELS", LOG_DEBUG, "Attempted to join server user " + user->uuid + " to channel " + this->name);
		return;
	}

	user->chans.insert(this);

	if (privs)
	{
		// If the user was granted prefix modes (in the OnUserPreJoin hook, or he's a
		// remote user and his own server set the modes), then set them internally now
		memb->modes = *privs;
		for (std::string::const_iterator i = privs->begin(); i != privs->end(); ++i)
		{
			ModeHandler* mh = ServerInstance->Modes->FindMode(*i, MODETYPE_CHANNEL);
			if (mh)
			{
				std::string nick = user->nick;
				/* Set, and make sure that the mode handler knows this mode was now set */
				this->SetPrefix(user, mh->GetModeChar(), true);
				mh->OnModeChange(ServerInstance->FakeClient, ServerInstance->FakeClient, this, nick, true);
			}
		}
	}

	// Tell modules about this join, they have the chance now to populate except_list with users we won't send the JOIN (and possibly MODE) to
	CUList except_list;
	FOREACH_MOD(I_OnUserJoin,OnUserJoin(memb, bursting, created_by_local, except_list));

	this->WriteAllExcept(user, false, 0, except_list, "JOIN :%s", this->name.c_str());

	/* Theyre not the first ones in here, make sure everyone else sees the modes we gave the user */
	if ((GetUserCounter() > 1) && (!memb->modes.empty()))
	{
		std::string ms = memb->modes;
		for(unsigned int i=0; i < memb->modes.length(); i++)
			ms.append(" ").append(user->nick);

		except_list.insert(user);
		this->WriteAllExcept(user, !ServerInstance->Config->CycleHostsFromUser, 0, except_list, "MODE %s +%s", this->name.c_str(), ms.c_str());
	}

	if (IS_LOCAL(user))
	{
		if (this->topicset)
		{
			user->WriteNumeric(RPL_TOPIC, "%s %s :%s", user->nick.c_str(), this->name.c_str(), this->topic.c_str());
			user->WriteNumeric(RPL_TOPICTIME, "%s %s %s %lu", user->nick.c_str(), this->name.c_str(), this->setby.c_str(), (unsigned long)this->topicset);
		}
		this->UserList(user);
	}

	FOREACH_MOD(I_OnPostJoin,OnPostJoin(memb));
}

bool Channel::IsBanned(User* user)
{
	ModResult result;
	FIRST_MOD_RESULT(OnCheckChannelBan, result, (user, this));

	if (result != MOD_RES_PASSTHRU)
		return (result == MOD_RES_DENY);

	ListModeBase* banlm = static_cast<ListModeBase*>(*ban);
	const ListModeBase::ModeList* bans = banlm->GetList(this);
	if (bans)
	{
		for (ListModeBase::ModeList::const_iterator it = bans->begin(); it != bans->end(); it++)
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
	if ((mask.length() <= 2) || (mask[1] == ':'))
		return false;

	std::string::size_type at = mask.find('@');
	if (at == std::string::npos)
		return false;

	const std::string nickIdent = user->nick + "!" + user->ident;
	std::string prefix = mask.substr(0, at);
	if (InspIRCd::Match(nickIdent, prefix, NULL))
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

	ListModeBase* banlm = static_cast<ListModeBase*>(*ban);
	const ListModeBase::ModeList* bans = banlm->GetList(this);
	if (bans)

	{
		for (ListModeBase::ModeList::const_iterator it = bans->begin(); it != bans->end(); ++it)
		{
			if (CheckBan(user, it->mask))
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

		WriteAllExcept(src, false, 0, except_list, "KICK %s %s :%s", name.c_str(), user->nick.c_str(), reason.c_str());

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
	if (!user)
		return;

	const std::string message = ":" + user->GetFullHost() + " " + text;

	for (UserMembIter i = userlist.begin(); i != userlist.end(); i++)
	{
		if (IS_LOCAL(i->first))
			i->first->Write(message);
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
	const std::string message = ":" + (ServName.empty() ? ServerInstance->Config->ServerName : ServName) + " " + text;

	for (UserMembIter i = userlist.begin(); i != userlist.end(); i++)
	{
		if (IS_LOCAL(i->first))
			i->first->Write(message);
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

	int offset = snprintf(textbuffer,MAXBUF,":%s ", serversource ? ServerInstance->Config->ServerName.c_str() : user->GetFullHost().c_str());

	va_start(argsPtr, text);
	vsnprintf(textbuffer + offset, MAXBUF - offset, text, argsPtr);
	va_end(argsPtr);

	this->RawWriteAllExcept(user, serversource, status, except_list, std::string(textbuffer));
}

void Channel::WriteAllExcept(User* user, bool serversource, char status, CUList &except_list, const std::string &text)
{
	const std::string message = ":" + (serversource ? ServerInstance->Config->ServerName : user->GetFullHost()) + " " + text;
	this->RawWriteAllExcept(user, serversource, status, except_list, message);
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
		if (!i->first->quitting && !i->first->IsModeSet('i'))
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
		if (i->first->quitting)
			continue;
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

void Invitation::Create(Channel* c, LocalUser* u, time_t timeout)
{
	if ((timeout != 0) && (ServerInstance->Time() >= timeout))
		// Expired, don't bother
		return;

	ServerInstance->Logs->Log("INVITATION", LOG_DEBUG, "Invitation::Create chan=%s user=%s", c->name.c_str(), u->uuid.c_str());

	Invitation* inv = Invitation::Find(c, u, false);
	if (inv)
	{
		 if ((inv->expiry == 0) || (inv->expiry > timeout))
			return;
		inv->expiry = timeout;
		ServerInstance->Logs->Log("INVITATION", LOG_DEBUG, "Invitation::Create changed expiry in existing invitation %p", (void*) inv);
	}
	else
	{
		inv = new Invitation(c, u, timeout);
		c->invites.push_back(inv);
		u->invites.push_back(inv);
		ServerInstance->Logs->Log("INVITATION", LOG_DEBUG, "Invitation::Create created new invitation %p", (void*) inv);
	}
}

Invitation* Invitation::Find(Channel* c, LocalUser* u, bool check_expired)
{
	ServerInstance->Logs->Log("INVITATION", LOG_DEBUG, "Invitation::Find chan=%s user=%s check_expired=%d", c ? c->name.c_str() : "NULL", u ? u->uuid.c_str() : "NULL", check_expired);
	if (!u || u->invites.empty())
		return NULL;

	InviteList locallist;
	locallist.swap(u->invites);

	Invitation* result = NULL;
	for (InviteList::iterator i = locallist.begin(); i != locallist.end(); )
	{
		Invitation* inv = *i;
		if ((check_expired) && (inv->expiry != 0) && (inv->expiry <= ServerInstance->Time()))
		{
			/* Expired invite, remove it. */
			std::string expiration = ServerInstance->TimeString(inv->expiry);
			ServerInstance->Logs->Log("INVITATION", LOG_DEBUG, "Invitation::Find ecountered expired entry: %p expired %s", (void*) inv, expiration.c_str());
			i = locallist.erase(i);
			inv->cull();
			delete inv;
		}
		else
		{
			/* Is it what we're searching for? */
			if (inv->chan == c)
			{
				result = inv;
				break;
			}
			++i;
		}
	}

	locallist.swap(u->invites);
	ServerInstance->Logs->Log("INVITATION", LOG_DEBUG, "Invitation::Find result=%p", (void*) result);
	return result;
}

Invitation::~Invitation()
{
	// Remove this entry from both lists
	InviteList::iterator it = std::find(chan->invites.begin(), chan->invites.end(), this);
	if (it != chan->invites.end())
		chan->invites.erase(it);
	it = std::find(user->invites.begin(), user->invites.end(), this);
	if (it != user->invites.end())
		user->invites.erase(it);
}

void InviteBase::ClearInvites()
{
	ServerInstance->Logs->Log("INVITEBASE", LOG_DEBUG, "InviteBase::ClearInvites %p", (void*) this);
	InviteList locallist;
	locallist.swap(invites);
	for (InviteList::const_iterator i = locallist.begin(); i != locallist.end(); ++i)
	{
		(*i)->cull();
		delete *i;
	}
}
