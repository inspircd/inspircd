/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
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
#include <ext/hash_map>
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
extern InspIRCd* ServerInstance;
extern ServerConfig* Config;

extern time_t TIME;

userrec* ModeParser::SanityChecks(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		return NULL;
	}
	d = Find(dest);
	if (!d)
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	return d;
}

char* ModeParser::Grant(userrec *d,chanrec *chan,int MASK)
{
	if (!chan)
		return NULL;

	for (std::vector<ucrec*>::const_iterator i = d->chans.begin(); i != d->chans.end(); i++)
	{
		if (((ucrec*)(*i))->channel == chan)
		{
			if (((ucrec*)(*i))->uc_modes & MASK)
			{
				return NULL;
			}
			((ucrec*)(*i))->uc_modes = ((ucrec*)(*i))->uc_modes | MASK;
			switch (MASK)
			{
				case UCMODE_OP:
					((ucrec*)(*i))->channel->AddOppedUser(d);
				break;
				case UCMODE_HOP:
					((ucrec*)(*i))->channel->AddHalfoppedUser(d);
				break;
				case UCMODE_VOICE:
					((ucrec*)(*i))->channel->AddVoicedUser(d);
				break;
			}
			log(DEBUG,"grant: %s %s",((ucrec*)(*i))->channel->name,d->nick);
			return d->nick;
		}
	}
	return NULL;
}

char* ModeParser::Revoke(userrec *d,chanrec *chan,int MASK)
{
	if (!chan)
		return NULL;

	for (std::vector<ucrec*>::const_iterator i = d->chans.begin(); i != d->chans.end(); i++)
	{
		if (((ucrec*)(*i))->channel == chan)
		{
			if ((((ucrec*)(*i))->uc_modes & MASK) == 0)
			{
				return NULL;
			}
			((ucrec*)(*i))->uc_modes ^= MASK;
			switch (MASK)
			{
				case UCMODE_OP:
					((ucrec*)(*i))->channel->DelOppedUser(d);
				break;
				case UCMODE_HOP:
					((ucrec*)(*i))->channel->DelHalfoppedUser(d);
				break;
				case UCMODE_VOICE:
					((ucrec*)(*i))->channel->DelVoicedUser(d);
				break;
			}
			log(DEBUG,"revoke: %s %s",((ucrec*)(*i))->channel->name,d->nick);
			return d->nick;
		}
	}
	return NULL;
}

char* ModeParser::GiveOps(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);
	
	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_OP));
			
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
		}

		return this->Grant(d,chan,UCMODE_OP);
	}
	return NULL;
}

char* ModeParser::GiveHops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);
	
	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_HALFOP));
		
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
		}

		return this->Grant(d,chan,UCMODE_HOP);
	}
	return NULL;
}

char* ModeParser::GiveVoice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);
	
	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_VOICE));
			
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
		}

		return this->Grant(d,chan,UCMODE_VOICE);
	}
	return NULL;
}

char* ModeParser::TakeOps(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);
	
	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEOP));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!is_uline(user->server)) && (IS_LOCAL(user)))
				{
					WriteServ(user->fd,"482 %s %s :You are not a channel operator",user->nick, chan->name);
					return NULL;
				}
			}
		}

		return this->Revoke(d,chan,UCMODE_OP);
	}
	return NULL;
}

char* ModeParser::TakeHops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);
	
	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEHALFOP));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				/* Tweak by Brain suggested by w00t, allow a halfop to dehalfop themselves */
				if ((user != d) && ((status < STATUS_OP) && (!is_uline(user->server))))
				{
					WriteServ(user->fd,"482 %s %s :You are not a channel operator",user->nick, chan->name);
					return NULL;
				}
			}
		}

		return this->Revoke(d,chan,UCMODE_HOP);
	}
	return NULL;
}

char* ModeParser::TakeVoice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);

	if (d)	
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEVOICE));
			
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
		}

		return this->Revoke(d,chan,UCMODE_VOICE);
	}
	return NULL;
}

char* ModeParser::AddBan(userrec *user,char *dest,chanrec *chan,int status)
{
	BanItem b;
	int toomanyexclamation = 0;
	int toomanyat = 0;

	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		log(DEFAULT,"*** BUG *** AddBan was given an invalid parameter");
		return NULL;
	}

	for (char* i = dest; *i; i++)
	{
		if ((*i < 32) || (*i > 126))
		{
			return NULL;
		}
		else if (*i == '!')
		{
			toomanyexclamation++;
		}
		else if (*i == '@')
		{
			toomanyat++;
		}
	}

	if (toomanyexclamation != 1 || toomanyat != 1)
		/*
		 * this stops sillyness like n!u!u!u@h, though note that most
		 * ircds don't actually verify banmask validity. --w00t
		 */
		return NULL;

	long maxbans = GetMaxBans(chan->name);
	if ((unsigned)chan->bans.size() > (unsigned)maxbans)
	{
		WriteServ(user->fd,"478 %s %s :Channel ban list for %s is full (maximum entries for this channel is %d)",user->nick, chan->name,chan->name,maxbans);
		return NULL;
	}

	log(DEBUG,"AddBan: %s %s",chan->name,user->nick);

	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnAddBan,OnAddBan(user,chan,dest));
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
	strlcpy(b.data,dest,MAXBUF);
	if (*user->nick)
	{
		strlcpy(b.set_by,user->nick,NICKMAX-1);
	}
	else
	{
		strlcpy(b.set_by,Config->ServerName,NICKMAX-1);
	}
	chan->bans.push_back(b);
	return dest;
}

char* ModeParser::TakeBan(userrec *user,char *dest,chanrec *chan,int status)
{
	if ((!user) || (!dest) || (!chan) || (!*dest)) {
		log(DEFAULT,"*** BUG *** TakeBan was given an invalid parameter");
		return 0;
	}

	log(DEBUG,"del_ban: %s %s",chan->name,user->nick);
	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnDelBan,OnDelBan(user,chan,dest));
			if (MOD_RESULT)
				return NULL;
			chan->bans.erase(i);
			return dest;
		}
	}
	return NULL;
}


/** ModeParser::CompressModes()
 * Tidies up redundant modes,
 * e.g. +nt-nt+i becomes +-+i
 * A section further down the chain tidies up the +-+- crap.
 */
std::string ModeParser::CompressModes(std::string modes,bool channelmodes)
{
	/*
	 * OK, iterate over the mode string and count how many times a certain mode appears in it.
	 * Then, erase all instances of any character that appears more than once.
	 * This only operates on modes with no parameters, you can still +v-v+v-v+v-v to your heart's content.
	 */
	
	/* Do we really need an int here? Can you fit enough modes in a line to overflow a short? */
	short counts[127];
	bool active[127];
	memset(counts, 0, sizeof(counts));
	memset(active, 0, sizeof(active));
	
	for(unsigned char* i = (unsigned char*)modes.c_str(); *i; i++)
	{
		if((*i == '+') || (*i == '-'))
			continue;

		if(!channelmodes || (channelmodes && (strchr("itnmsp", *i) || (ModeDefined(*i, MT_CHANNEL) && !ModeDefinedOn(*i,MT_CHANNEL) && !ModeDefinedOff(*i,MT_CHANNEL)))))
		{
			log(DEBUG,"Tidy mode %c", *i);
			counts[*i]++;
			active[*i] = true;
		}
	}
	
	for(unsigned char j = 65; j < 127; j++)
	{
		if ((counts[j] > 1) && (active[j] == true))
		{
			std::string::size_type pos;

			while((pos = modes.find(j)) != std::string::npos)
			{
				log(DEBUG, "Deleting occurence of mode %c...", j);
				modes.erase(pos, 1);
				log(DEBUG,"New mode line: %s", modes.c_str());
			}
		}
	}
	
	return modes;
}

void ModeParser::ProcessModes(char **parameters,userrec* user,chanrec *chan,int status, int pcnt, bool servermode, bool silent, bool local)
{
	if ((!parameters) || (pcnt < 2)) {
		return;
	}

	char outlist[MAXBUF];
	char mlist[MAXBUF];
	char *outpars[32];
	int param = 2;
	int pc = 0;
	int ptr = 0;
	int mdir = 1;
	char* r = NULL;
	bool k_set = false, l_set = false, previously_set_l = false, previously_unset_l = false, previously_set_k = false, previously_unset_k = false;

	int MOD_RESULT = 0;
	
	if (IS_LOCAL(user))
	{
		FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,NULL,chan,AC_GENERAL_MODE));	
		if (MOD_RESULT == ACR_DENY)
			return;
	}

	std::string tidied = this->CompressModes(parameters[1],true);
	strlcpy(mlist,tidied.c_str(),MAXBUF);
	char* modelist = mlist;

	*outlist = *modelist;
	char* outl = outlist+1;

	mdir = (*modelist == '+');

	log(DEBUG,"process_modes: modelist: %s",modelist);

	/* Related to BUG #127, can cause stack corruption if we allow an empty modestring */
	if (!*modelist)
		return;

	int len = tidied.length();
	while (modelist[len-1] == ' ')
		modelist[--len] = '\0';

	for (char* modechar = (modelist + 1); *modechar; ptr++, modechar++)
	{
		r = NULL;

		/* If we have more than MAXMODES changes in one line,
		 * drop all after the MAXMODES
		 */
		if (pc > MAXMODES-1)
			break;


		
		switch (*modechar)
		{
			case '-':
				*outl++ = '-';
				mdir = 0;
			break;			

			case '+':
				*outl++ = '+';
				mdir = 1;
			break;

			case 'o':
				log(DEBUG,"Ops");
				if ((param >= pcnt)) break;
				log(DEBUG,"Enough parameters left");
				r = NULL;
				if (mdir == 1)
				{
					MOD_RESULT = 0;
					FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'o', parameters[param], true, 1));
					if (!MOD_RESULT)
					{
						r = GiveOps(user,parameters[param++],chan,status);
					}
					else param++;
				}
				else
				{
					MOD_RESULT = 0;
					FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'o', parameters[param], false, 1));
					if (!MOD_RESULT)
					{
						r = TakeOps(user,parameters[param++],chan,status);
					}
					else param++;
				}
				if (r)
				{
					*outl++ = 'o';
					outpars[pc++] = r;
				}
			break;
			
			case 'h':
				if (((param >= pcnt)) || (!Config->AllowHalfop)) break;
				r = NULL;
				if (mdir == 1)
				{
					MOD_RESULT = 0;
					FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'h', parameters[param], true, 1));
					if (!MOD_RESULT)
					{
						r = GiveHops(user,parameters[param++],chan,status);
					}
					else param++;
				}
				else
				{
					MOD_RESULT = 0;
					FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'h', parameters[param], false, 1));
					if (!MOD_RESULT)
					{
						r = TakeHops(user,parameters[param++],chan,status);
					}
					else param++;
				}
				if (r)
				{
					*outl++ = 'h';
					outpars[pc++] = r;
				}
			break;
			
				
			case 'v':
					if ((param >= pcnt)) break;
					r = NULL;
					if (mdir == 1)
					{
						MOD_RESULT = 0;
						FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'v', parameters[param], true, 1));
						if (!MOD_RESULT)
						{
							r = GiveVoice(user,parameters[param++],chan,status);
						}
						else param++;
					}
					else
					{
						MOD_RESULT = 0;
						FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'v', parameters[param], false, 1));
						if (!MOD_RESULT)
						{
							r = TakeVoice(user,parameters[param++],chan,status);
						}
						else param++;
					}
					if (r)
					{
						*outl++ = 'v';
						outpars[pc++] = r;
					}
			break;
				
			case 'b':
				if ((param >= pcnt)) break;
				r = NULL;
				if (mdir == 1)
				{
					MOD_RESULT = 0;
					FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'b', parameters[param], true, 1));
					if (!MOD_RESULT)
					{
						r = AddBan(user,parameters[param++],chan,status);
					}
					else param++;
				}
				else
				{
					MOD_RESULT = 0;
					FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'b', parameters[param], false, 1));
					if (!MOD_RESULT)
					{
						r = TakeBan(user,parameters[param++],chan,status);
					}
					else param++;
				}
				if (r)
				{
					*outl++ = 'b';
					outpars[pc++] = parameters[param-1];
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
						
					if (!chan->modes[CM_KEY])
					{
						MOD_RESULT = 0;
						FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'k', parameters[param], true, 1));
						if (!MOD_RESULT)
						{
							*outl++ = 'k';
							char key[MAXBUF];
							strlcpy(key,parameters[param++],32);
							outpars[pc++] = key;
							strlcpy(chan->key,key,MAXBUF);
							chan->modes[CM_KEY] = 1;
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
					FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'k', parameters[param], false, 1));
					if (!MOD_RESULT)
					{
						strlcpy(key,parameters[param++],32);
						/* only allow -k if correct key given */
						if (!strcmp(chan->key,key))
						{
							*outl++ = 'k';
							*chan->key = 0;
							chan->modes[CM_KEY] = 0;
							outpars[pc++] = key;
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
					FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'l', "", false, 0));
					if (!MOD_RESULT)
					{
						if (chan->modes[CM_LIMIT])
						{
							*outl++ = 'l';
							chan->limit = 0;
							chan->modes[CM_LIMIT] = 0;
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
					for (char* f = parameters[param]; *f; f++)
					{
						if ((*f < '0') || (*f > '9'))
						{
							invalid = true;
						}
					}
					/* If the limit is < 1, or the new limit is the current limit, dont allow */
					if ((atoi(parameters[param]) < 1) || ((chan->limit > 0) && (atoi(parameters[param]) == chan->limit)))
					{
						invalid = true;
					}

					if (invalid)
						break;

					MOD_RESULT = 0;
					FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'l', parameters[param], true, 1));
					if (!MOD_RESULT)
					{
	
						chan->limit = atoi(parameters[param]);
							
						// reported by mech: large values cause underflow
						if (chan->limit < 0)
							chan->limit = 0x7FFF;
					}
						
					if (chan->limit)
					{
						*outl++ = 'l';
						chan->modes[CM_LIMIT] = 1;
						outpars[pc++] = parameters[param++];
						l_set = true;
					}
				}
			break;
				
			case 'i':
				MOD_RESULT = 0;
				FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'i', "", mdir, 0));
				if (!MOD_RESULT)
				{
					if (mdir)
					{
						if (!(chan->modes[CM_INVITEONLY])) *outl++ = 'i';
						chan->modes[CM_INVITEONLY] = 1;
					}
					else
					{
						if (chan->modes[CM_INVITEONLY]) *outl++ = 'i';
						chan->modes[CM_INVITEONLY] = 0;
					}
				}
			break;
				
			case 't':
				MOD_RESULT = 0;
				FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 't', "", mdir, 0));
				if (!MOD_RESULT)
				{
					if (mdir)
					{
						if (!(chan->modes[CM_TOPICLOCK])) *outl++ = 't';
						chan->modes[CM_TOPICLOCK] = 1;
					}
					else
					{
						if (chan->modes[CM_TOPICLOCK]) *outl++ = 't';
						chan->modes[CM_TOPICLOCK] = 0;
					}
				}
			break;
				
			case 'n':
				MOD_RESULT = 0;
				FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'n', "", mdir, 0));
				if (!MOD_RESULT)
				{
					if (mdir)
					{
						if (!(chan->modes[CM_NOEXTERNAL])) *outl++ = 'n';
						chan->modes[CM_NOEXTERNAL] = 1;
					}
					else
					{
						if (chan->modes[CM_NOEXTERNAL]) *outl++ = 'n';
						chan->modes[CM_NOEXTERNAL] = 0;
					}
				}
			break;
				
			case 'm':
				MOD_RESULT = 0;
				FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'm', "", mdir, 0));
				if (!MOD_RESULT)
				{
					if (mdir)
					{
						if (!(chan->modes[CM_MODERATED])) *outl++ = 'm';
						chan->modes[CM_MODERATED] = 1;
					}
					else
					{
						if (chan->modes[CM_MODERATED]) *outl++ = 'm';
						chan->modes[CM_MODERATED] = 0;
					}
				}
			break;
				
			case 's':
				MOD_RESULT = 0;
				FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 's', "", mdir, 0));
				if (!MOD_RESULT)
				{
					if (mdir)
					{
						if (!(chan->modes[CM_SECRET])) *outl++ = 's';
						chan->modes[CM_SECRET] = 1;
						if (chan->modes[CM_PRIVATE])
						{
							chan->modes[CM_PRIVATE] = 0;
							if (mdir)
							{
								*outl++ = '-'; *outl++ = 'p'; *outl++ = '+';
							}
						}
					}
					else
					{
						if (chan->modes[CM_SECRET]) *outl++ = 's';
						chan->modes[CM_SECRET] = 0;
					}
				}
			break;
				
			case 'p':
				MOD_RESULT = 0;
				FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, 'p', "", mdir, 0));
				if(!MOD_RESULT)
				{
					if(mdir)
					{
						if(!(chan->modes[CM_PRIVATE]))
							*outl++ = 'p';
						
						chan->modes[CM_PRIVATE] = 1;
						
						if(chan->modes[CM_SECRET])
						{
							chan->modes[CM_SECRET] = 0;

							*outl++ = '-';
							*outl++ = 's';
							*outl++ = '+';
						}
					}
					else
					{
						if(chan->modes[CM_PRIVATE])
							*outl++ = 'p';
						
						chan->modes[CM_PRIVATE] = 0;
					}
				}
				break;
				
			default:
				string_list p;
				p.clear();
				bool x = chan->modes[*modechar-65];
				if ((!x && !mdir) || (x && mdir))
				{
					if (!ModeIsListMode(*modechar,MT_CHANNEL))
					{
						log(DEBUG,"Mode %c isnt set on %s but trying to remove!",*modechar,chan->name);
						break;
					}
				}
				if (ModeDefined(*modechar,MT_CHANNEL))
				{
					/* A module has claimed this mode */
					if (param<pcnt)
					{
						if ((ModeDefinedOn(*modechar,MT_CHANNEL)>0) && (mdir))
						{
							p.push_back(parameters[param]);
						}
						if ((ModeDefinedOff(*modechar,MT_CHANNEL)>0) && (!mdir))
						{
							p.push_back(parameters[param]);
						}
					}
					bool handled = false;
					if (param>=pcnt)
					{
						// we're supposed to have a parameter, but none was given... so dont handle the mode.
						if (((ModeDefinedOn(*modechar,MT_CHANNEL)>0) && (mdir)) || ((ModeDefinedOff(*modechar,MT_CHANNEL)>0) && (!mdir)))	
						{
							log(DEBUG,"Not enough parameters for module-mode %c",*modechar);
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
					
					FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, *modechar, para, mdir, pcnt));
					if(!MOD_RESULT)
					{
 						for (int i = 0; i <= MODCOUNT; i++)
						{
							if (!handled)
							{
								int t = modules[i]->OnExtendedMode(user,chan,*modechar,MT_CHANNEL,mdir,p);
								if (t != 0)
								{
									log(DEBUG,"OnExtendedMode returned nonzero for a module");
									if (ModeIsListMode(*modechar,MT_CHANNEL))
									{
										if (t == -1)
										{
											//pc++;
											param++;
										}
										else
										{
											if (param < pcnt)
											{
												*outl++ = *modechar;
											}
											outpars[pc++] = parameters[param++];
										}
									}
									else
									{
										*outl++ = *modechar;
										chan->SetCustomMode(*modechar,mdir);
										// include parameters in output if mode has them
										if ((ModeDefinedOn(*modechar,MT_CHANNEL)>0) && (mdir))
										{
											if (param < pcnt)
											{
												chan->SetCustomModeParam(*modechar,parameters[param],mdir);
												outpars[pc++] = parameters[param++];
											}
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
					WriteServ(user->fd,"472 %s %c :is unknown mode char to me",user->nick,*modechar);
				}
			break;
		}
	}

	/* Null terminate it now we're done */
	*outl = 0;


	/************ Fast, but confusing string tidying ************/
	outl = outlist;
	while (*outl && (*outl < 'A'))
		outl++;
	/* outl now points to the first mode character after +'s and -'s */
	outl--;
	/* Now points at first mode-modifier + or - symbol */
	char* trim = outl;
	/* Now we tidy off any trailing -'s etc */
	while (*trim++);
	trim--;
	while ((*--trim == '+') || (*trim == '-'))
		*trim = 0;
	/************ Done wih the string tidy functions ************/


	/* The mode change must be at least two characters long (+ or - and at least one mode) */
	if (((*outl == '+') || (*outl == '-')) && *(outl+1))
	{
		for (ptr = 0; ptr < pc; ptr++)
		{
			charlcat(outl,' ',MAXBUF);
			strlcat(outl,outpars[ptr],MAXBUF-1);
		}
		if (local)
		{
			log(DEBUG,"Local mode change");
			WriteChannelLocal(chan, user, "MODE %s %s",chan->name,outl);
			FOREACH_MOD(I_OnMode,OnMode(user, chan, TYPE_CHANNEL, outl));
		}
		else
		{
			if (servermode)
			{
				if (!silent)
				{
					WriteChannelWithServ(Config->ServerName,chan,"MODE %s %s",chan->name,outl);
				}
					
			}
			else
			{
				if (!silent)
				{
					WriteChannel(chan,user,"MODE %s %s",chan->name,outl);
					FOREACH_MOD(I_OnMode,OnMode(user, chan, TYPE_CHANNEL, outl));
				}
			}
		}
	}
}

// based on sourcemodes, return true or false to determine if umode is a valid mode a user may set on themselves or others.

bool ModeParser::AllowedUmode(char umode, char* sourcemodes,bool adding,bool serveroverride)
{
	bool sourceoper = (strchr(sourcemodes,'o') != NULL);
	log(DEBUG,"Allowed_umode: %c %s",umode,sourcemodes);
	// Servers can +o and -o arbitrarily
	if ((serveroverride == true) && (umode == 'o'))
	{
		return true;
	}
	// RFC1459 specified modes
	if ((umode == 'w') || (umode == 's') || (umode == 'i'))
	{
		/* umode allowed by RFC1459 scemantics */
		return true;
	}
	
	/* user may not +o themselves or others, but an oper may de-oper other opers or themselves */
	if (sourceoper && !adding)
	{
		return true;
	}
	else if (umode == 'o')
	{
		/* Bad oper, bad bad! */
		return false;
	}
	
	/* process any module-defined modes that need oper */
	if ((ModeDefinedOper(umode,MT_CLIENT)) && (sourceoper))
	{
		log(DEBUG,"umode %c allowed by module handler (oper only mode)",umode);
		return true;
	}
	else if (ModeDefined(umode,MT_CLIENT))
	{
		// process any module-defined modes that don't need oper
		log(DEBUG,"umode %c allowed by module handler (non-oper mode)",umode);
		if ((ModeDefinedOper(umode,MT_CLIENT)) && (!sourceoper))
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
		strlcpy(s2->nick,Config->ServerName,NICKMAX-1);
		*s2->modes = 'o';
		*(s2->modes+1) = 0;
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

void cmd_mode::Handle (char **parameters, int pcnt, userrec *user)
{
	chanrec* chan;
	userrec* dest = Find(parameters[0]);
	int MOD_RESULT;
	int can_change;
	int direction = 1;
	char outpars[MAXBUF];
	bool next_ok = true;

	if (!user)
		return;

	if ((dest) && (pcnt == 1))
	{
		WriteServ(user->fd,"221 %s :+%s",dest->nick,dest->modes);
		return;
	}
	else if ((dest) && (pcnt > 1))
	{
		std::string tidied = ServerInstance->ModeGrok->CompressModes(parameters[1],false);
		parameters[1] = (char*)tidied.c_str();

		char dmodes[MAXBUF];
		strlcpy(dmodes,dest->modes,MAXMODES);
		log(DEBUG,"pulled up dest user modes: %s",dmodes);

		can_change = 0;
		if (user != dest)
		{
			if ((*user->oper) || (is_uline(user->server)))
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
		
		outpars[0] = *parameters[1];
		outpars[1] = 0;
		direction = (*parameters[1] == '+');

		if ((*parameters[1] != '+') && (*parameters[1] != '-'))
			return;

		for (char* i = parameters[1]; *i; i++)
		{
			if ((i != parameters[1]) && (*i != '+') && (*i != '-'))
				next_ok = true;

			switch (*i)
			{
				case ' ':
				continue;

				case '+':
					if ((direction != 1) && (next_ok))
					{
						charlcat(outpars,'+',MAXBUF);
						next_ok = false;
					}	
					direction = 1;
				break;

				case '-':
					if ((direction != 0) && (next_ok))
					{
						charlcat(outpars,'-',MAXBUF);
						next_ok = false;
					}
					direction = 0;
				break;

				default:
					can_change = 0;
					if (*user->oper)
					{
						can_change = 1;
					}
					else
					{
						if ((*i == 'i') || (*i == 'w') || (*i == 's') || (ServerInstance->ModeGrok->AllowedUmode(*i,user->modes,direction,false)))
						{
							can_change = 1;
						}
					}
					if (can_change)
					{
						if (direction == 1)
						{
							if ((!strchr(dmodes,*i)) && (ServerInstance->ModeGrok->AllowedUmode(*i,user->modes,true,false)))
							{
								if ((ServerInstance->ModeGrok->ProcessModuleUmode(*i, user, dest, direction)) || (*i == 'i') || (*i == 's') || (*i == 'w') || (*i == 'o'))
								{
									charlcat(dmodes,*i,53);
									charlcat(outpars,*i,MAXMODES);
									switch (*i)
									{
										case 'o':
											FOREACH_MOD(I_OnGlobalOper,OnGlobalOper(dest));
										break;
										case 'i':
											dest->modebits |= UM_INVISIBLE;
										break;
										case 's':
											dest->modebits |= UM_SERVERNOTICE;
										break;
										case 'w':
											dest->modebits |= UM_WALLOPS;
										break;
										default:
										break;
									}
								}
							}
						}
						else
						{
							if ((ServerInstance->ModeGrok->AllowedUmode(*i,user->modes,false,false)) && (strchr(dmodes,*i)))
							{
								if ((ServerInstance->ModeGrok->ProcessModuleUmode(*i, user, dest, direction)) || (*i == 'i') || (*i == 's') || (*i == 'w') || (*i == 'o'))
								{
									charlcat(outpars,*i,MAXMODES);
									charremove(dmodes,*i);
									switch (*i)
									{
										case 'o':
											*dest->oper = 0;
											DeleteOper(dest);
										break;
										case 'i':
											dest->modebits &= ~UM_INVISIBLE;
										break;
										case 's':
											dest->modebits &= ~UM_SERVERNOTICE;
										break;
										case 'w':
											dest->modebits &= ~UM_WALLOPS;
										break;
										default:
										break;
									}
								}
							}
						}
					}
				break;
			}
		}
		if (*outpars)
		{
			char b[MAXBUF];
			char* z = b;

			for (char* i = outpars; *i;)
			{
				*z++ = *i++;
				if (((*i == '-') || (*i == '+')) && ((*(i+1) == '-') || (*(i+1) == '+')))
				{
					// someones playing silly buggers and trying
					// to put a +- or -+ into the line...
					i++;
				}
				if (!*(i+1))
				{
					// Someone's trying to make the last character in
					// the line be a + or - symbol.
					if ((*i == '-') || (*i == '+'))
					{
						i++;
					}
				}
			}
			*z = 0;

			if ((*b) && (!IS_SINGLE(b,'+')) && (!IS_SINGLE(b,'-')))
			{
				WriteTo(user, dest, "MODE %s :%s", dest->nick, b);
				FOREACH_MOD(I_OnMode,OnMode(user, dest, TYPE_USER, b));
			}

			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strlcpy(dest->modes,dmodes,MAXMODES-1);

		}

		return;
	}
	else
	{
		chan = FindChan(parameters[0]);
		if(chan)
		{
			if (pcnt == 1)
			{
				/* just /modes #channel */
				WriteServ(user->fd,"324 %s %s +%s",user->nick, chan->name, chanmodes(chan, chan->HasUser(user)));
				WriteServ(user->fd,"329 %s %s %d", user->nick, chan->name, chan->created);
				return;
			}
			else if (pcnt == 2)
			{
				char* mode = parameters[1];
				
				MOD_RESULT = 0;
				
				if (*mode == '+')
					mode++;

				FOREACH_RESULT(I_OnRawMode,OnRawMode(user, chan, *mode, "", false, 0));
				if(!MOD_RESULT)
				{
					if (*mode == 'b')
					{
						for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
						{
							WriteServ(user->fd,"367 %s %s %s %s %d",user->nick, chan->name, i->data, i->set_by, i->set_time);
						}
						WriteServ(user->fd,"368 %s %s :End of channel ban list",user->nick, chan->name);
						return;
					}
					
					if ((ModeDefined(*mode,MT_CHANNEL)) && (ModeIsListMode(*mode,MT_CHANNEL)))
					{
						// list of items for an extmode
						log(DEBUG,"Calling OnSendList for all modules, list output for mode %c",*mode);
						FOREACH_MOD(I_OnSendList,OnSendList(user,chan,*mode));
						return;
					}
				}
			}

			if ((IS_LOCAL(user)) && (!is_uline(user->server)) && (!chan->HasUser(user)))
			{
				WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, chan->name);
				return;
			}
	
			MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user, NULL, chan, AC_GENERAL_MODE));
				
			if(MOD_RESULT == ACR_DENY)
				return;

			if(MOD_RESULT == ACR_DEFAULT)
			{
				if ((IS_LOCAL(user)) && (cstatus(user,chan) < STATUS_HOP))
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
					return;
				}
			}
	
			ServerInstance->ModeGrok->ProcessModes(parameters,user,chan,cstatus(user,chan),pcnt,false,false,false);
		}
		else
		{
			WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}
	}
}




void ModeParser::ServerMode(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	userrec* dest = Find(parameters[0]);
	int can_change;
	int direction = 1;
	char outpars[MAXBUF];
	bool next_ok = true;

	if ((dest) && (pcnt > 1))
	{
		std::string tidied = ServerInstance->ModeGrok->CompressModes(parameters[1],false);
		parameters[1] = (char*)tidied.c_str();

		char dmodes[MAXBUF];
		strlcpy(dmodes,dest->modes,MAXBUF);

		outpars[0] = *parameters[1];
		outpars[1] = 0;
		direction = (*parameters[1] == '+');

		if ((*parameters[1] != '+') && (*parameters[1] != '-'))
			return;

		for (char* i = parameters[1]; *i; i++)
		{
			if ((i != parameters[1]) && (*i != '+') && (*i != '-'))
				next_ok = true;

			switch (*i)
			{
				case ' ':
				continue;

				case '+':
					if ((direction != 1) && (next_ok))
					{
						next_ok = false;
						charlcat(outpars,'+',MAXBUF);
					}
					direction = 1;
				break;

				case '-':
					if ((direction != 0) && (next_ok))
					{
						next_ok = false;
						charlcat(outpars,'-',MAXBUF);
					}
					direction = 0;
				break;

				default:
					log(DEBUG,"begin mode processing entry");
					can_change = 1;
					if (can_change)
					{
						if (direction == 1)
						{
							log(DEBUG,"umode %c being added",*i);
							if ((!strchr(dmodes,*i)) && (ServerInstance->ModeGrok->AllowedUmode(*i,user->modes,true,true)))
							{
								log(DEBUG,"umode %c is an allowed umode",*i);
								if ((*i == 'i') || (*i == 's') || (*i == 'w') || (*i == 'o') || (ServerInstance->ModeGrok->ProcessModuleUmode(*i, user, dest, direction)))
								{
									charlcat(dmodes,*i,MAXBUF);
									charlcat(outpars,*i,53);
									switch (*i)
									{
										case 'i':
											dest->modebits |= UM_INVISIBLE;
										break;
										case 's':
											dest->modebits |= UM_SERVERNOTICE;
										break;
										case 'w':
											dest->modebits |= UM_WALLOPS;
										break;
										default:
										break;
									}
								}
							}
						}
						else
						{
							// can only remove a mode they already have
							log(DEBUG,"umode %c being removed",*i);
							if ((ServerInstance->ModeGrok->AllowedUmode(*i,user->modes,false,true)) && (strchr(dmodes,*i)))
							{
								log(DEBUG,"umode %c is an allowed umode",*i);
								if ((*i == 'i') || (*i == 's') || (*i == 'w') || (*i == 'o') || (ServerInstance->ModeGrok->ProcessModuleUmode(*i, user, dest, direction)))
								{
									charlcat(outpars,*i,MAXBUF);
									charremove(dmodes,*i);
									switch (*i)
									{
										case 'i':
											dest->modebits &= ~UM_INVISIBLE;
										break;
										case 's':
											dest->modebits &= ~UM_SERVERNOTICE;
										break;
										case 'w':
											dest->modebits &= ~UM_WALLOPS;
										break;
										default:
										break;
									}
								}
							}
						}
					}
				break;
			}
		}
		if (*outpars)
		{
			char b[MAXBUF];
			char* z = b;

			for (char* i = outpars; *i;)
			{
				*z++ = *i++;
				if (((*i == '-') || (*i == '+')) && ((*(i+1) == '-') || (*(i+1) == '+')))
				{
					// someones playing silly buggers and trying
					// to put a +- or -+ into the line...
					i++;
				}
				if (!*(i+1))
				{
					// Someone's trying to make the last character in
					// the line be a + or - symbol.
					if ((*i == '-') || (*i == '+'))
					{
						i++;
					}
				}
			}
			*z = 0;

			if ((*b) && (!IS_SINGLE(b,'+')) && (!IS_SINGLE(b,'-')))
			{
				WriteTo(user, dest, "MODE %s :%s", dest->nick, b);
				FOREACH_MOD(I_OnMode,OnMode(user, dest, TYPE_USER, b));
			}

			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strlcpy(dest->modes,dmodes,MAXMODES-1);
					 
		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		ServerInstance->ModeGrok->ProcessModes(parameters,user,Ptr,STATUS_OP,pcnt,true,false,false);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}
