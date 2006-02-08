/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
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

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif

extern ServerConfig* Config;

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern int WHOWAS_STALE;
extern int WHOWAS_MAX;
extern time_t TIME;
extern chan_hash chanlist;

using namespace std;

std::vector<ModeParameter> custom_mode_params;

chanrec* ForceChan(chanrec* Ptr,ucrec &a,userrec* user, int created);

chanrec::chanrec()
{
	*name = *custom_modes = *topic = *setby = *key = 0;
	created = topicset = limit = binarymodes = 0;
	internal_userlist.clear();
}

void chanrec::SetCustomMode(char mode,bool mode_on)
{
	if (mode_on) {
		static char m[3];
		m[0] = mode;
		m[1] = '\0';
		if (!strchr(this->custom_modes,mode))
		{
			strlcat(custom_modes,m,MAXMODES);
		}
		log(DEBUG,"Custom mode %c set",mode);
	}
	else {

		std::string a = this->custom_modes;
		int pos = a.find(mode);
		a.erase(pos,1);
		strlcpy(this->custom_modes,a.c_str(),MAXMODES);

		log(DEBUG,"Custom mode %c removed: modelist='%s'",mode,this->custom_modes);
		this->SetCustomModeParam(mode,"",false);
	}
}


void chanrec::SetCustomModeParam(char mode,char* parameter,bool mode_on)
{

	log(DEBUG,"SetCustomModeParam called");
	ModeParameter M;
	M.mode = mode;
	strlcpy(M.channel,this->name,CHANMAX);
	strlcpy(M.parameter,parameter,MAXBUF);
	if (mode_on)
	{
		log(DEBUG,"Custom mode parameter %c %s added",mode,parameter);
		custom_mode_params.push_back(M);
	}
	else
	{
		if (custom_mode_params.size())
		{
			for (vector<ModeParameter>::iterator i = custom_mode_params.begin(); i < custom_mode_params.end(); i++)
			{
				if ((i->mode == mode) && (!strcasecmp(this->name,i->channel)))
				{
					log(DEBUG,"Custom mode parameter %c %s removed",mode,parameter);
					custom_mode_params.erase(i);
					return;
				}
			}
		}
		log(DEBUG,"*** BUG *** Attempt to remove non-existent mode parameter!");
	}
}

bool chanrec::IsCustomModeSet(char mode)
{
	return (strchr(this->custom_modes,mode));
}

std::string chanrec::GetModeParameter(char mode)
{
	if (custom_mode_params.size())
	{
		for (vector<ModeParameter>::iterator i = custom_mode_params.begin(); i < custom_mode_params.end(); i++)
		{
			if ((i->mode == mode) && (!strcasecmp(this->name,i->channel)))
			{
				return i->parameter;
			}
		}
	}
	return "";
}

long chanrec::GetUserCounter()
{
	return (this->internal_userlist.size());
}

void chanrec::AddUser(char* castuser)
{
	internal_userlist[castuser] = castuser;
	log(DEBUG,"Added casted user to channel's internal list");
}

void chanrec::DelUser(char* castuser)
{
	std::map<char*,char*>::iterator a = internal_userlist.find(castuser);
	if (a != internal_userlist.end())
	{
		log(DEBUG,"Removed casted user from channel's internal list");
		internal_userlist.erase(a);
		/* And tidy any others... */
		DelOppedUser(castuser);
		DelHalfoppedUser(castuser);
		DelVoicedUser(castuser);
		return;
	}
}

void chanrec::AddOppedUser(char* castuser)
{
        internal_op_userlist[castuser] = castuser;
        log(DEBUG,"Added casted user to channel's internal list");
}

void chanrec::DelOppedUser(char* castuser)
{
        std::map<char*,char*>::iterator a = internal_op_userlist.find(castuser);
        if (a != internal_op_userlist.end())
        {
                log(DEBUG,"Removed casted user from channel's internal list");
                internal_op_userlist.erase(a);
                return;
        }
}

void chanrec::AddHalfoppedUser(char* castuser)
{
        internal_halfop_userlist[castuser] = castuser;
        log(DEBUG,"Added casted user to channel's internal list");
}

void chanrec::DelHalfoppedUser(char* castuser)
{
        std::map<char*,char*>::iterator a = internal_halfop_userlist.find(castuser);
        if (a != internal_halfop_userlist.end())
        {       
                log(DEBUG,"Removed casted user from channel's internal list");
                internal_halfop_userlist.erase(a);
                return; 
        }       
}

void chanrec::AddVoicedUser(char* castuser)
{
        internal_voice_userlist[castuser] = castuser;
        log(DEBUG,"Added casted user to channel's internal list");
}

void chanrec::DelVoicedUser(char* castuser)
{
        std::map<char*,char*>::iterator a = internal_voice_userlist.find(castuser);
        if (a != internal_voice_userlist.end())
        {       
                log(DEBUG,"Removed casted user from channel's internal list");
                internal_voice_userlist.erase(a);
                return; 
        }       
}

std::map<char*,char*> *chanrec::GetUsers()
{
	return &internal_userlist;
}

std::map<char*,char*> *chanrec::GetOppedUsers()
{
	        return &internal_op_userlist;
}

std::map<char*,char*> *chanrec::GetHalfoppedUsers()
{
	        return &internal_halfop_userlist;
}

std::map<char*,char*> *chanrec::GetVoicedUsers()
{
	        return &internal_voice_userlist;
}

/* add a channel to a user, creating the record for it if needed and linking
 * it to the user record */

chanrec* add_channel(userrec *user, const char* cn, const char* key, bool override)
{
        if ((!user) || (!cn))
        {
                log(DEFAULT,"*** BUG *** add_channel was given an invalid parameter");
                return 0;
        }

        int created = 0;
        char cname[MAXBUF];
        int MOD_RESULT = 0;
        strlcpy(cname,cn,CHANMAX);
	log(DEBUG,"cname='%s' cn='%s'",cname,cn);

        log(DEBUG,"add_channel: %s %s",user->nick,cname);

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
                chanlist[cname] = new chanrec();
                strlcpy(chanlist[cname]->name, cname,CHANMAX);
                chanlist[cname]->binarymodes = CM_TOPICLOCK | CM_NOEXTERNAL;
                chanlist[cname]->created = TIME;
                *chanlist[cname]->topic = 0;
                strlcpy(chanlist[cname]->setby, user->nick,NICKMAX);
                chanlist[cname]->topicset = 0;
                Ptr = chanlist[cname];
                log(DEBUG,"add_channel: created: %s",cname);
                /* set created to 2 to indicate user
                 * is the first in the channel
                 * and should be given ops */
                created = 2;
        }
        else
        {
                /* Already on the channel */
                if (has_channel(user,Ptr))
                        return NULL;

                // remote users are allowed us to bypass channel modes
                // and bans (used by servers)
                if (user->fd > -1)
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
                                                        log(DEBUG,"add_channel: no key given in JOIN");
                                                        WriteServ(user->fd,"475 %s %s :Cannot join channel (Requires key)",user->nick, Ptr->name);
                                                        return NULL;
                                                }
                                                else
                                                {
                                                        if (strcasecmp(key,Ptr->key))
                                                        {
                                                                log(DEBUG,"add_channel: bad key given in JOIN");
                                                                WriteServ(user->fd,"475 %s %s :Cannot join channel (Incorrect key)",user->nick, Ptr->name);
                                                                return NULL;
                                                        }
                                                }
                                        }
                                }
                                if (Ptr->binarymodes & CM_INVITEONLY)
                                {
					MOD_RESULT = 0;
					irc::string xname(Ptr->name);
                                        FOREACH_RESULT(I_OnCheckInvite,OnCheckInvite(user, Ptr));
                                        if (!MOD_RESULT)
                                        {
                                                log(DEBUG,"add_channel: channel is +i");
                                                if (user->IsInvited(xname))
                                                {
                                                        /* user was invited to channel */
                                                        /* there may be an optional channel NOTICE here */
                                                }
                                                else
                                                {
                                                        WriteServ(user->fd,"473 %s %s :Cannot join channel (Invite only)",user->nick, Ptr->name);
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
                                                if (usercount(Ptr) >= Ptr->limit)
                                                {
                                                        WriteServ(user->fd,"471 %s %s :Cannot join channel (Channel is full)",user->nick, Ptr->name);
                                                        return NULL;
                                                }
                                        }
                                }
                                if (Ptr->bans.size())
                                {
                                        log(DEBUG,"add_channel: about to walk banlist");
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(I_OnCheckBan,OnCheckBan(user, Ptr));
                                        if (!MOD_RESULT)
                                        {
                                                for (BanList::iterator i = Ptr->bans.begin(); i != Ptr->bans.end(); i++)
                                                {
                                                        if ((match(user->GetFullHost(),i->data)) || (match(user->GetFullRealHost(),i->data)))
                                                        {
                                                                WriteServ(user->fd,"474 %s %s :Cannot join channel (You're banned)",user->nick, Ptr->name);
                                                                return NULL;
                                                        }
                                                }
                                        }
                                }
                        }
                }
                else
                {
                        log(DEBUG,"Overridden checks");
                }
                created = 1;
        }

        log(DEBUG,"Passed channel checks");

        for (unsigned int index =0; index < user->chans.size(); index++)
        {
                if (user->chans[index].channel == NULL)
                {
                        return ForceChan(Ptr,user->chans[index],user,created);
                }
        }
        /* XXX: If the user is an oper here, we can just extend their user->chans vector by one
         * and put the channel in here. Same for remote users which are not bound by
         * the channel limits. Otherwise, nope, youre boned.
         */
        if (user->fd < 0)
        {
                ucrec a;
                chanrec* c = ForceChan(Ptr,a,user,created);
                user->chans.push_back(a);
                return c;
        }
        else if (strchr(user->modes,'o'))
        {
                /* Oper allows extension up to the OPERMAXCHANS value */
                if (user->chans.size() < OPERMAXCHANS)
                {
                        ucrec a;
                        chanrec* c = ForceChan(Ptr,a,user,created);
                        user->chans.push_back(a);
                        return c;
                }
        }
        log(DEBUG,"add_channel: user channel max exceeded: %s %s",user->nick,cname);
        WriteServ(user->fd,"405 %s %s :You are on too many channels",user->nick, cname);
        return NULL;
}

chanrec* ForceChan(chanrec* Ptr,ucrec &a,userrec* user, int created)
{
        if (created == 2)
        {
                /* first user in is given ops */
                a.uc_modes = UCMODE_OP;
		Ptr->AddOppedUser((char*)user);
        }
        else
        {
                a.uc_modes = 0;
        }
        a.channel = Ptr;
        Ptr->AddUser((char*)user);
        WriteChannel(Ptr,user,"JOIN :%s",Ptr->name);
        log(DEBUG,"Sent JOIN to client");
        if (Ptr->topicset)
        {
                WriteServ(user->fd,"332 %s %s :%s", user->nick, Ptr->name, Ptr->topic);
                WriteServ(user->fd,"333 %s %s %s %lu", user->nick, Ptr->name, Ptr->setby, (unsigned long)Ptr->topicset);
        }
        userlist(user,Ptr);
        WriteServ(user->fd,"366 %s %s :End of /NAMES list.", user->nick, Ptr->name);
        FOREACH_MOD(I_OnUserJoin,OnUserJoin(user,Ptr));
        return Ptr;
}

/* remove a channel from a users record, and remove the record from memory
 * if the channel has become empty */

chanrec* del_channel(userrec *user, const char* cname, const char* reason, bool local)
{
        if ((!user) || (!cname))
        {
                log(DEFAULT,"*** BUG *** del_channel was given an invalid parameter");
                return NULL;
        }

        chanrec* Ptr = FindChan(cname);

        if (!Ptr)
                return NULL;

        log(DEBUG,"del_channel: removing: %s %s",user->nick,Ptr->name);

        for (unsigned int i =0; i < user->chans.size(); i++)
        {
                /* zap it from the channel list of the user */
                if (user->chans[i].channel == Ptr)
                {
                        if (reason)
                        {
				FOREACH_MOD(I_OnUserPart,OnUserPart(user,Ptr,reason));
                                WriteChannel(Ptr,user,"PART %s :%s",Ptr->name, reason);
                        }
                        else
                        {
				FOREACH_MOD(I_OnUserPart,OnUserPart(user,Ptr,""));
                                WriteChannel(Ptr,user,"PART :%s",Ptr->name);
                        }
                        user->chans[i].uc_modes = 0;
                        user->chans[i].channel = NULL;
                        log(DEBUG,"del_channel: unlinked: %s %s",user->nick,Ptr->name);
                        break;
                }
        }

        Ptr->DelUser((char*)user);

        /* if there are no users left on the channel */
        if (!usercount(Ptr))
        {
                chan_hash::iterator iter = chanlist.find(Ptr->name);

                log(DEBUG,"del_channel: destroying channel: %s",Ptr->name);

                /* kill the record */
                if (iter != chanlist.end())
                {
                        log(DEBUG,"del_channel: destroyed: %s",Ptr->name);
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(Ptr));
                        delete Ptr;
                        chanlist.erase(iter);
                }
        }

        return NULL;
}

void server_kick_channel(userrec* user, chanrec* Ptr, char* reason, bool triggerevents)
{
	if ((!user) || (!Ptr) || (!reason))
	{
		return;
	}

	if (IS_LOCAL(user))
	{
		if (!has_channel(user,Ptr))
		{
			/* Not on channel */
			return;
		}
	}
	
	if (triggerevents)
	{
		FOREACH_MOD(I_OnUserKick,OnUserKick(NULL,user,Ptr,reason));
	}

	for (unsigned int i =0; i < user->chans.size(); i++)
	{
		if (user->chans[i].channel)
		if (!strcasecmp(user->chans[i].channel->name,Ptr->name))
		{
			WriteChannelWithServ(Config->ServerName,Ptr,"KICK %s %s :%s",Ptr->name, user->nick, reason);
			user->chans[i].uc_modes = 0;
			user->chans[i].channel = NULL;
			break;
		}
	}

	Ptr->DelUser((char*)user);

        if (!usercount(Ptr))
        {
                chan_hash::iterator iter = chanlist.find(Ptr->name);
                log(DEBUG,"del_channel: destroying channel: %s",Ptr->name);
                /* kill the record */
                if (iter != chanlist.end())
                {
                        log(DEBUG,"del_channel: destroyed: %s",Ptr->name);
                       	FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(Ptr));
                        delete Ptr;
                        chanlist.erase(iter);
                }
        }
}

void kick_channel(userrec *src,userrec *user, chanrec *Ptr, char* reason)
{
        if ((!src) || (!user) || (!Ptr) || (!reason))
        {
                log(DEFAULT,"*** BUG *** kick_channel was given an invalid parameter");
                return;
        }

        log(DEBUG,"kick_channel: removing: %s %s %s",user->nick,Ptr->name,src->nick);

	if (IS_LOCAL(src))
	{
	        if (!has_channel(user,Ptr))
	        {
	                WriteServ(src->fd,"441 %s %s %s :They are not on that channel",src->nick, user->nick, Ptr->name);
	                return;
	        }
	        int MOD_RESULT = 0;

		if (!is_uline(src->server))
		{
			MOD_RESULT = 0;
			FOREACH_RESULT(I_OnUserPreKick,OnUserPreKick(src,user,Ptr,reason));
			if (MOD_RESULT == 1)
				return;
		}
		/* Set to -1 by OnUserPreKick if explicit allow was set */
		if (MOD_RESULT != -1)
		{
		        FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(src,user,Ptr,AC_KICK));
		        if ((MOD_RESULT == ACR_DENY) && (!is_uline(src->server)))
		                return;
	
		        if ((MOD_RESULT == ACR_DEFAULT) || (!is_uline(src->server)))
		        {
       		 	        if ((cstatus(src,Ptr) < STATUS_HOP) || (cstatus(src,Ptr) < cstatus(user,Ptr)))
		                {
		                        if (cstatus(src,Ptr) == STATUS_HOP)
		                        {
		                                WriteServ(src->fd,"482 %s %s :You must be a channel operator",src->nick, Ptr->name);
		                        }
		                        else
		                        {
		                                WriteServ(src->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",src->nick, Ptr->name);
		                        }
		
		                        return;
		                }
		        }
		}
	}

        FOREACH_MOD(I_OnUserKick,OnUserKick(src,user,Ptr,reason));

        for (unsigned int i =0; i < user->chans.size(); i++)
        {
                /* zap it from the channel list of the user */
                if (user->chans[i].channel)
                if (!strcasecmp(user->chans[i].channel->name,Ptr->name))
                {
                        WriteChannel(Ptr,src,"KICK %s %s :%s",Ptr->name, user->nick, reason);
                        user->chans[i].uc_modes = 0;
                        user->chans[i].channel = NULL;
                        log(DEBUG,"del_channel: unlinked: %s %s",user->nick,Ptr->name);
                        break;
                }
        }

        Ptr->DelUser((char*)user);

        /* if there are no users left on the channel */
        if (!usercount(Ptr))
        {
                chan_hash::iterator iter = chanlist.find(Ptr->name);

                log(DEBUG,"del_channel: destroying channel: %s",Ptr->name);

                /* kill the record */
                if (iter != chanlist.end())
                {
                        log(DEBUG,"del_channel: destroyed: %s",Ptr->name);
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(Ptr));
                        delete Ptr;
                        chanlist.erase(iter);
                }
        }
}


