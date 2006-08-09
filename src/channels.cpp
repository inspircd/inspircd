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

#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include <stdarg.h>
#include "configreader.h"
#include "inspircd.h"
#include "hash_map.h"
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "commands.h"
#include "wildcard.h"
#include "message.h"
#include "mode.h"
#include "xline.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "typedefs.h"

extern InspIRCd* ServerInstance;

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;

chanrec::chanrec()
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
	log(DEBUG,"SetModeParam called");
	
	CustomModeList::iterator n = custom_mode_params.find(mode);	

	if (mode_on)
	{
		if (n == custom_mode_params.end())
		{
			custom_mode_params[mode] = strdup(parameter);
			log(DEBUG,"Custom mode parameter %c %s added",mode,parameter);
		}
		else
		{
			log(DEBUG, "Tried to set custom mode parameter for %c '%s' when it was already '%s'", mode, parameter, n->second);
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
chanrec* chanrec::JoinUser(userrec *user, const char* cn, bool override, const char* key)
{
	if (!user || !cn)
		return NULL;

	int created = 0;
	char cname[MAXBUF];
	int MOD_RESULT = 0;
	strlcpy(cname,cn,CHANMAX);

	chanrec* Ptr = FindChan(cname);

	if (!Ptr)
	{
		if (user->fd > -1)
		{
			MOD_RESULT = 0;
			FOREACH_RESULT(I_OnUserPreJoin,OnUserPreJoin(user,NULL,cname));
			if (MOD_RESULT == 1)
				return NULL;
		}

		/* create a new one */
		Ptr = new chanrec();
		ServerInstance->chanlist[cname] = Ptr;

		strlcpy(Ptr->name, cname,CHANMAX);
		Ptr->modes[CM_TOPICLOCK] = Ptr->modes[CM_NOEXTERNAL] = 1;
		Ptr->created = TIME;
		*Ptr->topic = 0;
		strlcpy(Ptr->setby, user->nick,NICKMAX-1);
		Ptr->topicset = 0;
		log(DEBUG,"chanrec::JoinUser(): created: %s",cname);
		/*
		 * set created to 2 to indicate user
		 * is the first in the channel
		 * and should be given ops
		 */
		created = 2;
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
			FOREACH_RESULT(I_OnUserPreJoin,OnUserPreJoin(user,Ptr,cname));
			if (MOD_RESULT == 1)
			{
				return NULL;
			}
			else if (MOD_RESULT == 0)
			{
				if (*Ptr->key)
				{
					MOD_RESULT = 0;
					FOREACH_RESULT(I_OnCheckKey,OnCheckKey(user, Ptr, key ? key : ""));
					if (!MOD_RESULT)
					{
						if (!key)
						{
							log(DEBUG,"chanrec::JoinUser(): no key given in JOIN");
							user->WriteServ("475 %s %s :Cannot join channel (Requires key)",user->nick, Ptr->name);
							return NULL;
						}
						else
						{
							if (strcmp(key,Ptr->key))
							{
								log(DEBUG,"chanrec::JoinUser(): bad key given in JOIN");
								user->WriteServ("475 %s %s :Cannot join channel (Incorrect key)",user->nick, Ptr->name);
								return NULL;
							}
						}
					}
				}
				if (Ptr->modes[CM_INVITEONLY])
				{
					MOD_RESULT = 0;
					irc::string xname(Ptr->name);
					FOREACH_RESULT(I_OnCheckInvite,OnCheckInvite(user, Ptr));
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
					FOREACH_RESULT(I_OnCheckLimit,OnCheckLimit(user, Ptr));
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
					FOREACH_RESULT(I_OnCheckBan,OnCheckBan(user, Ptr));
					char mask[MAXBUF];
					sprintf(mask,"%s!%s@%s",user->nick, user->ident, user->GetIPString());
					if (!MOD_RESULT)
					{
						for (BanList::iterator i = Ptr->bans.begin(); i != Ptr->bans.end(); i++)
						{
							/* This allows CIDR ban matching
							 * 
							 *	  Full masked host			Full unmasked host		     IP with/without CIDR
							 */
							if ((match(user->GetFullHost(),i->data)) || (match(user->GetFullRealHost(),i->data)) || (match(mask, i->data, true)))
							{
								user->WriteServ("474 %s %s :Cannot join channel (You're banned)",user->nick, Ptr->name);
								return NULL;
							}
						}
					}
				}
			}
		}
		else
		{
			log(DEBUG,"chanrec::JoinUser(): Overridden checks");
		}
		created = 1;
	}

	for (UserChanList::const_iterator index = user->chans.begin(); index != user->chans.end(); index++)
	{
		if ((*index)->channel == NULL)
		{
			return chanrec::ForceChan(Ptr, *index, user, created);
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
		chanrec* c = chanrec::ForceChan(Ptr,a,user,created);
		user->chans.push_back(a);
		return c;
	}
	else if (*user->oper)
	{
		/* Oper allows extension up to the OPERMAXCHANS value */
		if (user->chans.size() < OPERMAXCHANS)
		{
			ucrec* a = new ucrec();
			chanrec* c = chanrec::ForceChan(Ptr,a,user,created);
			user->chans.push_back(a);
			return c;
		}
	}

	user->WriteServ("405 %s %s :You are on too many channels",user->nick, cname);

	if (created == 2)
	{
		log(DEBUG,"BLAMMO, Whacking channel.");
		/* Things went seriously pear shaped, so take this away. bwahaha. */
		chan_hash::iterator n = ServerInstance->chanlist.find(cname);
		if (n != ServerInstance->chanlist.end())
		{
			Ptr->DelUser(user);
			DELETE(Ptr);
			ServerInstance->chanlist.erase(n);
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

chanrec* chanrec::ForceChan(chanrec* Ptr,ucrec *a,userrec* user, int created)
{
	if (created == 2)
	{
		/* first user in is given ops */
		a->uc_modes = UCMODE_OP;
		Ptr->AddOppedUser(user);
	}
	else
	{
		a->uc_modes = 0;
	}

	a->channel = Ptr;
	Ptr->AddUser(user);
	Ptr->WriteChannel(user,"JOIN :%s",Ptr->name);

	/* Major improvement by Brain - we dont need to be calculating all this pointlessly for remote users */
	if (IS_LOCAL(user))
	{
		log(DEBUG,"Sent JOIN to client");
		if (Ptr->topicset)
		{
			user->WriteServ("332 %s %s :%s", user->nick, Ptr->name, Ptr->topic);
			user->WriteServ("333 %s %s %s %lu", user->nick, Ptr->name, Ptr->setby, (unsigned long)Ptr->topicset);
		}
		userlist(user,Ptr);
		user->WriteServ("366 %s %s :End of /NAMES list.", user->nick, Ptr->name);
	}
	FOREACH_MOD(I_OnUserJoin,OnUserJoin(user,Ptr));
	return Ptr;
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
			break;
		}
	}

	if (!this->DelUser(user)) /* if there are no users left on the channel... */
	{
		chan_hash::iterator iter = ServerInstance->chanlist.find(this->name);
		/* kill the record */
		if (iter != ServerInstance->chanlist.end())
		{
			log(DEBUG,"del_channel: destroyed: %s", this->name);
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
		if ((is_uline(user->server)) && (!is_uline(src->server)))
		{
			src->WriteServ("482 %s %s :Only a u-line may kick a u-line from a channel.",src->nick, this->name);
			return this->GetUserCounter();
		}
		int MOD_RESULT = 0;

		if (!is_uline(src->server))
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
			if ((MOD_RESULT == ACR_DENY) && (!is_uline(src->server)))
				return this->GetUserCounter();
	
			if ((MOD_RESULT == ACR_DEFAULT) || (!is_uline(src->server)))
			{
				int them = cstatus(src, this);
				int us = cstatus(user, this);
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
		if (i->second->fd != FD_MAGIC_NUMBER)
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
void chanrec::WriteAllExceptSender(userrec* user, char status, char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if (!user || !text)
		return;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteAllExceptSender(user, status, std::string(textbuffer));
}

void chanrec::WriteAllExceptSender(userrec* user, char status, const std::string& text)
{
	CUList *ulist;

	if (!user)
		return;

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
			i->second->WriteFrom(user,text);
	}
}

