/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <stdarg.h>
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "wildcard.h"
#include "mode.h"
#include "xline.h"
#include "exitcodes.h"

static char TIMESTR[26];
static time_t LAST = 0;

/** Log()
 *  Write a line of text `text' to the logfile (and stdout, if in nofork) if the level `level'
 *  is greater than the configured loglevel.
 */
void InspIRCd::Log(int level, const char* text, ...)
{
	/* sanity check, just in case */
	if (!this->Config)
		return;

	/* Do this check again here so that we save pointless vsnprintf calls */
	if ((level < Config->LogLevel) && !Config->forcedebug)
		return;

	va_list argsPtr;
	char textbuffer[65536];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, 65536, text, argsPtr);
	va_end(argsPtr);

	this->Log(level, std::string(textbuffer));
}

void InspIRCd::Log(int level, const std::string &text)
{
	/* sanity check, just in case */
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
	std::string description;

	FOREACH_MOD_I(this,I_OnGetServerDescription,OnGetServerDescription(servername,description));

	if (!description.empty())
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
		if (IS_LOCAL(a) && a->IsModeSet('s'))
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

	if (!text || !modes || !flags)
	{
		this->Log(DEFAULT,"*** BUG *** WriteMode was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	modelen = strlen(modes);

	if (flags == WM_AND)
	{
		for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
		{
			userrec* t = *i;
			bool send_to_user = true;

			for (int n = 0; n < modelen; n++)
			{
				if (!t->IsModeSet(modes[n]))
				{
					send_to_user = false;
					break;
				}
			}
			if (send_to_user)
			{
				t->WriteServ("NOTICE %s :%s", t->nick, textbuffer);
			}
		}
	}
	else if (flags == WM_OR)
	{
		for (std::vector<userrec*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
		{
			userrec* t = *i;
			bool send_to_user = false;

			for (int n = 0; n < modelen; n++)
			{
				if (t->IsModeSet(modes[n]))
				{
					send_to_user = true;
					break;
				}
			}

			if (send_to_user)
			{
				t->WriteServ("NOTICE %s :%s", t->nick, textbuffer);
			}
		}
	}
}

/* Find a user record by nickname and return a pointer to it */
userrec* InspIRCd::FindNick(const std::string &nick)
{
	user_hash::iterator iter = clientlist->find(nick);

	if (iter == clientlist->end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

userrec* InspIRCd::FindNick(const char* nick)
{
	user_hash::iterator iter = clientlist->find(nick);
	
	if (iter == clientlist->end())
		return NULL;

	return iter->second;
}

/* find a channel record by channel name and return a pointer to it */
chanrec* InspIRCd::FindChan(const char* chan)
{
	chan_hash::iterator iter = chanlist->find(chan);

	if (iter == chanlist->end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

chanrec* InspIRCd::FindChan(const std::string &chan)
{
	chan_hash::iterator iter = chanlist->find(chan);

	if (iter == chanlist->end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

/* Send an error notice to all users, registered or not */
void InspIRCd::SendError(const std::string &s)
{
	for (std::vector<userrec*>::const_iterator i = this->local_users.begin(); i != this->local_users.end(); i++)
	{
		if ((*i)->registered == REG_ALL)
		{
		   	(*i)->WriteServ("NOTICE %s :%s",(*i)->nick,s.c_str());
	   	}
		else
		{
			/* Unregistered connections receive ERROR, not a NOTICE */
			(*i)->Write("ERROR :" + s);
		}
		/* This might generate a whole load of EAGAIN, but we dont really
		 * care about this, as if we call SendError something catastrophic
		 * has occured anyway, and we wont receive the events for these.
		 */
		(*i)->FlushWriteBuf();
	}
}

/* this function counts all users connected, wether they are registered or NOT. */
int InspIRCd::UserCount()
{
	return clientlist->size();
}

/* this counts only registered users, so that the percentages in /MAP don't mess up when users are sitting in an unregistered state */
int InspIRCd::RegisteredUserCount()
{
	return clientlist->size() - this->UnregisteredUserCount();
}

/* return how many users have a given mode e.g. 'a' */
int InspIRCd::ModeCount(const char mode)
{
	ModeHandler* mh = this->Modes->FindMode(mode, MODETYPE_USER);

	if (mh)
		return mh->GetCount();
	else
		return 0;
}

/* wrapper for readability */
int InspIRCd::InvisibleUserCount()
{
	return ModeCount('i');
}

/* return how many users are opered */
int InspIRCd::OperCount()
{
	return this->all_opers.size();
}

/* return how many users are unregistered */
int InspIRCd::UnregisteredUserCount()
{
	return this->unregistered_count;
}

/* return channel count */
long InspIRCd::ChannelCount()
{
	return chanlist->size();
}

/* return how many local registered users there are */
long InspIRCd::LocalUserCount()
{
	/* Doesnt count unregistered clients */
	return (local_users.size() - this->UnregisteredUserCount());
}

/* true for valid channel name, false else */
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

/* true for valid nickname, false else */
bool InspIRCd::IsNick(const char* n)
{
	if (!n || !*n)
		return false;
 
	int p = 0;
	for (char* i = (char*)n; *i; i++, p++)
	{
		if ((*i >= 'A') && (*i <= '}'))
		{
			/* "A"-"}" can occur anywhere in a nickname */
			continue;
		}

		if ((((*i >= '0') && (*i <= '9')) || (*i == '-')) && (i > n))
		{
			/* "0"-"9", "-" can occur anywhere BUT the first char of a nickname */
			continue;
		}

		/* invalid character! abort */
		return false;
	}

	/* too long? or not -- pointer arithmetic rocks */
	return (p < NICKMAX - 1);
}

/* return true for good ident, false else */
bool InspIRCd::IsIdent(const char* n)
{
	if (!n || !*n)
		return false;

	for (char* i = (char*)n; *i; i++)
	{
		if ((*i >= 'A') && (*i <= '}'))
		{
			continue;
		}

		if (((*i >= '0') && (*i <= '9')) || (*i == '-') || (*i == '.'))
		{
			continue;
		}

		return false;
	}

	return true;
}

/* open the proper logfile */
void InspIRCd::OpenLog(char** argv, int argc)
{
	Config->MyDir = Config->GetFullProgDir();

	if (!*this->LogFileName)
	{
		if (Config->logpath.empty())
		{
#ifndef DARWIN
			Config->logpath = Config->MyDir + "/ircd.log";
#else
			Config->logpath = "/var/log/ircd.log";
#endif
		}

		Config->log_file = fopen(Config->logpath.c_str(),"a+");
	}
	else
	{
		Config->log_file = fopen(this->LogFileName,"a+");
	}

	if (!Config->log_file)
	{
		printf("ERROR: Could not write to logfile %s: %s\n\n", Config->logpath.c_str(), strerror(errno));
		Exit(EXIT_STATUS_LOG);
	}

	this->Logger = new FileLogger(this, Config->log_file);
}

void InspIRCd::CheckRoot()
{
#ifndef DARWIN
	if (geteuid() == 0)
	{
		printf("WARNING!!! You are running an irc server as ROOT!!! DO NOT DO THIS!!!\n\n");
		this->Log(DEFAULT,"Cant start as root");
#else
	if (geteuid() != 16)
	{
		printf("WARNING!!! You are not running inspircd as the ircdaemon user!!! YOU CAN NOT DO THIS!!!\n\n");
		this->Log(DEFAULT,"Must start as user ircdaemon");
#endif
		Exit(EXIT_STATUS_ROOT);
	}
}

void InspIRCd::CheckDie()
{
	if (*Config->DieValue)
	{
		printf("WARNING: %s\n\n",Config->DieValue);
		this->Log(DEFAULT,"Died because of <die> tag: %s",Config->DieValue);
		Exit(EXIT_STATUS_DIETAG);
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
		Config->ConfValue(Config->config_data, "module", "name", count, configToken, MAXBUF);
		printf_c("[\033[1;32m*\033[0m] Loading module:\t\033[1;32m%s\033[0m\n",configToken);
		
		if (!this->LoadModule(configToken))		
		{
			this->Log(DEFAULT,"There was an error loading the module '%s': %s", configToken, this->ModuleError());
			printf_c("\n[\033[1;31m*\033[0m] There was an error loading the module '%s': %s\n\n", configToken, this->ModuleError());
			Exit(EXIT_STATUS_MODULE);
		}
	}
	printf_c("\nA total of \033[1;32m%d\033[0m module%s been loaded.\n", this->ModCount+1, this->ModCount+1 == 1 ? " has" : "s have");
	this->Log(DEFAULT,"Total loaded modules: %d", this->ModCount+1);
}

void InspIRCd::SendWhoisLine(userrec* user, userrec* dest, int numeric, const std::string &text)
{
	std::string copy_text = text;

	int MOD_RESULT = 0;
	FOREACH_RESULT_I(this, I_OnWhoisLine, OnWhoisLine(user, dest, numeric, copy_text));

	if (!MOD_RESULT)
		user->WriteServ("%d %s", numeric, copy_text.c_str());
}

void InspIRCd::SendWhoisLine(userrec* user, userrec* dest, int numeric, const char* format, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, format);
	vsnprintf(textbuffer, MAXBUF, format, argsPtr);
	va_end(argsPtr);

	this->SendWhoisLine(user, dest, numeric, std::string(textbuffer));
}

