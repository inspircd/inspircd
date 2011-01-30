/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
#include "command_parse.h"
#include "xline.h"
#include "exitcodes.h"

std::string InspIRCd::GetServerDescription(const std::string& servername)
{
	std::string description;

	FOREACH_MOD(I_OnGetServerDescription,OnGetServerDescription(servername,description));

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
	for (std::vector<LocalUser*>::const_iterator i = this->Users->local_users.begin(); i != this->Users->local_users.end(); i++)
	{
		User* u = *i;
		if (u->registered == REG_ALL)
		{
		   	u->WriteServ("NOTICE %s :%s",u->nick.c_str(),s.c_str());
	   	}
		else
		{
			/* Unregistered connections receive ERROR, not a NOTICE */
			u->Write("ERROR :" + s);
		}
	}
}

/* return channel count */
long InspIRCd::ChannelCount()
{
	return chanlist->size();
}

bool InspIRCd::IsValidMask(const std::string &mask)
{
	const char* dest = mask.c_str();
	int exclamation = 0;
	int atsign = 0;

	for (const char* i = dest; *i; i++)
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

bool InspIRCd::IsSID(const std::string &str)
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
	if (!Config->cmdline.writelog) return true; // Skip opening default log if -nolog

	if (Config->cmdline.startup_log.empty())
		Config->cmdline.startup_log = "logs/startup.log";
	FILE* startup = fopen(Config->cmdline.startup_log.c_str(), "a+");

	if (!startup)
	{
		return false;
	}

	FileWriter* fw = new FileWriter(startup);
	FileLogStream *f = new FileLogStream((Config->cmdline.forcedebug ? DEBUG : DEFAULT), fw);

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

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnWhoisLine, MOD_RESULT, (user, dest, numeric, copy_text));

	if (MOD_RESULT != MOD_RES_DENY)
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

bool InspIRCd::ULine(const std::string& sserver)
{
	if (sserver.empty())
		return true;

	return (Config->ulines.find(sserver.c_str()) != Config->ulines.end());
}

bool InspIRCd::SilentULine(const std::string& sserver)
{
	std::map<irc::string,bool>::iterator n = Config->ulines.find(sserver.c_str());
	if (n != Config->ulines.end())
		return n->second;
	else
		return false;
}

std::string InspIRCd::TimeString(time_t curtime)
{
	return std::string(ctime(&curtime),24);
}

// You should only pass a single character to this.
void InspIRCd::AddExtBanChar(char c)
{
	std::string &tok = Config->data005;
	std::string::size_type ebpos = tok.find(" EXTBAN=,");

	if (ebpos == std::string::npos)
	{
		tok.append(" EXTBAN=,");
		tok.push_back(c);
	}
	else
	{
		ebpos += 9;
		while (isalpha(tok[ebpos]) && tok[ebpos] < c)
			ebpos++;
		tok.insert(ebpos, 1, c);
	}
}

std::string InspIRCd::GenRandomStr(int length, bool printable)
{
	char* buf = new char[length];
	GenRandom(buf, length);
	std::string rv;
	rv.resize(length);
	for(int i=0; i < length; i++)
		rv[i] = printable ? 0x3F + (buf[i] & 0x3F) : buf[i];
	delete[] buf;
	return rv;
}

// NOTE: this has a slight bias for lower values if max is not a power of 2.
// Don't use it if that matters.
unsigned long InspIRCd::GenRandomInt(unsigned long max)
{
	unsigned long rv;
	GenRandom((char*)&rv, sizeof(rv));
	return rv % max;
}

// This is overridden by a higher-quality algorithm when SSL support is loaded
void GenRandomHandler::Call(char *output, size_t max)
{
	for(unsigned int i=0; i < max; i++)
		output[i] = random();
}

ModResult InspIRCd::CheckExemption(User* user, Channel* chan, const std::string& restriction)
{
	PermissionData perm(user, "exempt/" + restriction, chan, NULL);
	FOR_EACH_MOD(OnPermissionCheck, (perm));
	if (perm.result != MOD_RES_PASSTHRU)
		return perm.result;

	unsigned int mypfx = chan->GetAccessRank(user);
	char minmode = 0;
	std::string current;

	irc::spacesepstream defaultstream(ServerInstance->Config->GetTag("options")->getString("exemptchanops"));

	while (defaultstream.GetToken(current))
	{
		std::string::size_type pos = current.find(':');
		if (pos == std::string::npos)
			continue;
		if (current.substr(0,pos) == restriction)
			minmode = current[pos+1];
	}

	ModeHandler* mh = ServerInstance->Modes->FindMode(minmode, MODETYPE_CHANNEL);
	if (mh && mypfx >= mh->GetPrefixRank())
		return MOD_RES_ALLOW;
	if (mh || minmode == '*')
		return MOD_RES_DENY;
	return MOD_RES_PASSTHRU;
}

void ModePermissionData::DoRankCheck()
{
	if (result != MOD_RES_PASSTHRU)
		return;

	ModeHandler* mh = ServerInstance->Modes->FindMode(mc.mode);

	unsigned int neededrank = mh->GetLevelRequired();

	/* Compare our rank on the channel against the rank of the required prefix,
	 * allow if >= ours. Because mIRC and xchat throw a tizz if the modes shown
	 * in NAMES(X) are not in rank order, we know the most powerful mode is listed
	 * first, so we don't need to iterate, we just look up the first instead.
	 */
	unsigned int ourrank = chan->GetAccessRank(source);
	Membership* memb = chan->GetUser(user);
	if(memb && ourrank < memb->GetProtectRank())
	{
		ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :They have a higher prefix set", chan->name.c_str());
		result = MOD_RES_DENY;
		return;
	}
	if (ourrank >= neededrank)
	{
		result = MOD_RES_ALLOW;
		return;
	}

	ModeHandler* neededmh = NULL;
	for(ModeIDIter id; id; id++)
	{
		ModeHandler* privmh = ServerInstance->Modes->FindMode(id);
		if (privmh && privmh->GetPrefixRank() >= neededrank)
		{
			// this mode is sufficient to allow this action
			if (!neededmh || privmh->GetPrefixRank() < neededmh->GetPrefixRank())
				neededmh = privmh;
		}
	}
	if (neededmh)
		ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :You must have channel %s access or above to %sset the %s channel mode",
			chan->name.c_str(), neededmh->name.c_str(), mc.adding ? "" : "un", mh->name.c_str());
	else
		ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :You cannot %sset the %s channel mode",
			chan->name.c_str(), mc.adding ? "" : "un", mh->name.c_str());
	result = MOD_RES_DENY;
}
