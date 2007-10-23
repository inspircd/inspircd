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

/* $Core: libIRCDhelper */

#include "inspircd.h"
#include <stdarg.h>
#include "wildcard.h"
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
	if (!this->Config || !this->Logger)
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
	if (!this->Config || !this->Logger)
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
	for (std::list<User*>::iterator i = this->all_opers.begin(); i != this->all_opers.end(); i++)
	{
		User* a = *i;
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

	for (std::vector<User*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		User* t = *i;
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

	for (std::vector<User*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		User* t = *i;
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
		for (std::vector<User*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
		{
			User* t = *i;
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
		for (std::vector<User*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
		{
			User* t = *i;
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
User* InspIRCd::FindNick(const std::string &nick)
{
	if (!nick.empty() && isdigit(*nick.begin()))
		return FindUUID(nick);

	user_hash::iterator iter = clientlist->find(nick);

	if (iter == clientlist->end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

User* InspIRCd::FindNick(const char* nick)
{
	if (isdigit(*nick))
		return FindUUID(nick);

	user_hash::iterator iter = clientlist->find(nick);
	
	if (iter == clientlist->end())
		return NULL;

	return iter->second;
}

User* InspIRCd::FindNickOnly(const std::string &nick)
{
	user_hash::iterator iter = clientlist->find(nick);

	if (iter == clientlist->end())
		return NULL;

	return iter->second;
}

User* InspIRCd::FindNickOnly(const char* nick)
{
	user_hash::iterator iter = clientlist->find(nick);

	if (iter == clientlist->end())
		return NULL;

	return iter->second;
}

User *InspIRCd::FindUUID(const std::string &uid)
{
	return FindUUID(uid.c_str());
}

User *InspIRCd::FindUUID(const char *uid)
{
	user_hash::iterator finduuid = uuidlist->find(uid);

	if (finduuid == uuidlist->end())
		return NULL;

	return finduuid->second;
}

/* find a channel record by channel name and return a pointer to it */
Channel* InspIRCd::FindChan(const char* chan)
{
	chan_hash::iterator iter = chanlist->find(chan);

	if (iter == chanlist->end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

Channel* InspIRCd::FindChan(const std::string &chan)
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
	for (std::vector<User*>::const_iterator i = this->local_users.begin(); i != this->local_users.end(); i++)
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

bool InspIRCd::IsValidMask(const std::string &mask)
{
	char* dest = (char*)mask.c_str();
	int exclamation = 0;
	int atsign = 0;

	for (char* i = dest; *i; i++)
	{
		/* out of range character, bad mask */
		if (*i < 32 || *i > 126)
		{
			return false;
		}

		switch (*i)
		{
			case '!':
				exclamation++;
				break;
			case '@':
				atsign++;
				break;
		}
	}

	/* valid masks only have 1 ! and @ */
	if (exclamation != 1 || atsign != 1)
		return false;

	return true;
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
bool IsNickHandler::Call(const char* n)
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
bool IsIdentHandler::Call(const char* n)
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
bool InspIRCd::OpenLog(char**, int)
{
	Config->MyDir = Config->GetFullProgDir();

	if (!*this->LogFileName)
	{
		if (Config->logpath.empty())
		{
			Config->logpath = Config->MyDir + "/ircd.log";
		}

		Config->log_file = fopen(Config->logpath.c_str(),"a+");
	}
	else
	{
		Config->log_file = fopen(this->LogFileName,"a+");
	}

	if (!Config->log_file)
	{
		this->Logger = NULL;
		return false;
	}

	this->Logger = new FileLogger(this, Config->log_file);
	return true;
}

void InspIRCd::CheckRoot()
{
	if (geteuid() == 0)
	{
		printf("WARNING!!! You are running an irc server as ROOT!!! DO NOT DO THIS!!!\n\n");
		this->Log(DEFAULT,"Cant start as root");
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

void InspIRCd::SendWhoisLine(User* user, User* dest, int numeric, const std::string &text)
{
	std::string copy_text = text;

	int MOD_RESULT = 0;
	FOREACH_RESULT_I(this, I_OnWhoisLine, OnWhoisLine(user, dest, numeric, copy_text));

	if (!MOD_RESULT)
		user->WriteServ("%d %s", numeric, copy_text.c_str());
}

void InspIRCd::SendWhoisLine(User* user, User* dest, int numeric, const char* format, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, format);
	vsnprintf(textbuffer, MAXBUF, format, argsPtr);
	va_end(argsPtr);

	this->SendWhoisLine(user, dest, numeric, std::string(textbuffer));
}

/** Refactored by Brain, Jun 2007. Much faster with some clever O(1) array
 * lookups and pointer maths.
 */
long InspIRCd::Duration(const std::string &str)
{
	unsigned char multiplier = 0;
	long total = 0;
	long times = 1;
	long subtotal = 0;

	/* Iterate each item in the string, looking for number or multiplier */
	for (std::string::const_reverse_iterator i = str.rbegin(); i != str.rend(); ++i)
	{
		/* Found a number, queue it onto the current number */
		if ((*i >= '0') && (*i <= '9'))
		{
			subtotal = subtotal + ((*i - '0') * times);
			times = times * 10;
		}
		else
		{
			/* Found something thats not a number, find out how much
			 * it multiplies the built up number by, multiply the total
			 * and reset the built up number.
			 */
			if (subtotal)
				total += subtotal * duration_multi[multiplier];

			/* Next subtotal please */
			subtotal = 0;
			multiplier = *i;
			times = 1;
		}
	}
	if (multiplier)
	{
		total += subtotal * duration_multi[multiplier];
		subtotal = 0;
	}
	/* Any trailing values built up are treated as raw seconds */
	return total + subtotal;
}

bool InspIRCd::ULine(const char* server)
{
	if (!server)
		return false;
	if (!*server)
		return true;

	return (Config->ulines.find(server) != Config->ulines.end());
}

bool InspIRCd::SilentULine(const char* server)
{
	std::map<irc::string,bool>::iterator n = Config->ulines.find(server);
	if (n != Config->ulines.end())
		return n->second;
	else return false;
}

std::string InspIRCd::TimeString(time_t curtime)
{
	return std::string(ctime(&curtime),24);
}

