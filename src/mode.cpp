#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
#include "inspircd_config.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <cstdio>
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
#include <errno.h>
#include <deque>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include "connection.h"
#include "users.h"
#include "servers.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "xline.h"

using namespace std;

extern int MODCOUNT;
extern vector<Module*> modules;
extern vector<ircd_module*> factory;

extern int LogLevel;
extern char ServerName[MAXBUF];
extern char Network[MAXBUF];
extern char ServerDesc[MAXBUF];
extern char AdminName[MAXBUF];
extern char AdminEmail[MAXBUF];
extern char AdminNick[MAXBUF];
extern char diepass[MAXBUF];
extern char restartpass[MAXBUF];
extern char motd[MAXBUF];
extern char rules[MAXBUF];
extern char list[MAXBUF];
extern char PrefixQuit[MAXBUF];
extern char DieValue[MAXBUF];

int give_ops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** give_ops was given an invalid parameter");
		return 0;
	}
	if ((status < STATUS_OP) && (!is_uline(user->server)))
	{
		log(DEBUG,"%s cant give ops to %s because they nave status %d and needs %d",user->nick,dest,status,STATUS_OP);
		WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
		return 0;
	}
	else
	{
		if (!isnick(dest))
		{
			log(DEFAULT,"the target nickname given to give_ops was invalid");
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		d = Find(dest);
		if (!d)
		{
			log(DEFAULT,"the target nickname given to give_ops couldnt be found");
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if (d->chans[i].uc_modes & UCMODE_OP)
					{
						/* mode already set on user, dont allow multiple */
						log(DEFAULT,"The target user given to give_ops was already opped on the channel");
						return 0;
					}
					d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_OP;
					log(DEBUG,"gave ops: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
			log(DEFAULT,"The target channel given to give_ops was not in the users mode list");
		}
	}
	return 1;
}

int give_hops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** give_hops was given an invalid parameter");
		return 0;
	}
	if ((status < STATUS_OP) && (!is_uline(user->server)))
	{
		WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
		return 0;
	}
	else
	{
		d = Find(dest);
		if (!isnick(dest))
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		if (!d)
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if (d->chans[i].uc_modes & UCMODE_HOP)
					{
						/* mode already set on user, dont allow multiple */
						return 0;
					}
					d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_HOP;
					log(DEBUG,"gave h-ops: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
		}
	}
	return 1;
}

int give_voice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** give_voice was given an invalid parameter");
		return 0;
	}
	if ((status < STATUS_HOP) && (!is_uline(user->server)))
	{
		WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
		return 0;
	}
	else
	{
		d = Find(dest);
		if (!isnick(dest))
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		if (!d)
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if (d->chans[i].uc_modes & UCMODE_VOICE)
					{
						/* mode already set on user, dont allow multiple */
						return 0;
					}
					d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_VOICE;
					log(DEBUG,"gave voice: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
		}
	}
	return 1;
}

int take_ops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** take_ops was given an invalid parameter");
		return 0;
	}
	if ((status < STATUS_OP) && (!is_uline(user->server)))
	{
		log(DEBUG,"%s cant give ops to %s because they have status %d and needs %d",user->nick,dest,status,STATUS_OP);
		WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
		return 0;
	}
	else
	{
		d = Find(dest);
		if (!isnick(dest))
		{
			log(DEBUG,"take_ops was given an invalid target nickname of %s",dest);
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		if (!d)
		{
			log(DEBUG,"take_ops couldnt resolve the target nickname: %s",dest);
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if ((d->chans[i].uc_modes & UCMODE_OP) == 0)
					{
						/* mode already set on user, dont allow multiple */
						return 0;
					}
					d->chans[i].uc_modes ^= UCMODE_OP;
					log(DEBUG,"took ops: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
			log(DEBUG,"take_ops couldnt locate the target channel in the target users list");
		}
	}
	return 1;
}

int take_hops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** take_hops was given an invalid parameter");
		return 0;
	}
	if ((status < STATUS_OP) && (!is_uline(user->server)))
	{
		WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
		return 0;
	}
	else
	{
		d = Find(dest);
		if (!isnick(dest))
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		if (!d)
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if ((d->chans[i].uc_modes & UCMODE_HOP) == 0)
					{
						/* mode already set on user, dont allow multiple */
						return 0;
					}
					d->chans[i].uc_modes ^= UCMODE_HOP;
					log(DEBUG,"took h-ops: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
		}
	}
	return 1;
}

int take_voice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** take_voice was given an invalid parameter");
		return 0;
	}
	if ((status < STATUS_HOP) && (!is_uline(user->server)))
	{
		WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
		return 0;
	}
	else
	{
		d = Find(dest);
		if (!isnick(dest))
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		if (!d)
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if ((d->chans[i].uc_modes & UCMODE_VOICE) == 0)
					{
						/* mode already set on user, dont allow multiple */
						return 0;
					}
					d->chans[i].uc_modes ^= UCMODE_VOICE;
					log(DEBUG,"took voice: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
		}
	}
	return 1;
}

int add_ban(userrec *user,char *dest,chanrec *chan,int status)
{
	if ((!user) || (!dest) || (!chan)) {
		log(DEFAULT,"*** BUG *** add_ban was given an invalid parameter");
		return 0;
	}

	BanItem b;
	if ((!user) || (!dest) || (!chan))
		return 0;
	if (strchr(dest,'!')==0)
		return 0;
	if (strchr(dest,'@')==0)
		return 0;
	for (int i = 0; i < strlen(dest); i++)
		if (dest[i] < 32)
			return 0;
	for (int i = 0; i < strlen(dest); i++)
		if (dest[i] > 126)
			return 0;
	int c = 0;
	for (int i = 0; i < strlen(dest); i++)
		if (dest[i] == '!')
			c++;
	if (c>1)
		return 0;
	c = 0;
	for (int i = 0; i < strlen(dest); i++)
		if (dest[i] == '@')
			c++;
	if (c>1)
		return 0;
	log(DEBUG,"add_ban: %s %s",chan->name,user->nick);

	TidyBan(dest);
	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest))
		{
			// dont allow a user to set the same ban twice
			return 0;
		}
	}

	b.set_time = time(NULL);
	strncpy(b.data,dest,MAXBUF);
	strncpy(b.set_by,user->nick,NICKMAX);
	chan->bans.push_back(b);
	return 1;
}

int take_ban(userrec *user,char *dest,chanrec *chan,int status)
{
	if ((!user) || (!dest) || (!chan)) {
		log(DEFAULT,"*** BUG *** take_ban was given an invalid parameter");
		return 0;
	}

	log(DEBUG,"del_ban: %s %s",chan->name,user->nick);
	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest))
		{
			chan->bans.erase(i);
			return 1;
		}
	}
	return 0;
}

void process_modes(char **parameters,userrec* user,chanrec *chan,int status, int pcnt, bool servermode, bool silent, bool local)
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
	int r = 0;
	bool k_set = false, l_set = false;

	if (pcnt < 2)
	{
		return;
	}

	log(DEBUG,"process_modes: start: parameters=%d",pcnt);

	strcpy(modelist,parameters[1]); /* mode list, e.g. +oo-o */
					/* parameters[2] onwards are parameters for
					 * modes that require them :) */
	strcpy(outlist,"+");
	mdir = 1;

	log(DEBUG,"process_modes: modelist: %s",modelist);

	for (ptr = 0; ptr < strlen(modelist); ptr++)
	{
		r = 0;

		{
			log(DEBUG,"process_modes: modechar: %c",modelist[ptr]);
			char modechar = modelist[ptr];
			switch (modelist[ptr])
			{
				case '-':
					if (mdir != 0)
					{
						if ((outlist[strlen(outlist)-1] == '+') || (outlist[strlen(outlist)-1] == '-'))
						{
							outlist[strlen(outlist)-1] = '-';
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
						if ((outlist[strlen(outlist)-1] == '+') || (outlist[strlen(outlist)-1] == '-'))
						{
							outlist[strlen(outlist)-1] = '+';
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
						log(DEBUG,"calling give_ops");
						r = give_ops(user,parameters[param++],chan,status);
					}
					else
					{
						log(DEBUG,"calling take_ops");
						r = take_ops(user,parameters[param++],chan,status);
					}
					if (r)
					{
						strcat(outlist,"o");
						strcpy(outpars[pc++],parameters[param-1]);
					}
				break;
			
				case 'h':
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
						r = give_hops(user,parameters[param++],chan,status);
					}
					else
					{
						r = take_hops(user,parameters[param++],chan,status);
					}
					if (r)
					{
						strcat(outlist,"h");
						strcpy(outpars[pc++],parameters[param-1]);
					}
				break;
			
				
				case 'v':
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
						r = give_voice(user,parameters[param++],chan,status);
					}
					else
					{
						r = take_voice(user,parameters[param++],chan,status);
					}
					if (r)
					{
						strcat(outlist,"v");
						strcpy(outpars[pc++],parameters[param-1]);
					}
				break;
				
				case 'b':
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
						r = add_ban(user,parameters[param++],chan,status);
					}
					else
					{
						r = take_ban(user,parameters[param++],chan,status);
					}
					if (r)
					{
						strcat(outlist,"b");
						strcpy(outpars[pc++],parameters[param-1]);
					}
				break;


				case 'k':
					if ((param >= pcnt))
						break;

					if (mdir == 1)
					{
						if (k_set)
							break;
						
						if (!strcmp(chan->key,""))
						{
							strcat(outlist,"k");
							char key[MAXBUF];
							strcpy(key,parameters[param++]);
							if (strlen(key)>32) {
								key[31] = '\0';
							}
							strcpy(outpars[pc++],key);
							strcpy(chan->key,key);
							k_set = true;
						}
					}
					else
					{
						/* checks on -k are case sensitive and only accurate to the
  						   first 32 characters */
						char key[MAXBUF];
						strcpy(key,parameters[param++]);
						if (strlen(key)>32) {
							key[31] = '\0';
						}
						/* only allow -k if correct key given */
						if (!strcmp(chan->key,key))
						{
							strcat(outlist,"k");
							strcpy(chan->key,"");
							strcpy(outpars[pc++],key);
						}
					}
				break;
				
				case 'l':
					if (mdir == 0)
					{
						if (chan->limit)
						{
							strcat(outlist,"l");
							chan->limit = 0;
						}
					}
					
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
						if (l_set)
							break;
						
						bool invalid = false;
						for (int i = 0; i < strlen(parameters[param]); i++)
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
						
						chan->limit = atoi(parameters[param]);
						
						// reported by mech: large values cause underflow
						if (chan->limit < 0)
							chan->limit = 0x7FFFFF;
							
						if (chan->limit)
						{
							strcat(outlist,"l");
							strcpy(outpars[pc++],parameters[param++]);
							l_set = true;
						}
					}
				break;
				
				case 'i':
					if (chan->inviteonly != mdir)
					{
						strcat(outlist,"i");
					}
					chan->inviteonly = mdir;
				break;
				
				case 't':
					if (chan->topiclock != mdir)
					{
						strcat(outlist,"t");
					}
					chan->topiclock = mdir;
				break;
				
				case 'n':
					if (chan->noexternal != mdir)
					{
						strcat(outlist,"n");
					}
					chan->noexternal = mdir;
				break;
				
				case 'm':
					if (chan->moderated != mdir)
					{
						strcat(outlist,"m");
					}
					chan->moderated = mdir;
				break;
				
				case 's':
					if (chan->secret != mdir)
					{
						strcat(outlist,"s");
						if (chan->c_private)
						{
							chan->c_private = 0;
							if (mdir)
							{
								strcat(outlist,"-p+");
							}
							else
							{
								strcat(outlist,"+p-");
							}
						}
					}
					chan->secret = mdir;
				break;
				
				case 'p':
					if (chan->c_private != mdir)
					{
						strcat(outlist,"p");
						if (chan->secret)
						{
							chan->secret = 0;
							if (mdir)
							{
								strcat(outlist,"-s+");
							}
							else
							{
								strcat(outlist,"+s-");
							}
						}
					}
					chan->c_private = mdir;
				break;
				
				default:
					log(DEBUG,"Preprocessing custom mode %c",modechar);
					string_list p;
					p.clear();
					if (((!strchr(chan->custom_modes,modechar)) && (!mdir)) || ((strchr(chan->custom_modes,modechar)) && (mdir)))
					{
						log(DEBUG,"Mode %c isnt set on %s but trying to remove!",modechar,chan->name);
						break;
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
  							log(DEBUG,"Not enough parameters for module-mode %c",modechar);
  							// we're supposed to have a parameter, but none was given... so dont handle the mode.
  							if (((ModeDefinedOn(modechar,MT_CHANNEL)>0) && (mdir)) || ((ModeDefinedOff(modechar,MT_CHANNEL)>0) && (!mdir)))	
  							{
  								handled = true;
  								param++;
  							}
  						}
  						for (int i = 0; i <= MODCOUNT; i++)
						{
							if (!handled)
							{
								if (modules[i]->OnExtendedMode(user,chan,modechar,MT_CHANNEL,mdir,p))
								{
									log(DEBUG,"OnExtendedMode returned nonzero for a module");
									char app[] = {modechar, 0};
									if (ptr>0)
									{
										if ((modelist[ptr-1] == '+') || (modelist[ptr-1] == '-'))
										{
											strcat(outlist, app);
										}
										else if (!strchr(outlist,modechar))
										{
											strcat(outlist, app);
										}
									}
									chan->SetCustomMode(modechar,mdir);
									// include parameters in output if mode has them
									if ((ModeDefinedOn(modechar,MT_CHANNEL)>0) && (mdir))
									{
										chan->SetCustomModeParam(modelist[ptr],parameters[param],mdir);
										strcpy(outpars[pc++],parameters[param++]);
									}
									// break, because only one module can handle the mode.
									handled = true;
        		 					}
        	 					}
     						}
     					}
				break;
				
			}
		}
	}

	/* this ensures only the *valid* modes are sent out onto the network */
	while ((outlist[strlen(outlist)-1] == '-') || (outlist[strlen(outlist)-1] == '+'))
	{
		outlist[strlen(outlist)-1] = '\0';
	}
	if (strcmp(outlist,""))
	{
		strcpy(outstr,outlist);
		for (ptr = 0; ptr < pc; ptr++)
		{
			strcat(outstr," ");
			strcat(outstr,outpars[ptr]);
		}
		if (local)
		{
			log(DEBUG,"Local mode change");
			WriteChannelLocal(chan, user, "MODE %s %s",chan->name,outstr);
		}
		else
		{
			if (servermode)
			{
				if (!silent)
				{
					WriteChannelWithServ(ServerName,chan,user,"MODE %s %s",chan->name,outstr);
					// M token for a usermode must go to all servers
					char buffer[MAXBUF];
					snprintf(buffer,MAXBUF,"M %s %s",chan->name, outstr);
					NetSendToAll(buffer);
				}
					
			}
			else
			{
				if (!silent)
				{
					WriteChannel(chan,user,"MODE %s %s",chan->name,outstr);
					// M token for a usermode must go to all servers
					char buffer[MAXBUF];
					snprintf(buffer,MAXBUF,"m %s %s %s",user->nick,chan->name, outstr);
					NetSendToAll(buffer);
				}
			}
		}
	}
}

// based on sourcemodes, return true or false to determine if umode is a valid mode a user may set on themselves or others.

bool allowed_umode(char umode, char* sourcemodes,bool adding)
{
	log(DEBUG,"Allowed_umode: %c %s",umode,sourcemodes);
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

bool process_module_umode(char umode, userrec* source, void* dest, bool adding)
{
	userrec* s2;
	bool faked = false;
	if (!source)
	{
		s2 = new userrec;
		strncpy(s2->nick,ServerName,NICKMAX);
		strcpy(s2->modes,"o");
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
				log(DEBUG,"Module claims umode %c",umode);
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
	int can_change,i;
	int direction = 1;
	char outpars[MAXBUF];

	dest = Find(parameters[0]);

	if (!user)
	{
		return;
	}

	if ((dest) && (pcnt == 1))
	{
		WriteServ(user->fd,"221 %s :+%s",user->nick,user->modes);
		return;
	}

	if ((dest) && (pcnt > 1))
	{
		char dmodes[MAXBUF];
		strncpy(dmodes,dest->modes,MAXBUF);
		log(DEBUG,"pulled up dest user modes: %s",dmodes);
	
		can_change = 0;
		if (user != dest)
		{
			if (strchr(user->modes,'o'))
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

		for (int i = 0; i < strlen(parameters[1]); i++)
		{
			if (parameters[1][i] == '+')
			{
				if (direction != 1)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '+';
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
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '-';
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
					if ((parameters[1][i] == 'i') || (parameters[1][i] == 'w') || (parameters[1][i] == 's') || (allowed_umode(parameters[1][i],user->modes,direction)))
					{
						can_change = 1;
					}
				}
				if (can_change)
				{
					if (direction == 1)
					{
						if ((!strchr(dmodes,parameters[1][i])) && (allowed_umode(parameters[1][i],user->modes,true)))
						{
							char umode = parameters[1][i];
							if ((process_module_umode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								dmodes[strlen(dmodes)+1]='\0';
								dmodes[strlen(dmodes)] = parameters[1][i];
								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							}
						}
					}
					else
					{
						if ((allowed_umode(parameters[1][i],user->modes,false)) && (strchr(dmodes,parameters[1][i])))
						{
							char umode = parameters[1][i];
							if ((process_module_umode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								int q = 0;
								char temp[MAXBUF];	
								char moo[MAXBUF];	

								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							
								strcpy(temp,"");
								for (q = 0; q < strlen(dmodes); q++)
								{
									if (dmodes[q] != parameters[1][i])
									{
										moo[0] = dmodes[q];
										moo[1] = '\0';
										strcat(temp,moo);
									}
								}
								strcpy(dmodes,temp);
							}
						}
					}
				}
			}
		}
		if (strlen(outpars))
		{
			char b[MAXBUF];
			strcpy(b,"");
			int z = 0;
			int i = 0;
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
				b[z] == '\0';

			if ((!strcmp(b,"+")) || (!strcmp(b,"-")))
				return;

			WriteTo(user, dest, "MODE %s :%s", dest->nick, b);

			// M token for a usermode must go to all servers
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"m %s %s %s",user->nick, dest->nick, b);
			NetSendToAll(buffer);

			if (strlen(dmodes)>MAXMODES)
			{
				dmodes[MAXMODES-1] = '\0';
			}
			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strncpy(dest->modes,dmodes,MAXMODES);

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
			if ((!strcmp(parameters[1],"+b")) || (!strcmp(parameters[1],"b")))
			{

				for (BanList::iterator i = Ptr->bans.begin(); i != Ptr->bans.end(); i++)
				{
					WriteServ(user->fd,"367 %s %s %s %s %d",user->nick, Ptr->name, i->data, i->set_by, i->set_time);
				}
				WriteServ(user->fd,"368 %s %s :End of channel ban list",user->nick, Ptr->name);
				return;
			}
		}

		if ((cstatus(user,Ptr) < STATUS_HOP) && (Ptr))
		{
			WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, Ptr->name);
			return;
		}

		process_modes(parameters,user,Ptr,cstatus(user,Ptr),pcnt,false,false,false);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
	}
}




void server_mode(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change,i;
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
		log(DEBUG,"params > 1");

		char dmodes[MAXBUF];
		strncpy(dmodes,dest->modes,MAXBUF);

		strcpy(outpars,"+");
		direction = 1;

		if ((parameters[1][0] != '+') && (parameters[1][0] != '-'))
			return;

		for (int i = 0; i < strlen(parameters[1]); i++)
		{
			if (parameters[1][i] == '+')
			{
				if (direction != 1)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '+';
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
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '-';
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
						if ((!strchr(dmodes,parameters[1][i])) && (allowed_umode(parameters[1][i],user->modes,true)))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								dmodes[strlen(dmodes)+1]='\0';
								dmodes[strlen(dmodes)] = parameters[1][i];
								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							}
						}
					}
					else
					{
						// can only remove a mode they already have
						log(DEBUG,"umode %c being removed",parameters[1][i]);
						if ((allowed_umode(parameters[1][i],user->modes,false)) && (strchr(dmodes,parameters[1][i])))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								int q = 0;
								char temp[MAXBUF];
								char moo[MAXBUF];	

								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							
								strcpy(temp,"");
								for (q = 0; q < strlen(dmodes); q++)
								{
									if (dmodes[q] != parameters[1][i])
									{
										moo[0] = dmodes[q];
										moo[1] = '\0';
										strcat(temp,moo);
									}
								}
								strcpy(dmodes,temp);
							}
						}
					}
				}
			}
		}
		if (strlen(outpars))
		{
			char b[MAXBUF];
			strcpy(b,"");
			int z = 0;
			int i = 0;
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
				b[z] == '\0';

			if ((!strcmp(b,"+")) || (!strcmp(b,"-")))
				return;

			WriteTo(user, dest, "MODE %s :%s", dest->nick, b);

			// M token for a usermode must go to all servers
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"m %s %s %s",user->nick, dest->nick, b);
			NetSendToAll(buffer);
			
			if (strlen(dmodes)>MAXMODES)
			{
				dmodes[MAXMODES-1] = '\0';
			}
			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strncpy(dest->modes,dmodes,MAXMODES);

		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		process_modes(parameters,user,Ptr,STATUS_OP,pcnt,true,false,false);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
	}
}



void merge_mode(char **parameters, int pcnt)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change,i;
	int direction = 1;
	char outpars[MAXBUF];

	dest = Find(parameters[0]);
	
	// fix: ChroNiCk found this - we cant use this as debug if its null!
	if (dest)
	{
		log(DEBUG,"merge_mode on %s",dest->nick);
	}

	if ((dest) && (pcnt > 1))
	{
		log(DEBUG,"params > 1");

		char dmodes[MAXBUF];
		strncpy(dmodes,dest->modes,MAXBUF);

		strcpy(outpars,"+");
		direction = 1;

		if ((parameters[1][0] != '+') && (parameters[1][0] != '-'))
			return;

		for (int i = 0; i < strlen(parameters[1]); i++)
		{
			if (parameters[1][i] == '+')
			{
				if (direction != 1)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '+';
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
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '-';
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
						if ((!strchr(dmodes,parameters[1][i])) && (allowed_umode(parameters[1][i],"o",true)))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, NULL, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								dmodes[strlen(dmodes)+1]='\0';
								dmodes[strlen(dmodes)] = parameters[1][i];
								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							}
						}
					}
					else
					{
						// can only remove a mode they already have
						log(DEBUG,"umode %c being removed",parameters[1][i]);
						if ((allowed_umode(parameters[1][i],"o",false)) && (strchr(dmodes,parameters[1][i])))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, NULL, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								int q = 0;
								char temp[MAXBUF];
								char moo[MAXBUF];	

								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							
								strcpy(temp,"");
								for (q = 0; q < strlen(dmodes); q++)
								{
									if (dmodes[q] != parameters[1][i])
									{
										moo[0] = dmodes[q];
										moo[1] = '\0';
										strcat(temp,moo);
									}
								}
								strcpy(dmodes,temp);
							}
						}
					}
				}
			}
		}
		if (strlen(outpars))
		{
			char b[MAXBUF];
			strcpy(b,"");
			int z = 0;
			int i = 0;
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
				b[z] == '\0';

			if ((!strcmp(b,"+")) || (!strcmp(b,"-")))
				return;

			if (strlen(dmodes)>MAXMODES)
			{
				dmodes[MAXMODES-1] = '\0';
			}
			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strncpy(dest->modes,dmodes,MAXMODES);

		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		userrec s2;
		strncpy(s2.nick,ServerName,NICKMAX);
		strcpy(s2.modes,"o");
		s2.fd = -1;
		process_modes(parameters,&s2,Ptr,STATUS_OP,pcnt,true,true,false);
	}
}


void merge_mode2(char **parameters, int pcnt, userrec* user)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change,i;
	int direction = 1;
	char outpars[MAXBUF];

	dest = Find(parameters[0]);
	
	// fix: ChroNiCk found this - we cant use this as debug if its null!
	if (dest)
	{
		log(DEBUG,"merge_mode on %s",dest->nick);
	}

	if ((dest) && (pcnt > 1))
	{
		log(DEBUG,"params > 1");

		char dmodes[MAXBUF];
		strncpy(dmodes,dest->modes,MAXBUF);

		strcpy(outpars,"+");
		direction = 1;

		if ((parameters[1][0] != '+') && (parameters[1][0] != '-'))
			return;

		for (int i = 0; i < strlen(parameters[1]); i++)
		{
			if (parameters[1][i] == '+')
			{
				if (direction != 1)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '+';
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
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '-';
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
						if ((!strchr(dmodes,parameters[1][i])) && (allowed_umode(parameters[1][i],user->modes,true)))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, NULL, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								dmodes[strlen(dmodes)+1]='\0';
								dmodes[strlen(dmodes)] = parameters[1][i];
								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							}
						}
					}
					else
					{
						// can only remove a mode they already have
						log(DEBUG,"umode %c being removed",parameters[1][i]);
						if ((allowed_umode(parameters[1][i],user->modes,false)) && (strchr(dmodes,parameters[1][i])))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, NULL, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								int q = 0;
								char temp[MAXBUF];
								char moo[MAXBUF];	

								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							
								strcpy(temp,"");
								for (q = 0; q < strlen(dmodes); q++)
								{
									if (dmodes[q] != parameters[1][i])
									{
										moo[0] = dmodes[q];
										moo[1] = '\0';
										strcat(temp,moo);
									}
								}
								strcpy(dmodes,temp);
							}
						}
					}
				}
			}
		}
		if (strlen(outpars))
		{
			char b[MAXBUF];
			strcpy(b,"");
			int z = 0;
			int i = 0;
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
				b[z] == '\0';

			if ((!strcmp(b,"+")) || (!strcmp(b,"-")))
				return;

			WriteTo(user,dest,"MODE :%s",b);

			if (strlen(dmodes)>MAXMODES)
			{
				dmodes[MAXMODES-1] = '\0';
			}
			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strncpy(dest->modes,dmodes,MAXMODES);

		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		log(DEBUG,"merge_mode2: found channel %s",Ptr->name);
		if (Ptr)
		{
			if ((cstatus(user,Ptr) < STATUS_HOP) && (!is_uline(user->server)))
			{
				return;
			}
			process_modes(parameters,user,Ptr,cstatus(user,Ptr),pcnt,false,false,true);
		}
	}
}


