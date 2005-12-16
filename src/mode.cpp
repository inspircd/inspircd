/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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
#include "connection.h"
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "xline.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "mode.h"

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern ServerConfig* Config;
extern ModeParser* ModeGrok;

extern time_t TIME;

char* ModeParser::GiveOps(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** GiveOps was given an invalid parameter");
		return NULL;
	}

	if (!isnick(dest))
	{
		log(DEFAULT,"the target nickname given to GiveOps was invalid");
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	d = Find(dest);
	if (!d)
	{
		log(DEFAULT,"the target nickname given to GiveOps couldnt be found");
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{

		int MOD_RESULT = 0;
		FOREACH_RESULT(OnAccessCheck(user,d,chan,AC_OP));
		
		if (MOD_RESULT == ACR_DENY)
			return NULL;
		if (MOD_RESULT == ACR_DEFAULT)
		{
			if ((status < STATUS_OP) && (!is_uline(user->server)))
			{
				log(DEBUG,"%s cant give ops to %s because they nave status %d and needs %d",user->nick,dest,status,STATUS_OP);
				WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
				return NULL;
			}
		}


		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
			if (d->chans[i].uc_modes & UCMODE_OP)
				{
					/* mode already set on user, dont allow multiple */
					log(DEFAULT,"The target user given to GiveOps was already opped on the channel");
					return NULL;
				}
				d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_OP;
				log(DEBUG,"gave ops: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
		log(DEFAULT,"The target channel given to GiveOps was not in the users mode list");
	}
	return NULL;
}

char* ModeParser::GiveHops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** GiveHops was given an invalid parameter");
		return NULL;
	}

	d = Find(dest);
	if (!isnick(dest))
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	if (!d)
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(OnAccessCheck(user,d,chan,AC_HALFOP));
		
		if (MOD_RESULT == ACR_DENY)
			return NULL;
		if (MOD_RESULT == ACR_DEFAULT)
		{
			if ((status < STATUS_OP) && (!is_uline(user->server)))
			{
				WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
				return NULL;
			}
		}

		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if (d->chans[i].uc_modes & UCMODE_HOP)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_HOP;
				log(DEBUG,"gave h-ops: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
	}
	return NULL;
}

char* ModeParser::GiveVoice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** GiveVoice was given an invalid parameter");
		return NULL;
	}

	d = Find(dest);
	if (!isnick(dest))
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	if (!d)
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(OnAccessCheck(user,d,chan,AC_VOICE));
		
		if (MOD_RESULT == ACR_DENY)
			return NULL;
		if (MOD_RESULT == ACR_DEFAULT)
		{
			if ((status < STATUS_HOP) && (!is_uline(user->server)))
			{
				WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
				return NULL;
			}
		}

		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if (d->chans[i].uc_modes & UCMODE_VOICE)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_VOICE;
				log(DEBUG,"gave voice: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
	}
	return NULL;
}

char* ModeParser::TakeOps(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** TakeOps was given an invalid parameter");
		return NULL;
	}

	d = Find(dest);
	if (!isnick(dest))
	{
		log(DEBUG,"TakeOps was given an invalid target nickname of %s",dest);
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	if (!d)
	{
		log(DEBUG,"TakeOps couldnt resolve the target nickname: %s",dest);
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(OnAccessCheck(user,d,chan,AC_DEOP));
		
		if (MOD_RESULT == ACR_DENY)
			return NULL;
		if (MOD_RESULT == ACR_DEFAULT)
		{
			if ((status < STATUS_OP) && (!is_uline(user->server)))
			{
				WriteServ(user->fd,"482 %s %s :You are not a channel operator",user->nick, chan->name);
				return NULL;
			}
		}

		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if ((d->chans[i].uc_modes & UCMODE_OP) == 0)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes ^= UCMODE_OP;
				log(DEBUG,"took ops: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
		log(DEBUG,"TakeOps couldnt locate the target channel in the target users list");
	}
	return NULL;
}

char* ModeParser::TakeHops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** TakeHops was given an invalid parameter");
		return NULL;
	}

	d = Find(dest);
	if (!isnick(dest))
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	if (!d)
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(OnAccessCheck(user,d,chan,AC_DEHALFOP));
		
		if (MOD_RESULT == ACR_DENY)
			return NULL;
		if (MOD_RESULT == ACR_DEFAULT)
		{
			if ((status < STATUS_OP) && (!is_uline(user->server)))
			{
				WriteServ(user->fd,"482 %s %s :You are not a channel operator",user->nick, chan->name);
				return NULL;
			}
		}

		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if ((d->chans[i].uc_modes & UCMODE_HOP) == 0)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes ^= UCMODE_HOP;
				log(DEBUG,"took h-ops: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
	}
	return NULL;
}

char* ModeParser::TakeVoice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** TakeVoice was given an invalid parameter");
		return NULL;
	}

	d = Find(dest);
	if (!isnick(dest))
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	if (!d)
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	else
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(OnAccessCheck(user,d,chan,AC_DEVOICE));
		
		if (MOD_RESULT == ACR_DENY)
			return NULL;
		if (MOD_RESULT == ACR_DEFAULT)
		{
			if ((status < STATUS_HOP) && (!is_uline(user->server)))
			{
				WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
				return NULL;
			}
		}

		for (unsigned int i = 0; i < d->chans.size(); i++)
		{
			if ((d->chans[i].channel != NULL) && (chan != NULL))
			if (!strcasecmp(d->chans[i].channel->name,chan->name))
			{
				if ((d->chans[i].uc_modes & UCMODE_VOICE) == 0)
				{
					/* mode already set on user, dont allow multiple */
					return NULL;
				}
				d->chans[i].uc_modes ^= UCMODE_VOICE;
				log(DEBUG,"took voice: %s %s",d->chans[i].channel->name,d->nick);
				return d->nick;
			}
		}
	}
	return NULL;
}

char* ModeParser::AddBan(userrec *user,char *dest,chanrec *chan,int status)
{
	if ((!user) || (!dest) || (!chan)) {
		log(DEFAULT,"*** BUG *** AddBan was given an invalid parameter");
		return NULL;
	}

	BanItem b;
	if ((!user) || (!dest) || (!chan))
		return NULL;
	unsigned int l = strlen(dest);
	if (strchr(dest,'!')==0)
		return NULL;
	if (strchr(dest,'@')==0)
		return NULL;
	for (unsigned int i = 0; i < l; i++)
		if (dest[i] < 32)
			return NULL;
	for (unsigned int i = 0; i < l; i++)
		if (dest[i] > 126)
			return NULL;
	int c = 0;
	for (unsigned int i = 0; i < l; i++)
		if (dest[i] == '!')
			c++;
	if (c>1)
		return NULL;
	c = 0;
	for (unsigned int i = 0; i < l; i++)
		if (dest[i] == '@')
			c++;
	if (c>1)
		return NULL;

	long maxbans = GetMaxBans(chan->name);
	if ((unsigned)chan->bans.size() > (unsigned)maxbans)
	{
		WriteServ(user->fd,"478 %s %s :Channel ban list for %s is full (maximum entries for this channel is %d)",user->nick, chan->name,chan->name,maxbans);
		return NULL;
	}

	log(DEBUG,"AddBan: %s %s",chan->name,user->nick);

	int MOD_RESULT = 0;
	FOREACH_RESULT(OnAddBan(user,chan,dest));
	if (MOD_RESULT)
		return NULL;

	TidyBan(dest);
	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest))
		{
			// dont allow a user to set the same ban twice
			return NULL;
		}
	}

	b.set_time = TIME;
	strncpy(b.data,dest,MAXBUF);
	strncpy(b.set_by,user->nick,NICKMAX);
	chan->bans.push_back(b);
	return dest;
}

char* ModeParser::TakeBan(userrec *user,char *dest,chanrec *chan,int status)
{
	if ((!user) || (!dest) || (!chan)) {
		log(DEFAULT,"*** BUG *** TakeBan was given an invalid parameter");
		return 0;
	}

	log(DEBUG,"del_ban: %s %s",chan->name,user->nick);
	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest))
		{
		        int MOD_RESULT = 0;
		        FOREACH_RESULT(OnDelBan(user,chan,dest));
		        if (MOD_RESULT)
		                return NULL;
			chan->bans.erase(i);
			return dest;
		}
	}
	return NULL;
}

// tidies up redundant modes, e.g. +nt-nt+i becomes +-+i,
// a section further down the chain tidies up the +-+- crap.
std::string ModeParser::CompressModes(std::string modes,bool channelmodes)
{
	int counts[127];
	bool active[127];
	memset(counts,0,sizeof(counts));
	memset(active,0,sizeof(active));
	for (unsigned int i = 0; i < modes.length(); i++)
	{
		if ((modes[i] == '+') || (modes[i] == '-'))
			continue;
		if (channelmodes)
		{
			if ((strchr("itnmsp",modes[i])) || ((ModeDefined(modes[i],MT_CHANNEL)) && (ModeDefinedOn(modes[i],MT_CHANNEL)==0) && (ModeDefinedOff(modes[i],MT_CHANNEL)==0)))
			{
				log(DEBUG,"Tidy mode %c",modes[i]);
				counts[(unsigned int)modes[i]]++;
				active[(unsigned int)modes[i]] = true;
			}
		}
		else
		{
			log(DEBUG,"Tidy mode %c",modes[i]);
			counts[(unsigned int)modes[i]]++;
			active[(unsigned int)modes[i]] = true;
		}
	}
	for (int j = 65; j < 127; j++)
	{
		if ((counts[j] > 1) && (active[j] == true))
		{
			static char v[2];
			v[0] = (unsigned char)j;
			v[1] = '\0';
			std::string mode_str = v;
			std::string::size_type pos = modes.find(mode_str);
			if (pos != std::string::npos)
			{
				log(DEBUG,"all occurances of mode %c to be deleted...",(unsigned char)j);
				while (modes.find(mode_str) != std::string::npos)
					modes.erase(modes.find(mode_str),1);
				log(DEBUG,"New mode line: %s",modes.c_str());
			}
		}
	}
	return modes;
}

void ModeParser::ProcessModes(char **parameters,userrec* user,chanrec *chan,int status, int pcnt, bool servermode, bool silent, bool local)
{
	if (!parameters) {
		log(DEFAULT,"*** BUG *** process_modes was given an invalid parameter");
		return;
	}

	char modelist[MAXBUF];
	char outlist[MAXBUF];
	char outstr[MAXBUF];
	char outpars[32][MAXBUF];
	int param = 2;
	int pc = 0;
	int ptr = 0;
	int mdir = 1;
	char* r = NULL;
	bool k_set = false, l_set = false, previously_set_l = false, previously_unset_l = false, previously_set_k = false, previously_unset_k = false;

	if (pcnt < 2)
	{
		return;
	}

	int MOD_RESULT = 0;
	FOREACH_RESULT(OnAccessCheck(user,NULL,chan,AC_GENERAL_MODE));
	
	if (MOD_RESULT == ACR_DENY)
		return;

	log(DEBUG,"process_modes: start: parameters=%d",pcnt);

	strlcpy(modelist,parameters[1],MAXBUF); /* mode list, e.g. +oo-o *
						 * parameters[2] onwards are parameters for
					 	 * modes that require them :) */
	strlcpy(outlist,"+",MAXBUF);
	mdir = 1;

	log(DEBUG,"process_modes: modelist: %s",modelist);

	std::string tidied = this->CompressModes(modelist,true);
	strlcpy(modelist,tidied.c_str(),MAXBUF);

	int len = strlen(modelist);
	while (modelist[len-1] == ' ')
		modelist[--len] = '\0';
	for (ptr = 0; ptr < len; ptr++)
	{
		r = NULL;

		{
			log(DEBUG,"process_modes: modechar: %c",modelist[ptr]);

			char modechar = modelist[ptr];
			switch (modelist[ptr])
			{
				case '-':
					if (mdir != 0)
					{
						int t = strlen(outlist)-1;
						if ((outlist[t] == '+') || (outlist[t] == '-'))
						{
							outlist[t] = '-';
						}
						else
						{
							strcat(outlist,"-");
						}
					}
					mdir = 0;
					
				break;			

				case '+':
					if (mdir != 1)
					{
						int t = strlen(outlist)-1;
						if ((outlist[t] == '+') || (outlist[t] == '-'))
						{
							outlist[t] = '+';
						}
						else
						{
							strcat(outlist,"+");
						}
					}
					mdir = 1;
				break;

				case 'o':
					log(DEBUG,"Ops");
					if ((param >= pcnt)) break;
					log(DEBUG,"Enough parameters left");
					if (mdir == 1)
					{
						MOD_RESULT = 0;
						FOREACH_RESULT(OnRawMode(user, chan, 'o', parameters[param], true, 1));
						if (!MOD_RESULT)
						{
							log(DEBUG,"calling GiveOps");
							r = GiveOps(user,parameters[param++],chan,status);
						}
						else param++;
					}
					else
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(OnRawMode(user, chan, 'o', parameters[param], false, 1));
                                                if (!MOD_RESULT)
                                                {
							log(DEBUG,"calling TakeOps");
							r = TakeOps(user,parameters[param++],chan,status);
						}
						else param++;
					}
					if (r)
					{
						strlcat(outlist,"o",MAXBUF);
						strlcpy(outpars[pc++],r,MAXBUF);
					}
				break;
			
				case 'h':
					if (((param >= pcnt)) || (!Config->AllowHalfop)) break;
					if (mdir == 1)
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(OnRawMode(user, chan, 'h', parameters[param], true, 1));
                                                if (!MOD_RESULT)
                                                {
							r = GiveHops(user,parameters[param++],chan,status);
						}
						else param++;
					}
					else
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(OnRawMode(user, chan, 'h', parameters[param], false, 1));
                                                if (!MOD_RESULT)
                                                {
							r = TakeHops(user,parameters[param++],chan,status);
						}
						else param++;
					}
					if (r)
					{
						strlcat(outlist,"h",MAXBUF);
						strlcpy(outpars[pc++],r,MAXBUF);
					}
				break;
			
				
				case 'v':
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(OnRawMode(user, chan, 'v', parameters[param], true, 1));
                                                if (!MOD_RESULT)
                                                {
							r = GiveVoice(user,parameters[param++],chan,status);
						}
						else param++;
					}
					else
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(OnRawMode(user, chan, 'v', parameters[param], false, 1));
                                                if (!MOD_RESULT)
                                                {
							r = TakeVoice(user,parameters[param++],chan,status);
						}
						else param++;
					}
					if (r)
					{
						strlcat(outlist,"v",MAXBUF);
						strlcpy(outpars[pc++],r,MAXBUF);
					}
				break;
				
				case 'b':
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(OnRawMode(user, chan, 'b', parameters[param], true, 1));
                                                if (!MOD_RESULT)
                                                {
							r = AddBan(user,parameters[param++],chan,status);
						}
						else param++;
					}
					else
					{
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(OnRawMode(user, chan, 'b', parameters[param], false, 1));
                                                if (!MOD_RESULT)
                                                {
							r = TakeBan(user,parameters[param++],chan,status);
						}
						else param++;
					}
					if (r)
					{
						strlcat(outlist,"b",MAXBUF);
						strlcpy(outpars[pc++],parameters[param-1],MAXBUF);
					}
				break;


				case 'k':
					if ((param >= pcnt))
						break;

					if (mdir == 1)
					{
						if (k_set)
							break;

						if (previously_unset_k)
							break;
						previously_set_k = true;
						
						if (!strcmp(chan->key,""))
						{
							MOD_RESULT = 0;
							FOREACH_RESULT(OnRawMode(user, chan, 'k', parameters[param], true, 1));
							if (!MOD_RESULT)
							{
								strcat(outlist,"k");
								char key[MAXBUF];
								strlcpy(key,parameters[param++],32);
								strlcpy(outpars[pc++],key,MAXBUF);
								strlcpy(chan->key,key,MAXBUF);
								k_set = true;
							}
							else param++;
						}
					}
					else
					{
						/* checks on -k are case sensitive and only accurate to the
  						   first 32 characters */
						if (previously_set_k)
							break;
						previously_unset_k = true;

						char key[MAXBUF];
						MOD_RESULT = 0;
						FOREACH_RESULT(OnRawMode(user, chan, 'k', parameters[param], false, 1));
						if (!MOD_RESULT)
						{
							strlcpy(key,parameters[param++],32);
							/* only allow -k if correct key given */
							if (!strcmp(chan->key,key))
							{
								strlcat(outlist,"k",MAXBUF);
								strlcpy(chan->key,"",MAXBUF);
								strlcpy(outpars[pc++],key,MAXBUF);
							}
						}
						else param++;
					}
				break;
				
				case 'l':
					if (mdir == 0)
					{
						if (previously_set_l)
							break;
						previously_unset_l = true;
                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(OnRawMode(user, chan, 'l', "", false, 0));
                                                if (!MOD_RESULT)
                                                {
							if (chan->limit)
							{
								strcat(outlist,"l");
								chan->limit = 0;
							}
						}
					}
					
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
						if (l_set)
							break;
						if (previously_unset_l)
							break;
						previously_set_l = true;
						bool invalid = false;
						for (unsigned int i = 0; i < strlen(parameters[param]); i++)
						{
							if ((parameters[param][i] < '0') || (parameters[param][i] > '9'))
							{
								invalid = true;
							}
						}
						if (atoi(parameters[param]) < 1)
						{
							invalid = true;
						}

						if (invalid)
							break;

                                                MOD_RESULT = 0;
                                                FOREACH_RESULT(OnRawMode(user, chan, 'l', parameters[param], true, 1));
                                                if (!MOD_RESULT)
                                                {
	
							chan->limit = atoi(parameters[param]);
							
							// reported by mech: large values cause underflow
							if (chan->limit < 0)
								chan->limit = 0x7FFF;
						}
							
						if (chan->limit)
						{
							strlcat(outlist,"l",MAXBUF);
							strlcpy(outpars[pc++],parameters[param++],MAXBUF);
							l_set = true;
						}
					}
				break;
				
				case 'i':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(OnRawMode(user, chan, 'i', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
						if (mdir)
						{
							if (!(chan->binarymodes & CM_INVITEONLY)) strlcat(outlist,"i",MAXBUF);
							chan->binarymodes |= CM_INVITEONLY;
						}
						else
						{
							if (chan->binarymodes & CM_INVITEONLY) strlcat(outlist,"i",MAXBUF);
							chan->binarymodes &= ~CM_INVITEONLY;
						}
					}
				break;
				
				case 't':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(OnRawMode(user, chan, 't', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
						if (mdir)
                                                {
							if (!(chan->binarymodes & CM_TOPICLOCK)) strlcat(outlist,"t",MAXBUF);
                                                        chan->binarymodes |= CM_TOPICLOCK;
                                                }
                                                else
                                                {
							if (chan->binarymodes & CM_NOEXTERNAL) strlcat(outlist,"t",MAXBUF);
                                                        chan->binarymodes &= ~CM_TOPICLOCK;
                                                }
					}
				break;
				
				case 'n':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(OnRawMode(user, chan, 'n', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
                                                if (mdir)
                                                {
							if (!(chan->binarymodes & CM_NOEXTERNAL)) strlcat(outlist,"n",MAXBUF);
                                                        chan->binarymodes |= CM_NOEXTERNAL;
                                                }
                                                else
                                                {
							if (chan->binarymodes & CM_NOEXTERNAL) strlcat(outlist,"n",MAXBUF);
                                                        chan->binarymodes &= ~CM_NOEXTERNAL;
                                                }
					}
				break;
				
				case 'm':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(OnRawMode(user, chan, 'm', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
                                                if (mdir)
                                                {
                                                        if (!(chan->binarymodes & CM_MODERATED)) strlcat(outlist,"m",MAXBUF);
                                                        chan->binarymodes |= CM_MODERATED;
                                                }
                                                else
                                                {
                                                        if (chan->binarymodes & CM_MODERATED) strlcat(outlist,"m",MAXBUF);
                                                        chan->binarymodes &= ~CM_MODERATED;
                                                }
					}
				break;
				
				case 's':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(OnRawMode(user, chan, 's', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
                                                if (mdir)
                                                {
                                                        if (!(chan->binarymodes & CM_SECRET)) strlcat(outlist,"s",MAXBUF);
                                                        chan->binarymodes |= CM_SECRET;
                                                        if (chan->binarymodes & CM_PRIVATE)
                                                        {
                                                                chan->binarymodes &= ~CM_PRIVATE;
                                                                if (mdir)
                                                                {
                                                                        strlcat(outlist,"-p+",MAXBUF);
                                                                }
                                                        }
                                                }
                                                else
                                                {
                                                        if (chan->binarymodes & CM_SECRET) strlcat(outlist,"s",MAXBUF);
                                                        chan->binarymodes &= ~CM_SECRET;
                                                }
					}
				break;
				
				case 'p':
                                        MOD_RESULT = 0;
                                        FOREACH_RESULT(OnRawMode(user, chan, 'p', "", mdir, 0));
                                        if (!MOD_RESULT)
                                        {
                                                if (mdir)
                                                {
                                                        if (!(chan->binarymodes & CM_PRIVATE)) strlcat(outlist,"p",MAXBUF);
                                                        chan->binarymodes |= CM_PRIVATE;
                                                        if (chan->binarymodes & CM_SECRET)
                                                        {
                                                                chan->binarymodes &= ~CM_SECRET;
                                                                if (mdir)
                                                                {
                                                                        strlcat(outlist,"-s+",MAXBUF);
                                                                }
                                                        }
                                                }
                                                else
                                                {
                                                        if (chan->binarymodes & CM_PRIVATE) strlcat(outlist,"p",MAXBUF);
                                                        chan->binarymodes &= ~CM_PRIVATE;
                                                }
					}
				break;
				
				default:
					log(DEBUG,"Preprocessing custom mode %c: modelist: %s",modechar,chan->custom_modes);
					string_list p;
					p.clear();
					if (((!strchr(chan->custom_modes,modechar)) && (!mdir)) || ((strchr(chan->custom_modes,modechar)) && (mdir)))
					{
						if (!ModeIsListMode(modechar,MT_CHANNEL))
						{
							log(DEBUG,"Mode %c isnt set on %s but trying to remove!",modechar,chan->name);
							break;
						}
					}
					if (ModeDefined(modechar,MT_CHANNEL))
					{
						log(DEBUG,"A module has claimed this mode");
						if (param<pcnt)
						{
     							if ((ModeDefinedOn(modechar,MT_CHANNEL)>0) && (mdir))
							{
      								p.push_back(parameters[param]);
  							}
							if ((ModeDefinedOff(modechar,MT_CHANNEL)>0) && (!mdir))
							{
      								p.push_back(parameters[param]);
  							}
  						}
  						bool handled = false;
  						if (param>=pcnt)
  						{
  							// we're supposed to have a parameter, but none was given... so dont handle the mode.
  							if (((ModeDefinedOn(modechar,MT_CHANNEL)>0) && (mdir)) || ((ModeDefinedOff(modechar,MT_CHANNEL)>0) && (!mdir)))	
  							{
  								log(DEBUG,"Not enough parameters for module-mode %c",modechar);
  								handled = true;
  								param++;
  							}
  						}

						// BIG ASS IDIOTIC CODER WARNING!
						// Using OnRawMode on another modules mode's behavour 
						// will confuse the crap out of admins! just because you CAN
						// do it, doesnt mean you SHOULD!
	                                        MOD_RESULT = 0;
						std::string para = "";
						if (p.size())
							para = p[0];
        	                                FOREACH_RESULT(OnRawMode(user, chan, modechar, para, mdir, pcnt));
                	                        if (!MOD_RESULT)
                        	                {
  							for (int i = 0; i <= MODCOUNT; i++)
							{
								if (!handled)
								{
									int t = modules[i]->OnExtendedMode(user,chan,modechar,MT_CHANNEL,mdir,p);
									if (t != 0)
									{
										log(DEBUG,"OnExtendedMode returned nonzero for a module");
										char app[] = {modechar, 0};
										if (ModeIsListMode(modechar,MT_CHANNEL))
										{
											if (t == -1)
											{
												//pc++;
												param++;
											}
											else
											{
												if (ptr>0)
												{
													strlcat(outlist, app,MAXBUF);
												}
												strlcpy(outpars[pc++],parameters[param++],MAXBUF);
											}
										}
										else
										{
											if (ptr>0)
											{
												if ((modelist[ptr-1] == '+') || (modelist[ptr-1] == '-'))
												{
													strlcat(outlist, app,MAXBUF);
												}
												else if (!strchr(outlist,modechar))
												{
													strlcat(outlist, app,MAXBUF);
												}
											}
											chan->SetCustomMode(modechar,mdir);
											// include parameters in output if mode has them
											if ((ModeDefinedOn(modechar,MT_CHANNEL)>0) && (mdir))
											{
												chan->SetCustomModeParam(modelist[ptr],parameters[param],mdir);
												strlcpy(outpars[pc++],parameters[param++],MAXBUF);
											}
										}
										// break, because only one module can handle the mode.
										handled = true;
       	 		 						}
       	 	 						}
	     						}
						}
     					}
					else
					{
						WriteServ(user->fd,"472 %s %c :is unknown mode char to me",user->nick,modechar);
					}
				break;
				
			}
		}
	}

	/* this ensures only the *valid* modes are sent out onto the network */
	int xt = strlen(outlist)-1;
	while ((outlist[xt] == '-') || (outlist[xt] == '+'))
	{
		outlist[xt] = '\0';
		xt = strlen(outlist)-1;
	}
	if (outlist[0])
	{
		strlcpy(outstr,outlist,MAXBUF);
		for (ptr = 0; ptr < pc; ptr++)
		{
			strlcat(outstr," ",MAXBUF);
			strlcat(outstr,outpars[ptr],MAXBUF);
		}
		if (local)
		{
			log(DEBUG,"Local mode change");
			WriteChannelLocal(chan, user, "MODE %s %s",chan->name,outstr);
			FOREACH_MOD OnMode(user, chan, TYPE_CHANNEL, outstr);
		}
		else
		{
			if (servermode)
			{
				if (!silent)
				{
					WriteChannelWithServ(Config->ServerName,chan,"MODE %s %s",chan->name,outstr);
				}
					
			}
			else
			{
				if (!silent)
				{
					WriteChannel(chan,user,"MODE %s %s",chan->name,outstr);
					FOREACH_MOD OnMode(user, chan, TYPE_CHANNEL, outstr);
				}
			}
		}
	}
}

// based on sourcemodes, return true or false to determine if umode is a valid mode a user may set on themselves or others.

bool ModeParser::AllowedUmode(char umode, char* sourcemodes,bool adding,bool serveroverride)
{
	log(DEBUG,"Allowed_umode: %c %s",umode,sourcemodes);
	// Servers can +o and -o arbitrarily
	if ((serveroverride == true) && (umode == 'o'))
	{
		return true;
	}
	// RFC1459 specified modes
	if ((umode == 'w') || (umode == 's') || (umode == 'i'))
	{
		log(DEBUG,"umode %c allowed by RFC1459 scemantics",umode);
		return true;
	}
	
	// user may not +o themselves or others, but an oper may de-oper other opers or themselves
	if ((strchr(sourcemodes,'o')) && (!adding))
	{
		log(DEBUG,"umode %c allowed by RFC1459 scemantics",umode);
		return true;
	}
	else if (umode == 'o')
	{
		log(DEBUG,"umode %c allowed by RFC1459 scemantics",umode);
		return false;
	}
	
	// process any module-defined modes that need oper
	if ((ModeDefinedOper(umode,MT_CLIENT)) && (strchr(sourcemodes,'o')))
	{
		log(DEBUG,"umode %c allowed by module handler (oper only mode)",umode);
		return true;
	}
	else
	if (ModeDefined(umode,MT_CLIENT))
	{
		// process any module-defined modes that don't need oper
		log(DEBUG,"umode %c allowed by module handler (non-oper mode)",umode);
		if ((ModeDefinedOper(umode,MT_CLIENT)) && (!strchr(sourcemodes,'o')))
		{
			// no, this mode needs oper, and this user 'aint got what it takes!
			return false;
		}
		return true;
	}

	// anything else - return false.
	log(DEBUG,"umode %c not known by any ruleset",umode);
	return false;
}

bool ModeParser::ProcessModuleUmode(char umode, userrec* source, void* dest, bool adding)
{
	userrec* s2;
	bool faked = false;
	if (!source)
	{
		s2 = new userrec;
		strlcpy(s2->nick,Config->ServerName,NICKMAX);
		strlcpy(s2->modes,"o",52);
		s2->fd = -1;
		source = s2;
		faked = true;
	}
	string_list p;
	p.clear();
	if (ModeDefined(umode,MT_CLIENT))
	{
		for (int i = 0; i <= MODCOUNT; i++)
		{
			if (modules[i]->OnExtendedMode(source,(void*)dest,umode,MT_CLIENT,adding,p))
			{
				log(DEBUG,"Module %s claims umode %c",Config->module_names[i].c_str(),umode);
				return true;
			}
		}
		log(DEBUG,"No module claims umode %c",umode);
		if (faked)
		{
			delete s2;
			source = NULL;
		}
		return false;
	}
	else
	{
		if (faked)
		{
			delete s2;
			source = NULL;
		}
		return false;
	}
}

void handle_mode(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change;
	int direction = 1;
	char outpars[MAXBUF];

	dest = Find(parameters[0]);

	if (!user)
	{
		return;
	}

	if ((dest) && (pcnt == 1))
	{
		WriteServ(user->fd,"221 %s :+%s",dest->nick,dest->modes);
		return;
	}

	if ((dest) && (pcnt > 1))
	{
		std::string tidied = ModeGrok->CompressModes(parameters[1],false);
		parameters[1] = (char*)tidied.c_str();

		char dmodes[MAXBUF];
		strlcpy(dmodes,dest->modes,52);
		log(DEBUG,"pulled up dest user modes: %s",dmodes);

		can_change = 0;
		if (user != dest)
		{
			if ((strchr(user->modes,'o')) || (is_uline(user->server)))
			{
				can_change = 1;
			}
		}
		else
		{
			can_change = 1;
		}
		if (!can_change)
		{
			WriteServ(user->fd,"482 %s :Can't change mode for other users",user->nick);
			return;
		}
		
		strcpy(outpars,"+");
		direction = 1;

		if ((parameters[1][0] != '+') && (parameters[1][0] != '-'))
			return;

		for (unsigned int i = 0; i < strlen(parameters[1]); i++)
		{
			if (parameters[1][i] == ' ')
				continue;
			if (parameters[1][i] == '+')
			{
				if (direction != 1)
				{
					int t = strlen(outpars)-1;
					if ((outpars[t] == '+') || (outpars[t] == '-'))
					{
						outpars[t] = '+';
					}
					else
					{
						strcat(outpars,"+");
					}
				}
				direction = 1;
			}
			else
			if (parameters[1][i] == '-')
			{
				if (direction != 0)
				{
					int t = strlen(outpars)-1;
					if ((outpars[t] == '+') || (outpars[t] == '-'))
					{
						outpars[t] = '-';
					}
					else
					{
						strcat(outpars,"-");
					}
				}
				direction = 0;
			}
			else
			{
				can_change = 0;
				if (strchr(user->modes,'o'))
				{
					can_change = 1;
				}
				else
				{
					if ((parameters[1][i] == 'i') || (parameters[1][i] == 'w') || (parameters[1][i] == 's') || (ModeGrok->AllowedUmode(parameters[1][i],user->modes,direction,false)))
					{
						can_change = 1;
					}
				}
				if (can_change)
				{
					if (direction == 1)
					{
						if ((!strchr(dmodes,parameters[1][i])) && (ModeGrok->AllowedUmode(parameters[1][i],user->modes,true,false)))
						{
							char umode = parameters[1][i];
							if ((ModeGrok->ProcessModuleUmode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								int q = strlen(dmodes);
								int r = strlen(outpars);
								dmodes[q+1]='\0';
								dmodes[q] = parameters[1][i];
								outpars[r+1]='\0';
								outpars[r] = parameters[1][i];
								if (parameters[1][i] == 'o')
								{
									FOREACH_MOD OnGlobalOper(dest);
								}
							}
						}
					}
					else
					{
						if ((ModeGrok->AllowedUmode(parameters[1][i],user->modes,false,false)) && (strchr(dmodes,parameters[1][i])))
						{
							char umode = parameters[1][i];
							if ((ModeGrok->ProcessModuleUmode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								unsigned int q = 0;
								char temp[MAXBUF];	
								char moo[MAXBUF];	

								unsigned int r = strlen(outpars);
								outpars[r+1]='\0';
								outpars[r] = parameters[1][i];
							
								strcpy(temp,"");
								for (q = 0; q < strlen(dmodes); q++)
								{
									if (dmodes[q] != parameters[1][i])
									{
										moo[0] = dmodes[q];
										moo[1] = '\0';
										strlcat(temp,moo,MAXBUF);
									}
								}
								strlcpy(dmodes,temp,52);

								if (umode == 'o')
									DeleteOper(dest);
							}
						}
					}
				}
			}
		}
		if (outpars[0])
		{
			char b[MAXBUF];
			strlcpy(b,"",MAXBUF);
			unsigned int z = 0;
			unsigned int i = 0;
			while (i < strlen (outpars))
			{
				b[z++] = outpars[i++];
				b[z] = '\0';
				if (i<strlen(outpars)-1)
				{
					if (((outpars[i] == '-') || (outpars[i] == '+')) && ((outpars[i+1] == '-') || (outpars[i+1] == '+')))
					{
						// someones playing silly buggers and trying
						// to put a +- or -+ into the line...
						i++;
					}
				}
				if (i == strlen(outpars)-1)
				{
					if ((outpars[i] == '-') || (outpars[i] == '+'))
					{
						i++;
					}
				}
			}

			z = strlen(b)-1;
			if ((b[z] == '-') || (b[z] == '+'))
				b[z] = '\0';

			if ((!b[0]) || (!strcmp(b,"+")) || (!strcmp(b,"-")))
				return;

			if (strcmp(b,""))
			{
				WriteTo(user, dest, "MODE %s :%s", dest->nick, b);
				FOREACH_MOD OnMode(user, dest, TYPE_USER, b);
			}

			if (strlen(dmodes)>MAXMODES)
			{
				dmodes[MAXMODES-1] = '\0';
			}
			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strlcpy(dest->modes,dmodes,52);

		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		if (pcnt == 1)
		{
			/* just /modes #channel */
			WriteServ(user->fd,"324 %s %s +%s",user->nick, Ptr->name, chanmodes(Ptr));
                        WriteServ(user->fd,"329 %s %s %d", user->nick, Ptr->name, Ptr->created);
			return;
		}
		else
		if (pcnt == 2)
		{
			char* mode = parameters[1];
			if (*mode == '+')
				mode++;
			int MOD_RESULT = 0;
                        FOREACH_RESULT(OnRawMode(user, Ptr, *mode, "", false, 0));
                        if (!MOD_RESULT)
                        {
				if (*mode == 'b')
				{

					for (BanList::iterator i = Ptr->bans.begin(); i != Ptr->bans.end(); i++)
					{
						WriteServ(user->fd,"367 %s %s %s %s %d",user->nick, Ptr->name, i->data, i->set_by, i->set_time);
					}
					WriteServ(user->fd,"368 %s %s :End of channel ban list",user->nick, Ptr->name);
					return;
				}
				if ((ModeDefined(*mode,MT_CHANNEL)) && (ModeIsListMode(*mode,MT_CHANNEL)))
				{
					// list of items for an extmode
					log(DEBUG,"Calling OnSendList for all modules, list output for mode %c",*mode);
					FOREACH_MOD OnSendList(user,Ptr,*mode);
					return;
				}
			}
		}

                if (((Ptr) && (!has_channel(user,Ptr))) && (!is_uline(user->server)))
                {
                        WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, Ptr->name);
                        return;
                }

		if (Ptr)
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(OnAccessCheck(user,NULL,Ptr,AC_GENERAL_MODE));
			
			if (MOD_RESULT == ACR_DENY)
				return;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if (cstatus(user,Ptr) < STATUS_HOP)
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, Ptr->name);
					return;
				}
			}

			ModeGrok->ProcessModes(parameters,user,Ptr,cstatus(user,Ptr),pcnt,false,false,false);
		}
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}




void ModeParser::ServerMode(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change;
	int direction = 1;
	char outpars[MAXBUF];

	dest = Find(parameters[0]);
	
	// fix: ChroNiCk found this - we cant use this as debug if its null!
	if (dest)
	{
		log(DEBUG,"server_mode on %s",dest->nick);
	}

	if ((dest) && (pcnt > 1))
	{
                std::string tidied = ModeGrok->CompressModes(parameters[1],false);
                parameters[1] = (char*)tidied.c_str();

		char dmodes[MAXBUF];
		strlcpy(dmodes,dest->modes,52);

		strcpy(outpars,"+");
		direction = 1;

		if ((parameters[1][0] != '+') && (parameters[1][0] != '-'))
			return;

		for (unsigned int i = 0; i < strlen(parameters[1]); i++)
		{
                        if (parameters[1][i] == ' ')
                                continue;
			if (parameters[1][i] == '+')
			{
				if (direction != 1)
				{
					int t = strlen(outpars)-1;
					if ((outpars[t] == '+') || (outpars[t] == '-'))
					{
						outpars[t] = '+';
					}
					else
					{
						strcat(outpars,"+");
					}
				}
				direction = 1;
			}
			else
			if (parameters[1][i] == '-')
			{
				if (direction != 0)
				{
					int t = strlen(outpars)-1;
					if ((outpars[t] == '+') || (outpars[t] == '-'))
					{
						outpars[t] = '-';
					}
					else
					{
						strcat(outpars,"-");
					}
				}
				direction = 0;
			}
			else
			{
				log(DEBUG,"begin mode processing entry");
				can_change = 1;
				if (can_change)
				{
					if (direction == 1)
					{
						log(DEBUG,"umode %c being added",parameters[1][i]);
						if ((!strchr(dmodes,parameters[1][i])) && (ModeGrok->AllowedUmode(parameters[1][i],user->modes,true,true)))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((ModeGrok->ProcessModuleUmode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								int v1 = strlen(dmodes);
								int v2 = strlen(outpars);
								dmodes[v1+1]='\0';
								dmodes[v1] = parameters[1][i];
								outpars[v2+1]='\0';
								outpars[v2] = parameters[1][i];
							}
						}
					}
					else
					{
						// can only remove a mode they already have
						log(DEBUG,"umode %c being removed",parameters[1][i]);
						if ((ModeGrok->AllowedUmode(parameters[1][i],user->modes,false,true)) && (strchr(dmodes,parameters[1][i])))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((ModeGrok->ProcessModuleUmode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								unsigned int q = 0;
								char temp[MAXBUF];
								char moo[MAXBUF];	

								unsigned int v1 = strlen(outpars);
								outpars[v1+1]='\0';
								outpars[v1] = parameters[1][i];
							
								strcpy(temp,"");
								for (q = 0; q < strlen(dmodes); q++)
								{
									if (dmodes[q] != parameters[1][i])
									{
										moo[0] = dmodes[q];
										moo[1] = '\0';
										strlcat(temp,moo,MAXBUF);
									}
								}
								strlcpy(dmodes,temp,52);
							}
						}
					}
				}
			}
		}
		if (outpars[0])
		{
			char b[MAXBUF];
			strlcpy(b,"",MAXBUF);
			unsigned int z = 0;
			unsigned int i = 0;
			while (i < strlen (outpars))
			{
				b[z++] = outpars[i++];
				b[z] = '\0';
				if (i<strlen(outpars)-1)
				{
					if (((outpars[i] == '-') || (outpars[i] == '+')) && ((outpars[i+1] == '-') || (outpars[i+1] == '+')))
					{
						// someones playing silly buggers and trying
						// to put a +- or -+ into the line...
						i++;
					}
				}
				if (i == strlen(outpars)-1)
				{
					if ((outpars[i] == '-') || (outpars[i] == '+'))
					{
						i++;
					}
				}
			}

			z = strlen(b)-1;
			if ((b[z] == '-') || (b[z] == '+'))
				b[z] = '\0';

			if ((!b[0]) || (!strcmp(b,"+")) || (!strcmp(b,"-")))
				return;

			if (strcmp(b,""))
			{
				WriteTo(user, dest, "MODE %s :%s", dest->nick, b);
				FOREACH_MOD OnMode(user, dest, TYPE_USER, b);
			}
			
			if (strlen(dmodes)>MAXMODES)
			{
				dmodes[MAXMODES-1] = '\0';
			}
			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strlcpy(dest->modes,dmodes,MAXMODES);

		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		ModeGrok->ProcessModes(parameters,user,Ptr,STATUS_OP,pcnt,true,false,false);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}
