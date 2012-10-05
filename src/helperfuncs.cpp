/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2006-2007 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/* $Core */

#include "inspircd.h"
#include "xline.h"
#include "exitcodes.h"

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

/* Find a user record by nickname and return a pointer to it */
User* InspIRCd::FindNick(const std::string &nick)
{
	if (!nick.empty() && isdigit(*nick.begin()))
		return FindUUID(nick);

	user_hash::iterator iter = this->Users->clientlist->find(nick);

	if (iter == this->Users->clientlist->end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

User* InspIRCd::FindNick(const char* nick)
{
	if (isdigit(*nick))
		return FindUUID(nick);

	user_hash::iterator iter = this->Users->clientlist->find(nick);

	if (iter == this->Users->clientlist->end())
		return NULL;

	return iter->second;
}

User* InspIRCd::FindNickOnly(const std::string &nick)
{
	user_hash::iterator iter = this->Users->clientlist->find(nick);

	if (iter == this->Users->clientlist->end())
		return NULL;

	return iter->second;
}

User* InspIRCd::FindNickOnly(const char* nick)
{
	user_hash::iterator iter = this->Users->clientlist->find(nick);

	if (iter == this->Users->clientlist->end())
		return NULL;

	return iter->second;
}

User *InspIRCd::FindUUID(const std::string &uid)
{
	return FindUUID(uid.c_str());
}

User *InspIRCd::FindUUID(const char *uid)
{
	user_hash::iterator finduuid = this->Users->uuidlist->find(uid);

	if (finduuid == this->Users->uuidlist->end())
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
	for (std::vector<User*>::const_iterator i = this->Users->local_users.begin(); i != this->Users->local_users.end(); i++)
	{
		if ((*i)->registered == REG_ALL)
		{
		   	(*i)->WriteServ("NOTICE %s :%s",(*i)->nick.c_str(),s.c_str());
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

/* return channel count */
long InspIRCd::ChannelCount()
{
	return chanlist->size();
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
	if (mask.length() > 250)
		return false;

	return true;
}

/* true for valid channel name, false else */
bool IsChannelHandler::Call(const char *chname, size_t max)
{
	const char *c = chname + 1;

	/* check for no name - don't check for !*chname, as if it is empty, it won't be '#'! */
	if (!chname || *chname != '#')
	{
		return false;
	}

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

	size_t len = c - chname;
	/* too long a name - note funky pointer arithmetic here. */
	if (len > max)
	{
			return false;
	}

	return true;
}

/* true for valid nickname, false else */
bool IsNickHandler::Call(const char* n, size_t max)
{
	if (!n || !*n)
		return false;

	unsigned int p = 0;
	for (const char* i = n; *i; i++, p++)
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
	return (p < max);
}

/* return true for good ident, false else */
bool IsIdentHandler::Call(const char* n)
{
	if (!n || !*n)
		return false;

	for (const char* i = n; *i; i++)
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

bool IsSIDHandler::Call(const std::string &str)
{
	/* Returns true if the string given is exactly 3 characters long,
	 * starts with a digit, and the other two characters are A-Z or digits
	 */
	return ((str.length() == 3) && isdigit(str[0]) &&
			((str[1] >= 'A' && str[1] <= 'Z') || isdigit(str[1])) &&
			 ((str[2] >= 'A' && str[2] <= 'Z') || isdigit(str[2])));
}

/* open the proper logfile */
bool InspIRCd::OpenLog(char**, int)
{
	/* This function only happens at startup now */
	if (Config->nofork)
	{
		this->Logs->SetupNoFork();
	}
	Config->MyDir = Config->GetFullProgDir();

	/* Attempt to find home directory, portable to windows */
	const char* home = getenv("HOME");
	if (!home)
	{
		/* No $HOME, log to %USERPROFILE% */
		home = getenv("USERPROFILE");
		if (!home)
		{
			/* Nothing could be found at all, log to current dir */
			Config->logpath = "./startup.log";
		}
	}

	if (!Config->writelog) return true; // Skip opening default log if -nolog

	if (!*this->LogFileName)
	{
		if (Config->logpath.empty())
		{
			Config->logpath = "./startup.log";
		}

		if (!Config->log_file)
			Config->log_file = fopen(Config->logpath.c_str(),"a+");
	}
	else
	{
		Config->log_file = fopen(this->LogFileName,"a+");
	}

	if (!Config->log_file)
	{
		return false;
	}

	FileWriter* fw = new FileWriter(this, Config->log_file);
	FileLogStream *f = new FileLogStream(this, (Config->forcedebug ? DEBUG : DEFAULT), fw);

	this->Logs->AddLogType("*", f, true);

	return true;
}

void InspIRCd::CheckRoot()
{
	if (geteuid() == 0)
	{
		printf("WARNING!!! You are running an irc server as ROOT!!! DO NOT DO THIS!!!\n\n");
		this->Logs->Log("STARTUP",DEFAULT,"Cant start as root");
		Exit(EXIT_STATUS_ROOT);
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

/** Refactored by Brain, Jun 2009. Much faster with some clever O(1) array
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

bool InspIRCd::ULine(const char* sserver)
{
	if (!sserver)
		return false;
	if (!*sserver)
		return true;

	return (Config->ulines.find(sserver) != Config->ulines.end());
}

bool InspIRCd::SilentULine(const char* sserver)
{
	std::map<irc::string,bool>::iterator n = Config->ulines.find(sserver);
	if (n != Config->ulines.end())
		return n->second;
	else return false;
}

const std::string &InspIRCd::TimeString(time_t curtime)
{
	static std::string buf;
	buf.assign(ctime(&curtime), 24);
	return buf;
}

// You should only pass a single character to this.
void InspIRCd::AddExtBanChar(char c)
{
	std::string &tok = Config->data005;
	std::string::size_type ebpos;

	if ((ebpos = tok.find(" EXTBAN=,")) == std::string::npos)
	{
		tok.append(" EXTBAN=,");
		tok.push_back(c);
	}
	else
		tok.insert(ebpos + 9, 1, c);
}
