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

extern int MODCOUNT;
extern vector<Module*> modules;
extern vector<ircd_module*> factory;

/* return 0 or 1 depending if users u and u2 share one or more common channels
 * (used by QUIT, NICK etc which arent channel specific notices) */

int common_channels(userrec *u, userrec *u2)
{
	int i = 0;
	int z = 0;

	if ((!u) || (!u2))
	{
		log(DEFAULT,"*** BUG *** common_channels was given an invalid parameter");
		return 0;
	}
	for (int i = 0; i != MAXCHANS; i++)
	{
		for (z = 0; z != MAXCHANS; z++)
		{
			if ((u->chans[i].channel != NULL) && (u2->chans[z].channel != NULL))
			{
				if ((u->chans[i].channel == u2->chans[z].channel) && (u->chans[i].channel) && (u2->chans[z].channel) && (u->registered == 7) && (u2->registered == 7))
				{
					if ((c_count(u)) && (c_count(u2)))
					{
						return 1;
					}
				}
			}
		}
	}
	return 0;
}


void safedelete(userrec *p)
{
	if (p)
	{
		log(DEBUG,"deleting %s %s %s %s",p->nick,p->ident,p->dhost,p->fullname);
		log(DEBUG,"safedelete(userrec*): pointer is safe to delete");
		delete p;
		p = NULL;
	}
	else
	{
		log(DEBUG,"safedelete(userrec*): unsafe pointer operation squished");
	}
}

void safedelete(chanrec *p)
{
	if (p)
	{
		delete p;
		p = NULL;
		log(DEBUG,"safedelete(chanrec*): pointer is safe to delete");
	}
	else
	{
		log(DEBUG,"safedelete(chanrec*): unsafe pointer operation squished");
	}
}


void tidystring(char* str)
{
	// strips out double spaces before a : parameter
	
	char temp[MAXBUF];
	bool go_again = true;
	
	if (!str)
	{
		return;
	}
	
	while ((str[0] == ' ') && (strlen(str)>0))
	{
		str++;
	}
	
	while (go_again)
	{
		bool noparse = false;
		int t = 0, a = 0;
		go_again = false;
		while (a < strlen(str))
		{
			if ((a<strlen(str)-1) && (noparse==false))
			{
				if ((str[a] == ' ') && (str[a+1] == ' '))
				{
					log(DEBUG,"Tidied extra space out of string: %s",str);
					go_again = true;
					a++;
				}
			}
			
			if (a<strlen(str)-1)
			{
				if ((str[a] == ' ') && (str[a+1] == ':'))
				{
					noparse = true;
				}
			}
			
			temp[t++] = str[a++];
		}
		temp[t] = '\0';
		strncpy(str,temp,MAXBUF);
	}
}

/* chop a string down to 512 characters and preserve linefeed (irc max
 * line length) */

void chop(char* str)
{
  if (!str)
  {
  	log(DEBUG,"ERROR! Null string passed to chop()!");
  	return;
  }
  string temp = str;
  FOREACH_MOD OnServerRaw(temp,false);
  const char* str2 = temp.c_str();
  sprintf(str,"%s",str2);
  

  if (strlen(str) >= 512)
  {
  	str[509] = '\r';
  	str[510] = '\n';
  	str[511] = '\0';
  }
}


void Blocking(int s)
{
  int flags;
  log(DEBUG,"Blocking: %d",s);
  flags = fcntl(s, F_GETFL, 0);
  fcntl(s, F_SETFL, flags ^ O_NONBLOCK);
}

void NonBlocking(int s)
{
  int flags;
  log(DEBUG,"NonBlocking: %d",s);
  flags = fcntl(s, F_GETFL, 0);
  //fcntl(s, F_SETFL, O_NONBLOCK);
  fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

int CleanAndResolve (char *resolvedHost, const char *unresolvedHost)
{
  struct hostent *hostPtr = NULL;
  struct in_addr addr;

  memset (resolvedHost, '\0',MAXBUF);
  if(unresolvedHost == NULL)
	return(ERROR);
  if ((inet_aton(unresolvedHost,&addr)) == 0)
	return(ERROR);
  hostPtr = gethostbyaddr ((char *)&addr.s_addr,sizeof(addr.s_addr),AF_INET);
  if (hostPtr != NULL)
  	snprintf(resolvedHost,MAXBUF,"%s",hostPtr->h_name);
  else
  	snprintf(resolvedHost,MAXBUF,"%s",unresolvedHost);
  return (TRUE);
}

int c_count(userrec* u)
{
	int z = 0;
	for (int i =0; i != MAXCHANS; i++)
		if (u->chans[i].channel != NULL)
			z++;
	return z;

}

bool hasumode(userrec* user, char mode)
{
	if (user)
	{
		return (strchr(user->modes,mode)>0);
	}
	else return false;
}


void ChangeName(userrec* user, const char* gecos)
{
	strncpy(user->fullname,gecos,MAXBUF);

	// TODO: replace these with functions:
	// NetSendToAll - to all
	// NetSendToCommon - to all that hold users sharing a common channel with another user
	// NetSendToOne - to one server
	// NetSendToAllExcept - send to all but one
	// all by servername

	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"a %s :%s",user->nick,gecos);
	NetSendToAll(buffer);
}

void ChangeDisplayedHost(userrec* user, const char* host)
{
	strncpy(user->dhost,host,160);
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"b %s %s",user->nick,host);
	NetSendToAll(buffer);
}

/* verify that a user's ident and nickname is valid */

int isident(const char* n)
{
        char v[MAXBUF];
        if (!n)

        {
                return 0;
        }
        if (!strcmp(n,""))
        {
                return 0;
        }
        for (int i = 0; i != strlen(n); i++)
        {
                if ((n[i] < 33) || (n[i] > 125))
                {
                        return 0;
                }
                /* can't occur ANYWHERE in an Ident! */
                if (strchr("<>,./?:;@'~#=+()*&%$£ \"!",n[i]))
                {
                        return 0;
                }
        }
        return 1;
}


int isnick(const char* n)
{
	int i = 0;
	char v[MAXBUF];
	if (!n)
	{
		return 0;
	}
	if (!strcmp(n,""))
	{
		return 0;
	}
	if (strlen(n) > NICKMAX-1)
	{
		return 0;
	}
	for (int i = 0; i != strlen(n); i++)
	{
		if ((n[i] < 33) || (n[i] > 125))
		{
			return 0;
		}
		/* can't occur ANYWHERE in a nickname! */
		if (strchr("<>,./?:;@'~#=+()*&%$£ \"!",n[i]))
		{
			return 0;
		}
		/* can't occur as the first char of a nickname... */
		if ((strchr("0123456789",n[i])) && (!i))
		{
			return 0;
		}
	}
	return 1;
}

/* returns the status character for a given user on a channel, e.g. @ for op,
 * % for halfop etc. If the user has several modes set, the highest mode
 * the user has must be returned. */

char* cmode(userrec *user, chanrec *chan)
{
	if ((!user) || (!chan))
	{
		log(DEFAULT,"*** BUG *** cmode was given an invalid parameter");
		return "";
	}

	int i;
	for (int i = 0; i != MAXCHANS; i++)
	{
		if ((user->chans[i].channel == chan) && (chan != NULL))
		{
			if ((user->chans[i].uc_modes & UCMODE_OP) > 0)
			{
				return "@";
			}
			if ((user->chans[i].uc_modes & UCMODE_HOP) > 0)
			{
				return "%";
			}
			if ((user->chans[i].uc_modes & UCMODE_VOICE) > 0)
			{
				return "+";
			}
			return "";
		}
	}
}

/* returns the status value for a given user on a channel, e.g. STATUS_OP for
 * op, STATUS_VOICE for voice etc. If the user has several modes set, the
 * highest mode the user has must be returned. */

int cstatus(userrec *user, chanrec *chan)
{
	if ((!chan) || (!user))
	{
		log(DEFAULT,"*** BUG *** cstatus was given an invalid parameter");
		return 0;
	}

	for (int i = 0; i != MAXCHANS; i++)
	{
		if ((user->chans[i].channel == chan) && (chan != NULL))
		{
			if ((user->chans[i].uc_modes & UCMODE_OP) > 0)
			{
				return STATUS_OP;
			}
			if ((user->chans[i].uc_modes & UCMODE_HOP) > 0)
			{
				return STATUS_HOP;
			}
			if ((user->chans[i].uc_modes & UCMODE_VOICE) > 0)
			{
				return STATUS_VOICE;
			}
			return STATUS_NORMAL;
		}
	}
}

/* returns 1 if user u has channel c in their record, 0 if not */

int has_channel(userrec *u, chanrec *c)
{
	if ((!u) || (!c))
	{
		log(DEFAULT,"*** BUG *** has_channel was given an invalid parameter");
		return 0;
	}
	for (int i =0; i != MAXCHANS; i++)
	{
		if (u->chans[i].channel == c)
		{
			return 1;
		}
	}
	return 0;
}


void TidyBan(char *ban)
{
	if (!ban) {
		log(DEFAULT,"*** BUG *** TidyBan was given an invalid parameter");
		return;
	}
	
	char temp[MAXBUF],NICK[MAXBUF],IDENT[MAXBUF],HOST[MAXBUF];

	strcpy(temp,ban);

	char* pos_of_pling = strchr(temp,'!');
	char* pos_of_at = strchr(temp,'@');

	pos_of_pling[0] = '\0';
	pos_of_at[0] = '\0';
	pos_of_pling++;
	pos_of_at++;

	strncpy(NICK,temp,NICKMAX);
	strncpy(IDENT,pos_of_pling,IDENTMAX+1);
	strncpy(HOST,pos_of_at,160);

	sprintf(ban,"%s!%s@%s",NICK,IDENT,HOST);
}

char lst[MAXBUF];

char* chlist(userrec *user)
{
	int i = 0;
	char cmp[MAXBUF];

        log(DEBUG,"chlist: %s",user->nick);
	strcpy(lst,"");
	if (!user)
	{
		return lst;
	}
	for (int i = 0; i != MAXCHANS; i++)
	{
		if (user->chans[i].channel != NULL)
		{
			if (user->chans[i].channel->name)
			{
				strcpy(cmp,user->chans[i].channel->name);
				strcat(cmp," ");
				if (!strstr(lst,cmp))
				{
					if ((!user->chans[i].channel->c_private) && (!user->chans[i].channel->secret))
					{
						strcat(lst,cmode(user,user->chans[i].channel));
						strcat(lst,user->chans[i].channel->name);
						strcat(lst," ");
					}
				}
			}
		}
	}
	if (strlen(lst))
	{
		lst[strlen(lst)-1] = '\0'; // chop trailing space
	}
	return lst;
}


void send_network_quit(const char* nick, const char* reason)
{
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"Q %s :%s",nick,reason);
	NetSendToAll(buffer);
}


