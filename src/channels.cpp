/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *   E-mail:
 *   	  <brain@chatspike.net>
 *   	  <Craig@chatspike.net>
 * 
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdarg.h>
#include "configreader.h"
#include "inspircd.h"
#include "users.h"
#include "modules.h"
#include "wildcard.h"
#include "mode.h"

chanrec::chanrec(InspIRCd* Instance) : ServerInstance(Instance)
{
	*name = *topic = *setby = *key = 0;
	created = topicset = limit = 0;
	internal_userlist.clear();
	memset(&modes,0,64);
}

void chanrec::SetMode(char mode,bool mode_on)
{
	modes[mode-65] = mode_on;
	if (!mode_on)
		this->SetModeParam(mode,"",false);
}


void chanrec::SetModeParam(char mode,const char* parameter,bool mode_on)
{
	ServerInstance->Log(DEBUG,"SetModeParam called");
	
	CustomModeList::iterator n = custom_mode_params.find(mode);	

	if (mode_on)
	{
		if (n == custom_mode_params.end())
		{
			custom_mode_params[mode] = strdup(parameter);
			ServerInstance->Log(DEBUG,"Custom mode parameter %c %s added",mode,parameter);
		}
		else
		{
			ServerInstance->Log(DEBUG, "Tried to set custom mode parameter for %c '%s' when it was already '%s'", mode, parameter, n->second);
		}
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
	if (mode == 'k')
	{
		return this->key;
	}
	else if (mode == 'l')
	{
		return ConvToStr(this->limit);
	}
	else
	{
		CustomModeList::iterator n = custom_mode_params.find(mode);
		if (n != custom_mode_params.end())
		{
			return n->second;
		}
		return "";
	}
}

long chanrec::GetUserCounter()
{
	return (this->internal_userlist.size());
}

void chanrec::AddUser(userrec* user)
{
	internal_userlist[user] = user;
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
	internal_op_userlist[user] = user;
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
	internal_halfop_userlist[user] = user;
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
	internal_voice_userlist[user] = user;
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

/* 
 * add a channel to a user, creating the record for it if needed and linking
 * it to the user record 
 */
chanrec* chanrec::JoinUser(InspIRCd* Instance, userrec *user, const char* cn, bool override, const char* key)
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
		if (IS_LOCAL(user))
		{
			privs = "@";
			MOD_RESULT = 0;
			FOREACH_RESULT_I(Instance,I_OnUserPreJoin,OnUserPreJoin(user,NULL,cname,privs));
			if (MOD_RESULT == 1)
				return NULL;
		}

		/* create a new one */
		Ptr = new chanrec(Instance);
		Instance->chanlist[cname] = Ptr;

		strlcpy(Ptr->name, cname,CHANMAX);
		Ptr->modes[CM_TOPICLOCK] = Ptr->modes[CM_NOEXTERNAL] = 1;
		Ptr->created = Instance->Time();
		*Ptr->topic = 0;
		*Ptr->setby = 0;
		Ptr->topicset = 0;
		Instance->Log(DEBUG,"chanrec::JoinUser(): created: %s",cname);
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
		if (IS_LOCAL(user)) /* was a check on fd > -1 */
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
				if (Ptr->modes[CM_INVITEONLY])
				{
					MOD_RESULT = 0;
					irc::string xname(Ptr->name);
					FOREACH_RESULT_I(Instance,I_OnCheckInvite,OnCheckInvite(user, Ptr));
					if (!MOD_RESULT)
					{
						if (user->IsInvited(xname))
						{
							/* user was invited to channel */
							/* there may be an optional channel NOTICE here */
						}
						else
						{
							user->WriteServ("473 %s %s :Cannot join channel (Invite only)",user->nick, Ptr->name);
							return NULL;
						}
					}
					user->RemoveInvite(xname);
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
					MOD_RESULT = 0;
					FOREACH_RESULT_I(Instance,I_OnCheckBan,OnCheckBan(user, Ptr));
					char mask[MAXBUF];
					sprintf(mask,"%s!%s@%s",user->nick, user->ident, user->GetIPString());
					if (!MOD_RESULT)
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
		else
		{
			Instance->Log(DEBUG,"chanrec::JoinUser(): Overridden checks");
		}
	}

	for (UserChanList::const_iterator index = user->chans.begin(); index != user->chans.end(); index++)
	{
		if ((*index)->channel == NULL)
		{
			return chanrec::ForceChan(Instance, Ptr, *index, user, privs);
		}
	}

	/*
	 * XXX: If the user is an oper here, we can just extend their user->chans vector by one
	 * and put the channel in here. Same for remote users which are not bound by
	 * the channel limits. Otherwise, nope, youre boned.
	 */
	if (!IS_LOCAL(user)) /* was a check on fd < 0 */
	{
		ucrec* a = new ucrec();
		chanrec* c = chanrec::ForceChan(Instance, Ptr, a, user, privs);
		user->chans.push_back(a);
		return c;
	}
	else if (*user->oper)
	{
		/* Oper allows extension up to the OPERMAXCHANS value */
		if (user->chans.size() < OPERMAXCHANS)
		{
			ucrec* a = new ucrec();
			chanrec* c = chanrec::ForceChan(Instance, Ptr, a, user, privs);
			user->chans.push_back(a);
			return c;
		}
	}

	user->WriteServ("405 %s %s :You are on too many channels",user->nick, cname);

	if (new_channel)
	{
		Instance->Log(DEBUG,"BLAMMO, Whacking channel.");
		/* Things went seriously pear shaped, so take this away. bwahaha. */
		chan_hash::iterator n = Instance->chanlist.find(cname);
		if (n != Instance->chanlist.end())
		{
			Ptr->DelUser(user);
			DELETE(Ptr);
			Instance->chanlist.erase(n);
			for (unsigned int index =0; index < user->chans.size(); index++)
			{
				if (user->chans[index]->channel == Ptr)
				{
					user->chans[index]->channel = NULL;
					user->chans[index]->uc_modes = 0;	
				}
			}
		}
	}
	else
	{
		for (unsigned int index =0; index < user->chans.size(); index++)
		{
			if (user->chans[index]->channel == Ptr)
			{
				user->chans[index]->channel = NULL;
				user->chans[index]->uc_modes = 0;
			}
		}
	}
	return NULL;
}

chanrec* chanrec::ForceChan(InspIRCd* Instance, chanrec* Ptr,ucrec *a,userrec* user, const std::string &privs)
{
	a->uc_modes = 0;

	for (std::string::const_iterator x = privs.begin(); x != privs.end(); x++)
	{
		const char status = *x;
		switch (status)
		{
			case '@':
				a->uc_modes = UCMODE_OP;
			break;
			case '%':
				a->uc_modes = UCMODE_HOP;
			break;
			case '+':
				a->uc_modes = UCMODE_VOICE;
			break;
		}
		ModeHandler* mh = Instance->Modes->FindPrefix(status);
		if (mh)
		{
			Ptr->SetPrefix(user, status, mh->GetPrefixRank(), true);
		}
	}

	a->channel = Ptr;
	Ptr->AddUser(user);
	user->ModChannelCount(1);
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
	FOREACH_MOD_I(Instance,I_OnUserJoin,OnUserJoin(user,Ptr));
	return Ptr;
}

bool chanrec::IsBanned(userrec* user)
{
	char mask[MAXBUF];
	sprintf(mask,"%s!%s@%s",user->nick, user->ident, user->GetIPString());
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
	return false;
}

/* chanrec::PartUser
 * remove a channel from a users record, and remove the record from the hash
 * if the channel has become empty
 */
long chanrec::PartUser(userrec *user, const char* reason)
{
	if (!user)
		return this->GetUserCounter();

	for (unsigned int i =0; i < user->chans.size(); i++)
	{
		/* zap it from the channel list of the user */
		if (user->chans[i]->channel == this)
		{
			if (reason)
			{
				FOREACH_MOD(I_OnUserPart,OnUserPart(user, this, reason));
				this->WriteChannel(user, "PART %s :%s", this->name, reason);
			}
			else
			{
				FOREACH_MOD(I_OnUserPart,OnUserPart(user, this, ""));
				this->WriteChannel(user, "PART :%s", this->name);
			}
			user->chans[i]->uc_modes = 0;
			user->chans[i]->channel = NULL;
			user->ModChannelCount(-1);
			this->RemoveAllPrefixes(user);
			break;
		}
	}

	if (!this->DelUser(user)) /* if there are no users left on the channel... */
	{
		chan_hash::iterator iter = ServerInstance->chanlist.find(this->name);
		/* kill the record */
		if (iter != ServerInstance->chanlist.end())
		{
			ServerInstance->Log(DEBUG,"del_channel: destroyed: %s", this->name);
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(this));
			ServerInstance->chanlist.erase(iter);
		}
		return 0;
	}

	return this->GetUserCounter();
}

long chanrec::ServerKickUser(userrec* user, const char* reason, bool triggerevents)
{
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
		FOREACH_MOD(I_OnUserKick,OnUserKick(NULL,user,this,reason));
	}

	for (unsigned int i =0; i < user->chans.size(); i++)
	{
		if (user->chans[i]->channel == this)
		{
			this->WriteChannelWithServ(ServerInstance->Config->ServerName, "KICK %s %s :%s", this->name, user->nick, reason);
			user->chans[i]->uc_modes = 0;
			user->chans[i]->channel = NULL;
			this->RemoveAllPrefixes(user);
			break;
		}
	}

	if (!this->DelUser(user))
	{
		chan_hash::iterator iter = ServerInstance->chanlist.find(this->name);
		/* kill the record */
		if (iter != ServerInstance->chanlist.end())
		{
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(this));
			ServerInstance->chanlist.erase(iter);
		}
		return 0;
	}

	return this->GetUserCounter();
}

long chanrec::KickUser(userrec *src, userrec *user, const char* reason)
{
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
					if (them == STATUS_HOP)
					{
						src->WriteServ("482 %s %s :You must be a channel operator",src->nick, this->name);
					}
					else
					{
						src->WriteServ("482 %s %s :You must be at least a half-operator",src->nick, this->name);
					}
					return this->GetUserCounter();
				}
			}
		}
	}

	FOREACH_MOD(I_OnUserKick,OnUserKick(src,user,this,reason));
			
	for (UserChanList::const_iterator i = user->chans.begin(); i != user->chans.end(); i++)
	{
		/* zap it from the channel list of the user */
		if ((*i)->channel == this)
		{
			this->WriteChannel(src, "KICK %s %s :%s", this->name, user->nick, reason);
			(*i)->uc_modes = 0;
			(*i)->channel = NULL;
			this->RemoveAllPrefixes(user);
			break;
		}
	}

	if (!this->DelUser(user))
	/* if there are no users left on the channel */
	{
		chan_hash::iterator iter = ServerInstance->chanlist.find(this->name);

		/* kill the record */
		if (iter != ServerInstance->chanlist.end())
		{
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(this));
			ServerInstance->chanlist.erase(iter);
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

	if (!user)
		return;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (IS_LOCAL(i->second))
			user->WriteTo(i->second,text);
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

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (IS_LOCAL(i->second))
			i->second->WriteServ(text);
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

void chanrec::WriteAllExceptSender(userrec* user, bool serversource, char status, const std::string& text)
{
	CUList *ulist;

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

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((IS_LOCAL(i->second)) && (user != i->second))
		{
			if (serversource)
				i->second->WriteServ(text);
			else
				i->second->WriteFrom(user,text);
		}
	}
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
		if (!(i->second->modes[UM_INVISIBLE]))
			count++;
	}

	return count;
}

char* chanrec::ChanModes(bool showkey)
{
	static char scratch[MAXBUF];
	static char sparam[MAXBUF];
	char* offset = scratch;
	std::string extparam = "";

	*scratch = '\0';
	*sparam = '\0';

	/* This was still iterating up to 190, chanrec::custom_modes is only 64 elements -- Om */
	for(int n = 0; n < 64; n++)
	{
		if(this->modes[n])
		{
			*offset++ = n + 65;
			extparam = "";
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
			if (extparam != "")
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
void chanrec::UserList(userrec *user)
{
	char list[MAXBUF];
	size_t dlen, curlen;
	int MOD_RESULT = 0;

	FOREACH_RESULT(I_OnUserList,OnUserList(user, this));
	ServerInstance->Log(DEBUG,"MOD_RESULT for UserList = %d",MOD_RESULT);
	if (MOD_RESULT == 1)
		return;

	ServerInstance->Log(DEBUG,"Using builtin NAMES list generation");

	dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, this->name);

	int numusers = 0;
	char* ptr = list + dlen;

	CUList *ulist= this->GetUsers();

	/* Improvement by Brain - this doesnt change in value, so why was it inside
	 * the loop?
	 */
	bool has_user = this->HasUser(user);

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((!has_user) && (i->second->modes[UM_INVISIBLE]))
		{
			/*
			 * user is +i, and source not on the channel, does not show
			 * nick in NAMES list
			 */
			continue;
		}

		size_t ptrlen = snprintf(ptr, MAXBUF, "%s%s ", this->GetPrefixChar(i->second), i->second->nick);

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
	std::string x;
	for (std::map<std::string,int>::iterator n = ServerInstance->Config->maxbans.begin(); n != ServerInstance->Config->maxbans.end(); n++)
	{
		x = n->first;
		if (match(this->name,x.c_str()))
		{
			return n->second;
		}
	}
	return 64;
}


/* returns the status character for a given user on a channel, e.g. @ for op,
 * % for halfop etc. If the user has several modes set, the highest mode
 * the user has must be returned.
 */
const char* chanrec::GetPrefixChar(userrec *user)
{
	static char px[2];
	unsigned int mx = 0;

	*px = 0;
	*(px+1) = 0;

	prefixlist::iterator n = prefixes.find(user);
	if (n != prefixes.end())
	{
		for (std::vector<prefixtype>::iterator x = n->second.begin(); x != n->second.end(); x++)
		{
			if (x->second > mx)
			{
				*px = x->first;
				mx  = x->second;
			}
		}
	}

	return px;
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
	unsigned int mx = 0;

	prefixlist::iterator n = prefixes.find(user);
	if (n != prefixes.end())
	{
		for (std::vector<prefixtype>::iterator x = n->second.begin(); x != n->second.end(); x++)
		{
			if (x->second > mx)
				mx  = x->second;
		}
	}

	return mx;
}


int chanrec::GetStatusFlags(userrec *user)
{
	for (std::vector<ucrec*>::const_iterator i = user->chans.begin(); i != user->chans.end(); i++)
	{
		if ((*i)->channel == this)
		{
			return (*i)->uc_modes;
		}
	}
	return 0;
}


int chanrec::GetStatus(userrec *user)
{
	if (ServerInstance->ULine(user->server))
		return STATUS_OP;

	for (std::vector<ucrec*>::const_iterator i = user->chans.begin(); i != user->chans.end(); i++)
	{
		if ((*i)->channel == this)
		{
			if (((*i)->uc_modes & UCMODE_OP) > 0)
			{
				return STATUS_OP;
			}
			if (((*i)->uc_modes & UCMODE_HOP) > 0)
			{
				return STATUS_HOP;
			}
			if (((*i)->uc_modes & UCMODE_VOICE) > 0)
			{
				return STATUS_VOICE;
			}
			return STATUS_NORMAL;
		}
	}
	return STATUS_NORMAL;
}

/*bool ModeParser::PrefixComparison(const prefixtype one, const prefixtype two)
{       
        return one.second > two.second;
}*/

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
		prefixes.erase(n);
}

