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
#include <stdarg.h>
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "wildcard.h"
#include "mode.h"

chanrec::chanrec(InspIRCd* Instance) : ServerInstance(Instance)
{
	*name = *topic = *setby = *key = 0;
	maxbans = created = topicset = limit = 0;
	memset(&modes,0,64);
	age = ServerInstance->Time(true);
}

void chanrec::SetMode(char mode,bool mode_on)
{
	modes[mode-65] = mode_on;
	if (!mode_on)
		this->SetModeParam(mode,"",false);
}


void chanrec::SetModeParam(char mode,const char* parameter,bool mode_on)
{
	CustomModeList::iterator n = custom_mode_params.find(mode);	

	if (mode_on)
	{
		if (n == custom_mode_params.end())
			custom_mode_params[mode] = strdup(parameter);
	}
	else
	{
		if (n != custom_mode_params.end())
		{
			free(n->second);
			custom_mode_params.erase(n);
		}
	}
}

bool chanrec::IsModeSet(char mode)
{
	return modes[mode-65];
}

std::string chanrec::GetModeParameter(char mode)
{
	switch (mode)
	{
		case 'k':
			return this->key;
		case 'l':
			return ConvToStr(this->limit);
		default:
			CustomModeList::iterator n = custom_mode_params.find(mode);
			if (n != custom_mode_params.end())
				return n->second;
			return "";
		break;
	}
}

long chanrec::GetUserCounter()
{
	return (this->internal_userlist.size());
}

void chanrec::AddUser(userrec* user)
{
	internal_userlist[user] = user->nick;
}

unsigned long chanrec::DelUser(userrec* user)
{
	CUListIter a = internal_userlist.find(user);
	
	if (a != internal_userlist.end())
	{
		internal_userlist.erase(a);
		/* And tidy any others... */
		DelOppedUser(user);
		DelHalfoppedUser(user);
		DelVoicedUser(user);
	}
	
	return internal_userlist.size();
}

bool chanrec::HasUser(userrec* user)
{
	return (internal_userlist.find(user) != internal_userlist.end());
}

void chanrec::AddOppedUser(userrec* user)
{
	internal_op_userlist[user] = user->nick;
}

void chanrec::DelOppedUser(userrec* user)
{
	CUListIter a = internal_op_userlist.find(user);
	if (a != internal_op_userlist.end())
	{
		internal_op_userlist.erase(a);
		return;
	}
}

void chanrec::AddHalfoppedUser(userrec* user)
{
	internal_halfop_userlist[user] = user->nick;
}

void chanrec::DelHalfoppedUser(userrec* user)
{
	CUListIter a = internal_halfop_userlist.find(user);

	if (a != internal_halfop_userlist.end())
	{   
		internal_halfop_userlist.erase(a);
	}
}

void chanrec::AddVoicedUser(userrec* user)
{
	internal_voice_userlist[user] = user->nick;
}

void chanrec::DelVoicedUser(userrec* user)
{
	CUListIter a = internal_voice_userlist.find(user);
	
	if (a != internal_voice_userlist.end())
	{
		internal_voice_userlist.erase(a);
	}
}

CUList* chanrec::GetUsers()
{
	return &internal_userlist;
}

CUList* chanrec::GetOppedUsers()
{
	return &internal_op_userlist;
}

CUList* chanrec::GetHalfoppedUsers()
{
	return &internal_halfop_userlist;
}

CUList* chanrec::GetVoicedUsers()
{
	return &internal_voice_userlist;
}

void chanrec::SetDefaultModes()
{
	irc::spacesepstream list(ServerInstance->Config->DefaultModes);
	std::string modeseq = list.GetToken();
	std::string parameter;
	userrec* dummyuser = new userrec(ServerInstance);
	dummyuser->SetFd(FD_MAGIC_NUMBER);

	for (std::string::iterator n = modeseq.begin(); n != modeseq.end(); ++n)
	{
		ModeHandler* mode = ServerInstance->Modes->FindMode(*n, MODETYPE_CHANNEL);
		if (mode)
		{
			if (mode->GetNumParams(true))
				parameter = list.GetToken().c_str();
			else
				parameter.clear();

			mode->OnModeChange(dummyuser, dummyuser, this, parameter, true);
		}
	}

	delete dummyuser;
}

/* 
 * add a channel to a user, creating the record for it if needed and linking
 * it to the user record 
 */
chanrec* chanrec::JoinUser(InspIRCd* Instance, userrec *user, const char* cn, bool override, const char* key, time_t TS)
{
	if (!user || !cn)
		return NULL;

	bool new_channel = false;
	char cname[MAXBUF];
	int MOD_RESULT = 0;
	strlcpy(cname,cn,CHANMAX);

	std::string privs;

	chanrec* Ptr = Instance->FindChan(cname);

	if (!Ptr)
	{
		/*
		 * Fix: desync bug was here, don't set @ on remote users - spanningtree handles their permissions. bug #358. -- w00t
		 */
		if (!IS_LOCAL(user))
		{
			if (!TS)
				Instance->Log(DEBUG,"*** BUG *** chanrec::JoinUser called for REMOTE user '%s' on channel '%s' but no TS given!", user->nick, cn);
		}
		else
		{
			privs = "@";
		}

		if (IS_LOCAL(user) && override == false)
		{
			MOD_RESULT = 0;
			FOREACH_RESULT_I(Instance,I_OnUserPreJoin,OnUserPreJoin(user,NULL,cname,privs));
			if (MOD_RESULT == 1)
				return NULL;
		}

		/* create a new one */
		Ptr = new chanrec(Instance);
		(*(Instance->chanlist))[cname] = Ptr;

		strlcpy(Ptr->name, cname,CHANMAX);

		/* As spotted by jilles, dont bother to set this on remote users */
		if (IS_LOCAL(user))
			Ptr->SetDefaultModes();

		Ptr->created = TS ? TS : Instance->Time();
		Ptr->age = Ptr->created;
		*Ptr->topic = 0;
		*Ptr->setby = 0;
		Ptr->topicset = 0;
		new_channel = true;
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
			MOD_RESULT = 0;
			FOREACH_RESULT_I(Instance,I_OnUserPreJoin,OnUserPreJoin(user,Ptr,cname,privs));
			if (MOD_RESULT == 1)
			{
				return NULL;
			}
			else if (MOD_RESULT == 0)
			{
				if (*Ptr->key)
				{
					MOD_RESULT = 0;
					FOREACH_RESULT_I(Instance,I_OnCheckKey,OnCheckKey(user, Ptr, key ? key : ""));
					if (!MOD_RESULT)
					{
						if ((!key) || strcmp(key,Ptr->key))
						{
							user->WriteServ("475 %s %s :Cannot join channel (Incorrect channel key)",user->nick, Ptr->name);
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
						if (!user->IsInvited(Ptr->name))
						{
							user->WriteServ("473 %s %s :Cannot join channel (Invite only)",user->nick, Ptr->name);
							return NULL;
						}
					}
					user->RemoveInvite(Ptr->name);
				}
				if (Ptr->limit)
				{
					MOD_RESULT = 0;
					FOREACH_RESULT_I(Instance,I_OnCheckLimit,OnCheckLimit(user, Ptr));
					if (!MOD_RESULT)
					{
						if (Ptr->GetUserCounter() >= Ptr->limit)
						{
							user->WriteServ("471 %s %s :Cannot join channel (Channel is full)",user->nick, Ptr->name);
							return NULL;
						}
					}
				}
				if (Ptr->bans.size())
				{
					if (Ptr->IsBanned(user))
					{
						user->WriteServ("474 %s %s :Cannot join channel (You're banned)",user->nick, Ptr->name);
						return NULL;
					}
				}
			}
		}
	}

	/* NOTE: If the user is an oper here, we can extend their user->chans by up to
	 * OperMaxchans. For remote users which are not bound by the channel limits,
	 * we can extend infinitely. Otherwise, nope, youre restricted to MaxChans.
	 */



	/*
	 * We place no restrictions on remote users, users that are override-joining, or users that are
	 * currently in under MaxChans channels. For all others, they won't get in here. -- w00t
	 */	
	if (!IS_LOCAL(user) || override == true || user->chans.size() < Instance->Config->MaxChans)
	{
		return chanrec::ForceChan(Instance, Ptr, user, privs);
	}

	/*
	 * If the above fails, and the user is an oper -- we let them in if they are under OperMaxChans.
	 * Otherwise, they're stuck, and need to override to get in, etc. -- w00t
	 */
	if (IS_OPER(user))
	{
		if (user->chans.size() < Instance->Config->OperMaxChans)
		{
			return chanrec::ForceChan(Instance, Ptr, user, privs);
		}
	}

	user->WriteServ("405 %s %s :You are on too many channels",user->nick, cname);

	if (new_channel)
	{
		/* Things went seriously pear shaped, so take this away. bwahaha. */
		chan_hash::iterator n = Instance->chanlist->find(cname);
		if (n != Instance->chanlist->end())
		{
			Ptr->DelUser(user);
			DELETE(Ptr);
			Instance->chanlist->erase(n);
		}
	}

	return NULL;
}

chanrec* chanrec::ForceChan(InspIRCd* Instance, chanrec* Ptr, userrec* user, const std::string &privs)
{
	userrec* dummyuser = new userrec(Instance);
	std::string nick = user->nick;
	bool silent = false;

	dummyuser->SetFd(FD_MAGIC_NUMBER);
	Ptr->AddUser(user);

	/* Just in case they have no permissions */
	user->chans[Ptr] = 0;

	for (std::string::const_iterator x = privs.begin(); x != privs.end(); x++)
	{
		const char status = *x;
		ModeHandler* mh = Instance->Modes->FindPrefix(status);
		if (mh)
		{
			Ptr->SetPrefix(user, status, mh->GetPrefixRank(), true);
			/* Make sure that the mode handler knows this mode was now set */
			mh->OnModeChange(dummyuser, dummyuser, Ptr, nick, true);

			switch (mh->GetPrefix())
			{
				/* These logic ops are SAFE IN THIS CASE
				 * because if the entry doesnt exist,
				 * addressing operator[] creates it.
				 * If they do exist, it points to it.
				 * At all other times where we dont want
				 * to create an item if it doesnt exist, we
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

	delete dummyuser;

	FOREACH_MOD_I(Instance,I_OnUserJoin,OnUserJoin(user, Ptr, silent));

	if (!silent)
		Ptr->WriteChannel(user,"JOIN :%s",Ptr->name);

	/* Theyre not the first ones in here, make sure everyone else sees the modes we gave the user */
	std::string ms = Instance->Modes->ModeString(user, Ptr);
	if ((Ptr->GetUserCounter() > 1) && (ms.length()))
		Ptr->WriteAllExceptSender(user, true, 0, "MODE %s +%s", Ptr->name, ms.c_str());

	/* Major improvement by Brain - we dont need to be calculating all this pointlessly for remote users */
	if (IS_LOCAL(user))
	{
		if (Ptr->topicset)
		{
			user->WriteServ("332 %s %s :%s", user->nick, Ptr->name, Ptr->topic);
			user->WriteServ("333 %s %s %s %lu", user->nick, Ptr->name, Ptr->setby, (unsigned long)Ptr->topicset);
		}
		Ptr->UserList(user);
	}
	FOREACH_MOD_I(Instance,I_OnPostJoin,OnPostJoin(user, Ptr));
	return Ptr;
}

bool chanrec::IsBanned(userrec* user)
{
	char mask[MAXBUF];
	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnCheckBan,OnCheckBan(user, this));
	if (!MOD_RESULT)
	{
		snprintf(mask, MAXBUF, "%s!%s@%s", user->nick, user->ident, user->GetIPString());
		for (BanList::iterator i = this->bans.begin(); i != this->bans.end(); i++)
		{
			/* This allows CIDR ban matching
			 * 
			 *        Full masked host                      Full unmasked host                   IP with/without CIDR
			 */
			if ((match(user->GetFullHost(),i->data)) || (match(user->GetFullRealHost(),i->data)) || (match(mask, i->data, true)))
			{
				return true;
			}
		}
	}
	return false;
}

/* chanrec::PartUser
 * remove a channel from a users record, and return the number of users left.
 * Therefore, if this function returns 0 the caller should delete the chanrec.
 */
long chanrec::PartUser(userrec *user, const char* reason)
{
	bool silent = false;

	if (!user)
		return this->GetUserCounter();

	UCListIter i = user->chans.find(this);
	if (i != user->chans.end())
	{
		FOREACH_MOD(I_OnUserPart,OnUserPart(user, this, reason ? reason : "", silent));

		if (!silent)
			this->WriteChannel(user, "PART %s%s%s", this->name, reason ? " :" : "", reason ? reason : "");

		user->chans.erase(i);
		this->RemoveAllPrefixes(user);
	}

	if (!this->DelUser(user)) /* if there are no users left on the channel... */
	{
		chan_hash::iterator iter = ServerInstance->chanlist->find(this->name);
		/* kill the record */
		if (iter != ServerInstance->chanlist->end())
		{
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(this));
			ServerInstance->chanlist->erase(iter);
		}
		return 0;
	}

	return this->GetUserCounter();
}

long chanrec::ServerKickUser(userrec* user, const char* reason, bool triggerevents)
{
	bool silent = false;

	if (!user || !reason)
		return this->GetUserCounter();

	if (IS_LOCAL(user))
	{
		if (!this->HasUser(user))
		{
			/* Not on channel */
			return this->GetUserCounter();
		}
	}

	if (triggerevents)
	{
		FOREACH_MOD(I_OnUserKick,OnUserKick(NULL, user, this, reason, silent));
	}

	UCListIter i = user->chans.find(this);
	if (i != user->chans.end())
	{
		if (!silent)
			this->WriteChannelWithServ(ServerInstance->Config->ServerName, "KICK %s %s :%s", this->name, user->nick, reason);

		user->chans.erase(i);
		this->RemoveAllPrefixes(user);
	}

	if (!this->DelUser(user))
	{
		chan_hash::iterator iter = ServerInstance->chanlist->find(this->name);
		/* kill the record */
		if (iter != ServerInstance->chanlist->end())
		{
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(this));
			ServerInstance->chanlist->erase(iter);
		}
		return 0;
	}

	return this->GetUserCounter();
}

long chanrec::KickUser(userrec *src, userrec *user, const char* reason)
{
	bool silent = false;

	if (!src || !user || !reason)
		return this->GetUserCounter();

	if (IS_LOCAL(src))
	{
		if (!this->HasUser(user))
		{
			src->WriteServ("441 %s %s %s :They are not on that channel",src->nick, user->nick, this->name);
			return this->GetUserCounter();
		}
		if ((ServerInstance->ULine(user->server)) && (!ServerInstance->ULine(src->server)))
		{
			src->WriteServ("482 %s %s :Only a u-line may kick a u-line from a channel.",src->nick, this->name);
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
					src->WriteServ("482 %s %s :You must be a channel %soperator",src->nick, this->name, them == STATUS_HOP ? "" : "half-");
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
			this->WriteChannel(src, "KICK %s %s :%s", this->name, user->nick, reason);

		user->chans.erase(i);
		this->RemoveAllPrefixes(user);
	}

	if (!this->DelUser(user))
	/* if there are no users left on the channel */
	{
		chan_hash::iterator iter = ServerInstance->chanlist->find(this->name);

		/* kill the record */
		if (iter != ServerInstance->chanlist->end())
		{
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(this));
			ServerInstance->chanlist->erase(iter);
		}
		return 0;
	}

	return this->GetUserCounter();
}

void chanrec::WriteChannel(userrec* user, char* text, ...)
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

void chanrec::WriteChannel(userrec* user, const std::string &text)
{
	CUList *ulist = this->GetUsers();
	char tb[MAXBUF];

	if (!user)
		return;

	snprintf(tb,MAXBUF,":%s %s",user->GetFullHost(),text.c_str());
	std::string out = tb;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (IS_LOCAL(i->first))
			i->first->Write(out);
	}
}

void chanrec::WriteChannelWithServ(const char* ServName, const char* text, ...)
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

void chanrec::WriteChannelWithServ(const char* ServName, const std::string &text)
{
	CUList *ulist = this->GetUsers();
	char tb[MAXBUF];

	snprintf(tb,MAXBUF,":%s %s",ServName ? ServName : ServerInstance->Config->ServerName, text.c_str());
	std::string out = tb;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (IS_LOCAL(i->first))
			i->first->Write(out);
	}
}

/* write formatted text from a source user to all users on a channel except
 * for the sender (for privmsg etc) */
void chanrec::WriteAllExceptSender(userrec* user, bool serversource, char status, char* text, ...)
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

void chanrec::WriteAllExcept(userrec* user, bool serversource, char status, CUList &except_list, char* text, ...)
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

void chanrec::WriteAllExcept(userrec* user, bool serversource, char status, CUList &except_list, const std::string &text)
{
	CUList *ulist;
	char tb[MAXBUF];

	switch (status)
	{
		case '@':
			ulist = this->GetOppedUsers();
			break;
		case '%':
			ulist = this->GetHalfoppedUsers();
			break;
		case '+':
			ulist = this->GetVoicedUsers();
			break;
		default:
			ulist = this->GetUsers();
			break;
	}

	snprintf(tb,MAXBUF,":%s %s",user->GetFullHost(),text.c_str());
	std::string out = tb;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((IS_LOCAL(i->first)) && (except_list.find(i->first) == except_list.end()))
		{
			if (serversource)
				i->first->WriteServ(text);
			else
				i->first->Write(out);
		}
	}
}

void chanrec::WriteAllExceptSender(userrec* user, bool serversource, char status, const std::string& text)
{
	CUList except_list;
	except_list[user] = user->nick;
	this->WriteAllExcept(user, serversource, status, except_list, std::string(text));
}

/*
 * return a count of the users on a specific channel accounting for
 * invisible users who won't increase the count. e.g. for /LIST
 */
int chanrec::CountInvisible()
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

char* chanrec::ChanModes(bool showkey)
{
	static char scratch[MAXBUF];
	static char sparam[MAXBUF];
	char* offset = scratch;
	std::string extparam;

	*scratch = '\0';
	*sparam = '\0';

	/* This was still iterating up to 190, chanrec::modes is only 64 elements -- Om */
	for(int n = 0; n < 64; n++)
	{
		if(this->modes[n])
		{
			*offset++ = n + 65;
			extparam.clear();
			switch (n)
			{
				case CM_KEY:
					extparam = (showkey ? this->key : "<key>");
				break;
				case CM_LIMIT:
					extparam = ConvToStr(this->limit);
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
void chanrec::UserList(userrec *user, CUList *ulist)
{
	char list[MAXBUF];
	size_t dlen, curlen;
	int MOD_RESULT = 0;

	if (!IS_LOCAL(user))
		return;

	FOREACH_RESULT(I_OnUserList,OnUserList(user, this, ulist));
	if (MOD_RESULT == 1)
		return;

	dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, this->name);

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

		size_t ptrlen = snprintf(ptr, MAXBUF, "%s%s ", this->GetPrefixChar(i->first), i->second.c_str());
		/* OnUserList can change this - reset it back to normal */
		i->second = i->first->nick;

		curlen += ptrlen;
		ptr += ptrlen;

		numusers++;

		if (curlen > (480-NICKMAX))
		{
			/* list overflowed into multiple numerics */
			user->WriteServ(std::string(list));

			/* reset our lengths */
			dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, this->name);
			ptr = list + dlen;

			ptrlen = 0;
			numusers = 0;
		}
	}

	/* if whats left in the list isnt empty, send it */
	if (numusers)
	{
		user->WriteServ(std::string(list));
	}

	user->WriteServ("366 %s %s :End of /NAMES list.", user->nick, this->name);
}

long chanrec::GetMaxBans()
{
	/* Return the cached value if there is one */
	if (this->maxbans)
		return this->maxbans;

	/* If there isnt one, we have to do some O(n) hax to find it the first time. (ick) */
	for (std::map<std::string,int>::iterator n = ServerInstance->Config->maxbans.begin(); n != ServerInstance->Config->maxbans.end(); n++)
	{
		if (match(this->name,n->first.c_str()))
		{
			this->maxbans = n->second;
			return n->second;
		}
	}

	/* Screw it, just return the default of 64 */
	this->maxbans = 64;
	return this->maxbans;
}

void chanrec::ResetMaxBans()
{
	this->maxbans = 0;
}

/* returns the status character for a given user on a channel, e.g. @ for op,
 * % for halfop etc. If the user has several modes set, the highest mode
 * the user has must be returned.
 */
const char* chanrec::GetPrefixChar(userrec *user)
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

const char* chanrec::GetAllPrefixChars(userrec* user)
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

unsigned int chanrec::GetPrefixValue(userrec* user)
{
	prefixlist::iterator n = prefixes.find(user);
	if (n != prefixes.end())
	{
		if (n->second.size())
			return n->second.begin()->second;
	}
	return 0;
}

int chanrec::GetStatusFlags(userrec *user)
{
	UCListIter i = user->chans.find(this);
	if (i != user->chans.end())
	{
		return i->second;
	}
	return 0;
}

int chanrec::GetStatus(userrec *user)
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

void chanrec::SetPrefix(userrec* user, char prefix, unsigned int prefix_value, bool adding)
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
			prefixes.insert(std::make_pair<userrec*,pfxcontainer>(user, one));
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

void chanrec::RemoveAllPrefixes(userrec* user)
{
	prefixlist::iterator n = prefixes.find(user);
	if (n != prefixes.end())
	{
		prefixes.erase(n);
	}
}

