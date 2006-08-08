/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *		<Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <stdarg.h>
#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <signal.h>
#include <time.h>
#include <string>
#include <sstream>
#ifdef HAS_EXECINFO
#include <execinfo.h>
#endif
#include "connection.h"
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "mode.h"
#include "xline.h"
#include "commands.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"

extern int MODCOUNT;
extern ModuleList modules;
extern ServerConfig *Config;
extern InspIRCd* ServerInstance;
extern time_t TIME;
extern char lowermap[255];
extern userrec* fd_ref_table[MAX_DESCRIPTORS];
extern std::vector<userrec*> all_opers;
extern user_hash clientlist;
extern chan_hash chanlist;

char LOG_FILE[MAXBUF];

extern std::vector<userrec*> local_users;

static char TIMESTR[26];
static time_t LAST = 0;

/** log()
 *  Write a line of text `text' to the logfile (and stdout, if in nofork) if the level `level'
 *  is greater than the configured loglevel.
 */
void do_log(int level, const char *text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	/* If we were given -debug we output all messages, regardless of configured loglevel */
	if ((level < Config->LogLevel) && !Config->forcedebug)
		return;

	if (TIME != LAST)
	{
		struct tm *timeinfo = localtime(&TIME);

		strlcpy(TIMESTR,asctime(timeinfo),26);
		TIMESTR[24] = ':';
		LAST = TIME;
	}

	if (Config->log_file)
	{
		va_start(argsPtr, text);
		vsnprintf(textbuffer, MAXBUF, text, argsPtr);
		va_end(argsPtr);

		if (Config->writelog)
		{
			fprintf(Config->log_file,"%s %s\n",TIMESTR,textbuffer);
			fflush(Config->log_file);
		}
	}
	
	if (Config->nofork)
	{
		printf("%s %s\n", TIMESTR, textbuffer);
	}
}

/** readfile()
 *  Read the contents of a file located by `fname' into a file_cache pointed at by `F'.
 *
 *  XXX - we may want to consider returning a file_cache or pointer to one, less confusing.
 */
void readfile(file_cache &F, const char* fname)
{
	FILE* file;
	char linebuf[MAXBUF];

	log(DEBUG,"readfile: loading %s",fname);
	F.clear();
	file =  fopen(fname,"r");

	if (file)
	{
		while (!feof(file))
		{
			fgets(linebuf,sizeof(linebuf),file);
			linebuf[strlen(linebuf)-1]='\0';

			if (!*linebuf)
			{
				strcpy(linebuf,"  ");
			}

			if (!feof(file))
			{
				F.push_back(linebuf);
			}
		}

		fclose(file);
	}
	else
	{
		log(DEBUG,"readfile: failed to load file: %s",fname);
	}

	log(DEBUG,"readfile: loaded %s, %lu lines",fname,(unsigned long)F.size());
}


std::string GetServerDescription(const char* servername)
{
	std::string description = "";

	FOREACH_MOD(I_OnGetServerDescription,OnGetServerDescription(servername,description));

	if (description != "")
	{
		return description;
	}
	else
	{
		// not a remote server that can be found, it must be me.
		return Config->ServerDesc;
	}
}

/* XXX - We don't use WriteMode for this because WriteMode is very slow and
 * this isnt. Basically WriteMode has to iterate ALL the users 'n' times for
 * the number of modes provided, e.g. if you send WriteMode 'og' to write to
 * opers with globops, and you have 2000 users, thats 4000 iterations. WriteOpers
 * uses the oper list, which means if you have 2000 users but only 5 opers,
 * it iterates 5 times.
 */
void WriteOpers(const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if (!text)
	{
		log(DEFAULT,"*** BUG *** WriteOpers was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	WriteOpers_NoFormat(textbuffer);
}

void WriteOpers_NoFormat(const char* text)
{
	if (!text)
	{
		log(DEFAULT,"*** BUG *** WriteOpers_NoFormat was given an invalid parameter");
		return;
	}

	for (std::vector<userrec*>::iterator i = all_opers.begin(); i != all_opers.end(); i++)
	{
		userrec* a = *i;

		if (IS_LOCAL(a))
		{
			if (a->modes[UM_SERVERNOTICE])
			{
				// send server notices to all with +s
				a->WriteServ("NOTICE %s :%s",a->nick,text);
			}
		}
	}
}

void ServerNoticeAll(char* text, ...)
{
	if (!text)
		return;

	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,"NOTICE $%s :%s",Config->ServerName,textbuffer);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = *i;
		t->WriteServ(std::string(formatbuffer));
	}
}

void ServerPrivmsgAll(char* text, ...)
{
	if (!text)
		return;

	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,"PRIVMSG $%s :%s",Config->ServerName,textbuffer);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = *i;
		t->WriteServ(std::string(formatbuffer));
	}
}

void WriteMode(const char* modes, int flags, const char* text, ...)
{
	char textbuffer[MAXBUF];
	int modelen;
	va_list argsPtr;

	if ((!text) || (!modes) || (!flags))
	{
		log(DEFAULT,"*** BUG *** WriteMode was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	modelen = strlen(modes);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		bool send_to_user = false;

		if (flags == WM_AND)
		{
			send_to_user = true;

			for (int n = 0; n < modelen; n++)
			{
				if (!t->modes[modes[n]-65])
				{
					send_to_user = false;
					break;
				}
			}
		}
		else if (flags == WM_OR)
		{
			send_to_user = false;

			for (int n = 0; n < modelen; n++)
			{
				if (t->modes[modes[n]-65])
				{
					send_to_user = true;
					break;
				}
			}
		}

		if (send_to_user)
		{
			t->WriteServ("NOTICE %s :%s",t->nick,textbuffer);
		}
	}
}

void NoticeAll(userrec *source, bool local_only, char* text, ...)
{
	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;

	if ((!text) || (!source))
	{
		log(DEFAULT,"*** BUG *** NoticeAll was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,"NOTICE $* :%s",textbuffer);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = *i;
		t->WriteFrom(source,std::string(formatbuffer));
	}
}


void WriteWallOps(userrec *source, bool local_only, char* text, ...)
{
	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;

	if ((!text) || (!source))
	{
		log(DEFAULT,"*** BUG *** WriteOpers was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,"WALLOPS :%s",textbuffer);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);

		if ((IS_LOCAL(t)) && (t->modes[UM_WALLOPS]))
		{
			source->WriteTo(t,std::string(formatbuffer));
		}
	}
}

/* convert a string to lowercase. Note following special circumstances
 * taken from RFC 1459. Many "official" server branches still hold to this
 * rule so i will too;
 *
 *  Because of IRC's scandanavian origin, the characters {}| are
 *  considered to be the lower case equivalents of the characters []\,
 *  respectively. This is a critical issue when determining the
 *  equivalence of two nicknames.
 */
void strlower(char *n)
{
	if (n)
	{
		for (char* t = n; *t; t++)
			*t = lowermap[(unsigned char)*t];
	}
}

/* Find a user record by nickname and return a pointer to it */

userrec* Find(const std::string &nick)
{
	user_hash::iterator iter = clientlist.find(nick);

	if (iter == clientlist.end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

userrec* Find(const char* nick)
{
	user_hash::iterator iter;

	if (!nick)
		return NULL;

	iter = clientlist.find(nick);
	
	if (iter == clientlist.end())
		return NULL;

	return iter->second;
}

/* find a channel record by channel name and return a pointer to it */

chanrec* FindChan(const char* chan)
{
	chan_hash::iterator iter;

	if (!chan)
	{
		log(DEFAULT,"*** BUG *** Findchan was given an invalid parameter");
		return NULL;
	}

	iter = chanlist.find(chan);

	if (iter == chanlist.end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}


long GetMaxBans(char* name)
{
	std::string x;
	for (std::map<std::string,int>::iterator n = Config->maxbans.begin(); n != Config->maxbans.end(); n++)
	{
		x = n->first;
		if (match(name,x.c_str()))
		{
			return n->second;
		}
	}
	return 64;
}

void purge_empty_chans(userrec* u)
{
	std::vector<chanrec*> to_delete;

	// firstly decrement the count on each channel
	for (std::vector<ucrec*>::iterator f = u->chans.begin(); f != u->chans.end(); f++)
	{
		ucrec* uc = (ucrec*)(*f);
		if (uc->channel)
		{
			if (uc->channel->DelUser(u) == 0)
			{
				/* No users left in here, mark it for deletion */
				to_delete.push_back(uc->channel);
				uc->channel = NULL;
			}
		}
	}

	log(DEBUG,"purge_empty_chans: %d channels to delete",to_delete.size());

	for (std::vector<chanrec*>::iterator n = to_delete.begin(); n != to_delete.end(); n++)
	{
		chanrec* thischan = (chanrec*)*n;
		chan_hash::iterator i2 = chanlist.find(thischan->name);
		if (i2 != chanlist.end())
		{
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(i2->second));
			DELETE(i2->second);
			chanlist.erase(i2);
		}
	}

	u->UnOper();
}


char* chanmodes(chanrec *chan, bool showkey)
{
	static char scratch[MAXBUF];
	static char sparam[MAXBUF];
	char* offset = scratch;
	std::string extparam = "";

	if (!chan)
	{
		log(DEFAULT,"*** BUG *** chanmodes was given an invalid parameter");
		*scratch = '\0';
		return scratch;
	}

	*scratch = '\0';
	*sparam = '\0';

	/* This was still iterating up to 190, chanrec::custom_modes is only 64 elements -- Om */
	for(int n = 0; n < 64; n++)
	{
		if(chan->modes[n])
		{
			*offset++ = n+65;
			extparam = "";
			switch (n)
			{
				case CM_KEY:
					extparam = (showkey ? chan->key : "<key>");
				break;
				case CM_LIMIT:
					extparam = ConvToStr(chan->limit);
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
					extparam = chan->GetModeParameter(n+65);
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
 * spaces and op, voice etc status shown as @ and + */

void userlist(userrec *user,chanrec *c)
{
	if ((!c) || (!user))
	{
		log(DEFAULT,"*** BUG *** userlist was given an invalid parameter");
		return;
	}

	char list[MAXBUF];
	size_t dlen, curlen;

	dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, c->name);

	int numusers = 0;
	char* ptr = list + dlen;

	CUList *ulist= c->GetUsers();

	/* Improvement by Brain - this doesnt change in value, so why was it inside
	 * the loop?
	 */
	bool has_user = c->HasUser(user);

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

		size_t ptrlen = snprintf(ptr, MAXBUF, "%s%s ", cmode(i->second, c), i->second->nick);

		curlen += ptrlen;
		ptr += ptrlen;

		numusers++;

		if (curlen > (480-NICKMAX))
		{
			/* list overflowed into multiple numerics */
			user->WriteServ(list);

			/* reset our lengths */
			dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, c->name);
			ptr = list + dlen;

			ptrlen = 0;
			numusers = 0;
		}
	}

	/* if whats left in the list isnt empty, send it */
	if (numusers)
	{
		user->WriteServ(list);
	}
}

/*
 * return a count of the users on a specific channel accounting for
 * invisible users who won't increase the count. e.g. for /LIST
 */
int usercount_i(chanrec *c)
{
	int count = 0;

	if (!c)
		return 0;

	CUList *ulist= c->GetUsers();
	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (!(i->second->modes[UM_INVISIBLE]))
			count++;
	}

	return count;
}

int usercount(chanrec *c)
{
	return (c ? c->GetUserCounter() : 0);
}


/* looks up a users password for their connection class (<ALLOW>/<DENY> tags)
 * NOTE: If the <ALLOW> or <DENY> tag specifies an ip, and this user resolves,
 * then their ip will be taken as 'priority' anyway, so for example,
 * <connect allow="127.0.0.1"> will match joe!bloggs@localhost
 */
ConnectClass GetClass(userrec *user)
{
	for (ClassVector::iterator i = Config->Classes.begin(); i != Config->Classes.end(); i++)
	{
		if ((match(user->GetIPString(),i->host.c_str(),true)) || (match(user->host,i->host.c_str())))
		{
			return *i;
		}
	}

	return *(Config->Classes.begin());
}

/*
 * sends out an error notice to all connected clients (not to be used
 * lightly!)
 */
void send_error(char *s)
{
	log(DEBUG,"send_error: %s",s);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		if (t->registered == REG_ALL)
		{
		   	t->WriteServ("NOTICE %s :%s",t->nick,s);
	   	}
		else
		{
			// fix - unregistered connections receive ERROR, not NOTICE
			t->Write("ERROR :%s",s);
		}
	}
}

void Error(int status)
{
	void *array[300];
	size_t size;
	char **strings;

	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGABRT, SIG_IGN);
	signal(SIGSEGV, SIG_IGN);
	signal(SIGURG, SIG_IGN);
	signal(SIGKILL, SIG_IGN);
	log(DEFAULT,"*** fell down a pothole in the road to perfection ***");
#ifdef HAS_EXECINFO
	log(DEFAULT,"Please report the backtrace lines shown below with any bugreport to the bugtracker at http://www.inspircd.org/bugtrack/");
	size = backtrace(array, 30);
	strings = backtrace_symbols(array, size);
	for (size_t i = 0; i < size; i++) {
		log(DEFAULT,"[%d] %s", i, strings[i]);
	}
	free(strings);
	WriteOpers("*** SIGSEGV: Please see the ircd.log for backtrace and report the error to http://www.inspircd.org/bugtrack/");
#else
	log(DEFAULT,"You do not have execinfo.h so i could not backtrace -- on FreeBSD, please install the libexecinfo port.");
#endif
	send_error("Somebody screwed up... Whoops. IRC Server terminating.");
	signal(SIGSEGV, SIG_DFL);
	if (raise(SIGSEGV) == -1)
	{
		log(DEFAULT,"What the hell, i couldnt re-raise SIGSEGV! Error: %s",strerror(errno));
	}
	Exit(status);
}

// this function counts all users connected, wether they are registered or NOT.
int usercnt(void)
{
	return clientlist.size();
}

// this counts only registered users, so that the percentages in /MAP don't mess up when users are sitting in an unregistered state
int registered_usercount(void)
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		c += (i->second->registered == REG_ALL);
	}

	return c;
}

int usercount_invisible(void)
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		c += ((i->second->registered == REG_ALL) && (i->second->modes[UM_INVISIBLE]));
	}

	return c;
}

int usercount_opers(void)
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (*(i->second->oper))
			c++;
	}
	return c;
}

int usercount_unknown(void)
{
	int c = 0;

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		if (t->registered != REG_ALL)
			c++;
	}

	return c;
}

long chancount(void)
{
	return chanlist.size();
}

long local_count()
{
	int c = 0;

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = (userrec*)(*i);
		if (t->registered == REG_ALL)
			c++;
	}

	return c;
}

void ShowMOTD(userrec *user)
{
	if (!Config->MOTD.size())
	{
		user->WriteServ("422 %s :Message of the day file is missing.",user->nick);
		return;
	}
	user->WriteServ("375 %s :%s message of the day", user->nick, Config->ServerName);

	for (unsigned int i = 0; i < Config->MOTD.size(); i++)
		user->WriteServ("372 %s :- %s",user->nick,Config->MOTD[i].c_str());

	user->WriteServ("376 %s :End of message of the day.", user->nick);
}

void ShowRULES(userrec *user)
{
	if (!Config->RULES.size())
	{
		user->WriteServ("NOTICE %s :Rules file is missing.",user->nick);
		return;
	}
	user->WriteServ("NOTICE %s :%s rules",user->nick,Config->ServerName);

	for (unsigned int i = 0; i < Config->RULES.size(); i++)
		user->WriteServ("NOTICE %s :%s",user->nick,Config->RULES[i].c_str());

	user->WriteServ("NOTICE %s :End of %s rules.",user->nick,Config->ServerName);
}

// this returns 1 when all modules are satisfied that the user should be allowed onto the irc server
// (until this returns true, a user will block in the waiting state, waiting to connect up to the
// registration timeout maximum seconds)
bool AllModulesReportReady(userrec* user)
{
	if (!Config->global_implementation[I_OnCheckReady])
		return true;

	for (int i = 0; i <= MODCOUNT; i++)
	{
		if (Config->implement_lists[i][I_OnCheckReady])
		{
			int res = modules[i]->OnCheckReady(user);
			if (!res)
				return false;
		}
	}

	return true;
}

/* Make Sure Modules Are Avaliable!
 * (BugFix By Craig.. See? I do work! :p)
 * Modified by brain, requires const char*
 * to work with other API functions
 */

/* XXX - Needed? */
bool FileExists (const char* file)
{
	FILE *input;
	if ((input = fopen (file, "r")) == NULL)
	{
		return(false);
	}
	else
	{
		fclose (input);
		return(true);
	}
}

char* CleanFilename(char* name)
{
	char* p = name + strlen(name);
	while ((p != name) && (*p != '/')) p--;
	return (p != name ? ++p : p);
}

bool DirValid(char* dirandfile)
{
	char work[MAXBUF];
	char buffer[MAXBUF];
	char otherdir[MAXBUF];
	int p;

	strlcpy(work, dirandfile, MAXBUF);
	p = strlen(work);

	// we just want the dir
	while (*work)
	{
		if (work[p] == '/')
		{
			work[p] = '\0';
			break;
		}

		work[p--] = '\0';
	}

	// Get the current working directory
	if (getcwd(buffer, MAXBUF ) == NULL )
		return false;

	chdir(work);

	if (getcwd(otherdir, MAXBUF ) == NULL )
		return false;

	chdir(buffer);

	size_t t = strlen(work);

	if (strlen(otherdir) >= t)
	{
		otherdir[t] = '\0';

		if (!strcmp(otherdir,work))
		{
			return true;
		}

		return false;
	}
	else
	{
		return false;
	}
}

std::string GetFullProgDir(char** argv, int argc)
{
	char work[MAXBUF];
	char buffer[MAXBUF];
	char otherdir[MAXBUF];
	int p;

	strlcpy(work,argv[0],MAXBUF);
	p = strlen(work);

	// we just want the dir
	while (*work)
	{
		if (work[p] == '/')
		{
			work[p] = '\0';
			break;
		}

		work[p--] = '\0';
	}

	// Get the current working directory
	if (getcwd(buffer, MAXBUF) == NULL)
		return "";

	chdir(work);

	if (getcwd(otherdir, MAXBUF) == NULL)
		return "";

	chdir(buffer);
	return otherdir;
}

int InsertMode(std::string &output, const char* mode, unsigned short section)
{
	unsigned short currsection = 1;
	unsigned int pos = output.find("CHANMODES=", 0) + 10; // +10 for the length of "CHANMODES="
	
	if(section > 4 || section == 0)
	{
		log(DEBUG, "InsertMode: CHANMODES doesn't have a section %dh :/", section);
		return 0;
	}
	
	for(; pos < output.size(); pos++)
	{
		if(section == currsection)
			break;
			
		if(output[pos] == ',')
			currsection++;
	}
	
	output.insert(pos, mode);
	return 1;
}

bool IsValidChannelName(const char *chname)
{
	char *c;

	/* check for no name - don't check for !*chname, as if it is empty, it won't be '#'! */
	if (!chname || *chname != '#')
	{
		return false;
	}

	c = (char *)chname + 1;
	while (*c)
	{
		switch (*c)
		{
			case ' ':
			case ',':
			case 7:
				return false;
		}

		c++;
	}
		
	/* too long a name - note funky pointer arithmetic here. */
	if ((c - chname) > CHANMAX)
	{
			return false;
	}

	return true;
}

inline int charlcat(char* x,char y,int z)
{
	char* x__n = x;
	int v = 0;

	while(*x__n++)
		v++;

	if (v < z - 1)
	{
		*--x__n = y;
		*++x__n = 0;
	}

	return v;
}

bool charremove(char* mp, char remove)
{
	char* mptr = mp;
	bool shift_down = false;

	while (*mptr)
	{
		if (*mptr == remove)
		shift_down = true;

		if (shift_down)
			*mptr = *(mptr+1);

		mptr++;
	}

	return shift_down;
}

void OpenLog(char** argv, int argc)
{
	if (!*LOG_FILE)
	{
		if (Config->logpath == "")
		{
			Config->logpath = GetFullProgDir(argv,argc) + "/ircd.log";
		}
	}
	else
	{
		Config->log_file = fopen(LOG_FILE,"a+");

		if (!Config->log_file)
		{
			printf("ERROR: Could not write to logfile %s, bailing!\n\n",Config->logpath.c_str());
			Exit(ERROR);
		}
		
		return;
	}

	Config->log_file = fopen(Config->logpath.c_str(),"a+");

	if (!Config->log_file)
	{
		printf("ERROR: Could not write to logfile %s, bailing!\n\n",Config->logpath.c_str());
		Exit(ERROR);
	}
}

void CheckRoot()
{
	if (geteuid() == 0)
	{
		printf("WARNING!!! You are running an irc server as ROOT!!! DO NOT DO THIS!!!\n\n");
		log(DEFAULT,"InspIRCd: startup: not starting with UID 0!");
		Exit(ERROR);
	}
}

void CheckDie()
{
	if (*Config->DieValue)
	{
		printf("WARNING: %s\n\n",Config->DieValue);
		log(DEFAULT,"Uh-Oh, somebody didn't read their config file: '%s'",Config->DieValue);
		Exit(ERROR);
	}
}

/* We must load the modules AFTER initializing the socket engine, now */
void LoadAllModules(InspIRCd* ServerInstance)
{
	char configToken[MAXBUF];
	Config->module_names.clear();
	MODCOUNT = -1;

	for (int count = 0; count < Config->ConfValueEnum(Config->config_data, "module"); count++)
	{
		Config->ConfValue(Config->config_data, "module","name",count,configToken,MAXBUF);
		printf("[\033[1;32m*\033[0m] Loading module:\t\033[1;32m%s\033[0m\n",configToken);
		
		if (!ServerInstance->LoadModule(configToken))		
		{
			log(DEFAULT,"Exiting due to a module loader error.");
			printf("\nThere was an error loading a module: %s\n\n",ServerInstance->ModuleError());
			Exit(0);
		}
	}
	
	log(DEFAULT,"Total loaded modules: %lu",(unsigned long)MODCOUNT+1);
}
