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
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "wildcard.h"
#include "mode.h"
#include "xline.h"
#include "inspircd.h"

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

	this->Log(level, std::string(textbuffer));
}

void InspIRCd::Log(int level, const std::string &text)
{
	if (!this->Config)
		return;

	/* If we were given -debug we output all messages, regardless of configured loglevel */
	if ((level < Config->LogLevel) && !Config->forcedebug)
		return;

	if (Time() != LAST)
	{
		time_t local = Time();
		struct tm *timeinfo = localtime(&local);

		strlcpy(TIMESTR,asctime(timeinfo),26);
		TIMESTR[24] = ':';
		LAST = Time();
	}

	if (Config->log_file && Config->writelog)
	{
		std::string out = std::string(TIMESTR) + " " + text.c_str() + "\n";
		this->Logger->WriteLogLine(out);
	}

	if (Config->nofork)
	{
		printf("%s %s\n", TIMESTR, text.c_str());
	}
}

std::string InspIRCd::GetServerDescription(const char* servername)
{
	std::string description = "";

	FOREACH_MOD_I(this,I_OnGetServerDescription,OnGetServerDescription(servername,description));

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
	for (std::vector<userrec*>::iterator i = this->all_opers.begin(); i != this->all_opers.end(); i++)
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

	snprintf(formatbuffer,MAXBUF,"NOTICE $%s :%s",Config->ServerName,textbuffer);

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

	snprintf(formatbuffer,MAXBUF,"PRIVMSG $%s :%s",Config->ServerName,textbuffer);

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
		this->Log(DEFAULT,"*** BUG *** WriteMode was given an invalid parameter");
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

// this function counts all users connected, wether they are registered or NOT.
int InspIRCd::UserCount()
{
	return clientlist.size();
}

// this counts only registered users, so that the percentages in /MAP don't mess up when users are sitting in an unregistered state
int InspIRCd::RegisteredUserCount()
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		c += (i->second->registered == REG_ALL);
	}

	return c;
}

int InspIRCd::InvisibleUserCount()
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		c += ((i->second->registered == REG_ALL) && (i->second->modes[UM_INVISIBLE]));
	}

	return c;
}

int InspIRCd::OperCount()
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (*(i->second->oper))
			c++;
	}
	return c;
}

int InspIRCd::UnregisteredUserCount()
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

long InspIRCd::ChannelCount()
{
	return chanlist.size();
}

long InspIRCd::LocalUserCount()
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

bool InspIRCd::IsChannel(const char *chname)
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

void InspIRCd::OpenLog(char** argv, int argc)
{
	if (!*this->LogFileName)
	{
		if (Config->logpath == "")
		{
			Config->logpath = ServerConfig::GetFullProgDir(argv,argc) + "/ircd.log";
		}
	}
	else
	{
		Config->log_file = fopen(this->LogFileName,"a+");

		if (!Config->log_file)
		{
			printf("ERROR: Could not write to logfile %s, bailing!\n\n",Config->logpath.c_str());
			Exit(ERROR);
		}

		this->Logger = new FileLogger(this, Config->log_file);
		return;
	}

	Config->log_file = fopen(Config->logpath.c_str(),"a+");

	if (!Config->log_file)
	{
		printf("ERROR: Could not write to logfile %s, bailing!\n\n",Config->logpath.c_str());
		Exit(ERROR);
	}

	this->Logger = new FileLogger(this, Config->log_file);
}

void InspIRCd::CheckRoot()
{
	if (geteuid() == 0)
	{
		printf("WARNING!!! You are running an irc server as ROOT!!! DO NOT DO THIS!!!\n\n");
		this->Log(DEFAULT,"Cant start as root");
		Exit(ERROR);
	}
}

void InspIRCd::CheckDie()
{
	if (*Config->DieValue)
	{
		printf("WARNING: %s\n\n",Config->DieValue);
		this->Log(DEFAULT,"Died because of <die> tag: %s",Config->DieValue);
		Exit(ERROR);
	}
}

/* We must load the modules AFTER initializing the socket engine, now */
void InspIRCd::LoadAllModules()
{
	char configToken[MAXBUF];
	Config->module_names.clear();
	this->ModCount = -1;

	for (int count = 0; count < Config->ConfValueEnum(Config->config_data, "module"); count++)
	{
		Config->ConfValue(Config->config_data, "module","name",count,configToken,MAXBUF);
		printf("[\033[1;32m*\033[0m] Loading module:\t\033[1;32m%s\033[0m\n",configToken);
		
		if (!this->LoadModule(configToken))		
		{
			this->Log(DEFAULT,"There was an error loading a module: %s", this->ModuleError());
			printf("\nThere was an error loading a module: %s\n\n",this->ModuleError());
			Exit(ERROR);
		}
	}
	printf("\nA total of \033[1;32m%d\033[0m module%s been loaded.\n", this->ModCount+1, this->ModCount+1 == 1 ? " has" : "s have");
	this->Log(DEFAULT,"Total loaded modules: %d", this->ModCount+1);
}

