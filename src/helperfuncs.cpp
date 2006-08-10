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
#include "mode.h"
#include "xline.h"
#include "commands.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "inspircd.h"

extern int MODCOUNT;
extern ModuleList modules;
extern InspIRCd* ServerInstance;
extern time_t TIME;
extern char lowermap[255];
extern std::vector<userrec*> all_opers;

char LOG_FILE[MAXBUF];

static char TIMESTR[26];
static time_t LAST = 0;

/** Log()
 *  Write a line of text `text' to the logfile (and stdout, if in nofork) if the level `level'
 *  is greater than the configured loglevel.
 */
void InspIRCd::Log(int level, const char* text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	InspIRCd::Log(level, std::string(textbuffer));
}

void InspIRCd::Log(int level, const std::string &text)
{
	if (!ServerInstance || !ServerInstance->Config)
		return;

	/* If we were given -debug we output all messages, regardless of configured loglevel */
	if ((level < ServerInstance->Config->LogLevel) && !ServerInstance->Config->forcedebug)
		return;

	if (TIME != LAST)
	{
		struct tm *timeinfo = localtime(&TIME);

		strlcpy(TIMESTR,asctime(timeinfo),26);
		TIMESTR[24] = ':';
		LAST = TIME;
	}

	if (ServerInstance->Config->log_file && ServerInstance->Config->writelog)
	{
		fprintf(ServerInstance->Config->log_file,"%s %s\n",TIMESTR,text.c_str());
		fflush(ServerInstance->Config->log_file);
	}

	if (ServerInstance->Config->nofork)
	{
		printf("%s %s\n", TIMESTR, text.c_str());
	}
}

std::string InspIRCd::GetServerDescription(const char* servername)
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
void InspIRCd::WriteOpers(const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteOpers(std::string(textbuffer));
}

void InspIRCd::WriteOpers(const std::string &text)
{
	for (std::vector<userrec*>::iterator i = all_opers.begin(); i != all_opers.end(); i++)
	{
		userrec* a = *i;
		if (IS_LOCAL(a) && a->modes[UM_SERVERNOTICE])
		{
			// send server notices to all with +s
			a->WriteServ("NOTICE %s :%s",a->nick,text.c_str());
		}
	}
}

void InspIRCd::ServerNoticeAll(char* text, ...)
{
	if (!text)
		return;

	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,"NOTICE $%s :%s",ServerInstance->Config->ServerName,textbuffer);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = *i;
		t->WriteServ(std::string(formatbuffer));
	}
}

void InspIRCd::ServerPrivmsgAll(char* text, ...)
{
	if (!text)
		return;

	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,"PRIVMSG $%s :%s",ServerInstance->Config->ServerName,textbuffer);

	for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		userrec* t = *i;
		t->WriteServ(std::string(formatbuffer));
	}
}

void InspIRCd::WriteMode(const char* modes, int flags, const char* text, ...)
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

/* Find a user record by nickname and return a pointer to it */

userrec* InspIRCd::FindNick(const std::string &nick)
{
	user_hash::iterator iter = clientlist.find(nick);

	if (iter == clientlist.end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

userrec* InspIRCd::FindNick(const char* nick)
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

chanrec* InspIRCd::FindChan(const char* chan)
{
	chan_hash::iterator iter;

	if (!chan)
		return NULL;

	iter = chanlist.find(chan);

	if (iter == chanlist.end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

chanrec* InspIRCd::FindChan(const std::string &chan)
{
	chan_hash::iterator iter = chanlist.find(chan);

	if (iter == chanlist.end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}


/*
 * sends out an error notice to all connected clients (not to be used
 * lightly!)
 */
void InspIRCd::SendError(const char *s)
{
	for (std::vector<userrec*>::const_iterator i = this->local_users.begin(); i != this->local_users.end(); i++)
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
	ServerInstance->WriteOpers("*** SIGSEGV: Please see the ircd.log for backtrace and report the error to http://www.inspircd.org/bugtrack/");
#else
	log(DEFAULT,"You do not have execinfo.h so i could not backtrace -- on FreeBSD, please install the libexecinfo port.");
#endif
	ServerInstance->SendError("Somebody screwed up... Whoops. IRC Server terminating.");
	signal(SIGSEGV, SIG_DFL);
	if (raise(SIGSEGV) == -1)
	{
		log(DEFAULT,"What the hell, i couldnt re-raise SIGSEGV! Error: %s",strerror(errno));
	}
	Exit(status);
}

// this function counts all users connected, wether they are registered or NOT.
int InspIRCd::usercnt()
{
	return clientlist.size();
}

// this counts only registered users, so that the percentages in /MAP don't mess up when users are sitting in an unregistered state
int InspIRCd::registered_usercount()
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		c += (i->second->registered == REG_ALL);
	}

	return c;
}

int InspIRCd::usercount_invisible()
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		c += ((i->second->registered == REG_ALL) && (i->second->modes[UM_INVISIBLE]));
	}

	return c;
}

int InspIRCd::usercount_opers()
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (*(i->second->oper))
			c++;
	}
	return c;
}

int InspIRCd::usercount_unknown()
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

long InspIRCd::chancount()
{
	return chanlist.size();
}

long InspIRCd::local_count()
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
	if (!ServerInstance->Config->MOTD.size())
	{
		user->WriteServ("422 %s :Message of the day file is missing.",user->nick);
		return;
	}
	user->WriteServ("375 %s :%s message of the day", user->nick, ServerInstance->Config->ServerName);

	for (unsigned int i = 0; i < ServerInstance->Config->MOTD.size(); i++)
		user->WriteServ("372 %s :- %s",user->nick,ServerInstance->Config->MOTD[i].c_str());

	user->WriteServ("376 %s :End of message of the day.", user->nick);
}

void ShowRULES(userrec *user)
{
	if (!ServerInstance->Config->RULES.size())
	{
		user->WriteServ("NOTICE %s :Rules file is missing.",user->nick);
		return;
	}
	user->WriteServ("NOTICE %s :%s rules",user->nick,ServerInstance->Config->ServerName);

	for (unsigned int i = 0; i < ServerInstance->Config->RULES.size(); i++)
		user->WriteServ("NOTICE %s :%s",user->nick,ServerInstance->Config->RULES[i].c_str());

	user->WriteServ("NOTICE %s :End of %s rules.",user->nick,ServerInstance->Config->ServerName);
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

void OpenLog(char** argv, int argc)
{
	if (!*LOG_FILE)
	{
		if (ServerInstance->Config->logpath == "")
		{
			ServerInstance->Config->logpath = ServerConfig::GetFullProgDir(argv,argc) + "/ircd.log";
		}
	}
	else
	{
		ServerInstance->Config->log_file = fopen(LOG_FILE,"a+");

		if (!ServerInstance->Config->log_file)
		{
			printf("ERROR: Could not write to logfile %s, bailing!\n\n",ServerInstance->Config->logpath.c_str());
			Exit(ERROR);
		}
		
		return;
	}

	ServerInstance->Config->log_file = fopen(ServerInstance->Config->logpath.c_str(),"a+");

	if (!ServerInstance->Config->log_file)
	{
		printf("ERROR: Could not write to logfile %s, bailing!\n\n",ServerInstance->Config->logpath.c_str());
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
	if (*ServerInstance->Config->DieValue)
	{
		printf("WARNING: %s\n\n",ServerInstance->Config->DieValue);
		log(DEFAULT,"Uh-Oh, somebody didn't read their config file: '%s'",ServerInstance->Config->DieValue);
		Exit(ERROR);
	}
}

/* We must load the modules AFTER initializing the socket engine, now */
void LoadAllModules(InspIRCd* ServerInstance)
{
	char configToken[MAXBUF];
	ServerInstance->Config->module_names.clear();
	MODCOUNT = -1;

	for (int count = 0; count < ServerInstance->Config->ConfValueEnum(ServerInstance->Config->config_data, "module"); count++)
	{
		ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "module","name",count,configToken,MAXBUF);
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
