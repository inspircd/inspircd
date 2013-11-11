/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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


#ifdef _WIN32
#define _CRT_RAND_S
#include <stdlib.h>
#endif

#include "inspircd.h"
#include "xline.h"
#include "exitcodes.h"
#include <iostream>

std::string InspIRCd::GetServerDescription(const std::string& servername)
{
	std::string description;

	FOREACH_MOD(OnGetServerDescription, (servername,description));

	return !description.empty() ? description : Config->ServerDesc;
}

/* Find a user record by nickname and return a pointer to it */
User* InspIRCd::FindNick(const std::string &nick)
{
	if (!nick.empty() && isdigit(*nick.begin()))
		return FindUUID(nick);

	return FindNickOnly(nick);
}

User* InspIRCd::FindNickOnly(const std::string &nick)
{
	user_hash::iterator iter = this->Users->clientlist->find(nick);

	if (iter == this->Users->clientlist->end())
		return NULL;

	return iter->second;
}

User *InspIRCd::FindUUID(const std::string &uid)
{
	user_hash::iterator finduuid = this->Users->uuidlist->find(uid);

	if (finduuid == this->Users->uuidlist->end())
		return NULL;

	return finduuid->second;
}
/* find a channel record by channel name and return a pointer to it */

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
	for (LocalUserList::const_iterator i = this->Users->local_users.begin(); i != this->Users->local_users.end(); i++)
	{
		User* u = *i;
		if (u->registered == REG_ALL)
		{
			u->WriteNotice(s);
		}
		else
		{
			/* Unregistered connections receive ERROR, not a NOTICE */
			u->Write("ERROR :" + s);
		}
	}
}

std::string::const_iterator InspIRCd::ParseCharsetField(char *set, std::string::const_iterator str, std::string::const_iterator end) {
	while (str != end && strchr(set, *str)) {
		str++;
	}

	return str;
}

bool InspIRCd::IsValidMask(const std::string &mask)
{
	if (mask.length() > 250)
		return false;

	std::string::const_iterator i = mask.begin();
	char *valid_nick_char = "*-0123456789?ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}";
	char *valid_user_char = "*-.0123456789?ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}";
	char *valid_host_char = "*-.0123456789?ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";

	if (i == mask.end() || *i == '-' || isdigit(*i))
	{
		return false;
	}

	i = ParseCharsetField(valid_nick_char, i, mask.end());
	if (i == mask.end() || *i != '!')
	{
		return false;
	}

	i = ParseCharsetField(valid_user_char, i+1, mask.end());
	if (i == mask.end() || *i != '@')
	{
		return false;
	}

	i = ParseCharsetField(valid_host_char, i+1, mask.end());
	return i == mask.end();
}

void InspIRCd::StripColor(std::string &sentence)
{
	std::string::iterator i = sentence.begin();
	while (i != sentence.end())
	{
		/* XXX: These magic numbers should probably be placed somewhere more central (and this comment removed), so they can be reused in the function below.
		 *      Their corresponding functionality is:
		 * 0x02: Bold
		 * 0x03: Colour
		 * 0x0F: Stop colours
		 * 0x15: I don't know. It was in the previous function...
		 * 0x16: Reverse background/foreground
		 * 0x1D: Italics
		 * 0x1F: Underline
		 */

		switch (*i)
		{
			case 0x02:
			case 0x0F:
			case 0x15:
			case 0x16:
			case 0x1D:
			case 0x1F:
				i = sentence.erase(i);
				break;
			case 0x03:
				for (size_t x = 0; x < 3; x++)
				{
					i = sentence.erase(i);
					if (i == sentence.end()) { return; }
					if (!isdigit(*i)) { break; }
				}

				if (*i != ',') { break; }

				for (size_t x = 0; x < 3; x++)
				{
					i = sentence.erase(i);
					if (i == sentence.end()) { return; }
					if (!isdigit(*i)) { break; }
				}
				break;
			default:
				i++;
				break;
		}
	}
}

void InspIRCd::ProcessColors(file_cache& input)
{
	/*
	 * Replace all color codes from the special[] array to actual
	 * color code chars using C++ style escape sequences. You
	 * can append other chars to replace if you like -- Justasic
	 */
	static struct special_chars
	{
		std::string character;
		std::string replace;
		special_chars(const std::string &c, const std::string &r) : character(c), replace(r) { }
	}

	special[] = {
		special_chars("\\002", "\002"),  // Bold
		special_chars("\\037", "\037"),  // underline
		special_chars("\\003", "\003"),  // Color
		special_chars("\\017", "\017"), // Stop colors
		special_chars("\\u", "\037"),    // Alias for underline
		special_chars("\\b", "\002"),    // Alias for Bold
		special_chars("\\x", "\017"),    // Alias for stop
		special_chars("\\c", "\003"),    // Alias for color
		special_chars("", "")
	};

	for(file_cache::iterator it = input.begin(), it_end = input.end(); it != it_end; it++)
	{
		std::string ret = *it;
		for(int i = 0; special[i].character.empty() == false; ++i)
		{
			std::string::size_type pos = ret.find(special[i].character);
			if(pos == std::string::npos) // Couldn't find the character, skip this line
				continue;

			if((pos > 0) && (ret[pos-1] == '\\') && (ret[pos] == '\\'))
				continue; // Skip double slashes.

			// Replace all our characters in the array
			while(pos != std::string::npos)
			{
				ret = ret.substr(0, pos) + special[i].replace + ret.substr(pos + special[i].character.size());
				pos = ret.find(special[i].character, pos + special[i].replace.size());
			}
		}

		// Replace double slashes with a single slash before we return
		std::string::size_type pos = ret.find("\\\\");
		while(pos != std::string::npos)
		{
			ret = ret.substr(0, pos) + "\\" + ret.substr(pos + 2);
			pos = ret.find("\\\\", pos + 1);
		}
		*it = ret;
	}
}

/* true for valid channel name, false else */
bool IsChannelHandler::Call(const std::string& chname)
{
	if (chname.empty() || chname.length() > ServerInstance->Config->Limits.ChanMax)
		return false;

	if (chname[0] != '#')
		return false;

	for (std::string::const_iterator i = chname.begin()+1; i != chname.end(); ++i)
	{
		switch (*i)
		{
			case ' ':
			case ',':
			case 7:
				return false;
		}
	}

	return true;
}

/* true for valid nickname, false else */
bool IsNickHandler::Call(const std::string& n)
{
	char *valid_char = "-0123456789?ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}";

	if (n.empty() || n.length() > ServerInstance->Config->Limits.NickMax)
		return false;

	std::string::const_iterator i = n.begin();

	/* Nickname can't begin with '-' or '0'..'9'. */
	if (*i == '-' || isdigit(*i))
	{
		return false;
	}

	i = ParseCharsetField(valid_char, i, n.end());
	return i == n.end();
}

/* return true for good ident, false else */
bool IsIdentHandler::Call(const std::string& n)
{
	char *valid_char = "-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}"
	if (n.empty())
		return false;

	std::string::const_iterator i = n.begin();
	i = ParseCharsetField(valid_char, i, n.end());
	return i == n.end();
}

bool InspIRCd::IsSID(const std::string &str)
{
	/* Returns true if the string given is exactly 3 characters long,
	 * starts with a digit, and the other two characters are A-Z or digits
	 */
	return str.length() == 3 && isdigit(str[0]) &&
		(isupper(str[1]) || isdigit(str[1])) &&
		(isupper(str[2]) || isdigit(str[2]));
}

void InspIRCd::CheckRoot()
{
#ifndef _WIN32
	if (geteuid() == 0)
	{
		std::cout << "ERROR: You are running an irc server as root! DO NOT DO THIS!" << std::endl << std::endl;
		this->Logs->Log("STARTUP", LOG_DEFAULT, "Can't start as root");
		Exit(EXIT_STATUS_ROOT);
	}
#endif
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
	std::string textbuffer;
	VAFORMAT(textbuffer, format, format)
	this->SendWhoisLine(user, dest, numeric, textbuffer);
}

std::string::const_iterator InspIRCd::StringIteratorToUnsignedLong(unsigned long *destination, std::string::const_iterator source, std::string::const_iterator end)
{
	unsigned long d = 0;
	while (source != end && isdigit(*source))
	{
		d *= 10;
		d += *source++ - '0';
	}
	*destination = d;
	return source;
}

/*
 * Vanity and pride are different things, though the words are often used synonymously.
 * A person may be proud without being vain. Pride relates more to our opinion of
 * ourselves, vanity to what we would have others think of us.  -- Jane Austen
 */
unsigned long InspIRCd::Duration(const std::string &str)
{
	unsigned long total = 0;
	unsigned long subtotal = 0;
	std::string::const_iterator i = str.begin();

	i = StringIteratorToUnsignedLong(&subtotal, i, str.end());
	if (i == str.end() || *i != 'y')
		goto w;

	total += subtotal * 31557600UL;
	i = StringIteratorToUnsignedLong(&subtotal, i+1, str.end());
w:	if (i == str.end() || *i != 'w')
		goto d;

	total += subtotal * 604800UL;
	i = StringIteratorToUnsignedLong(&subtotal, i+1, str.end());
d:	if (i == str.end() || *i != 'd')
		goto h;

	total += subtotal * 86400UL;
	i = StringIteratorToUnsignedLong(&subtotal, i+1, str.end());
h:	if (i == str.end() || *i != 'h')
		goto m;

	total += subtotal * 3600UL;
	i = StringIteratorToUnsignedLong(&subtotal, i+1, str.end());
m:	if (i == str.end() || *i != 'm')
		goto s;

	total += subtotal * 60UL;
	i = StringIteratorToUnsignedLong(&subtotal, i+1, str.end());
s:	return total + subtotal;
}

const char* InspIRCd::Format(va_list &vaList, const char* formatString)
{
	static std::vector<char> formatBuffer(1024);

	while (true)
	{
		va_list dst;
		va_copy(dst, vaList);

		int vsnret = vsnprintf(&formatBuffer[0], formatBuffer.size(), formatString, dst);
		if (vsnret > 0 && static_cast<unsigned>(vsnret) < formatBuffer.size())
		{
			return &formatBuffer[0];
		}

		formatBuffer.resize(formatBuffer.size() * 2);
	}

	throw CoreException();
}

const char* InspIRCd::Format(const char* formatString, ...)
{
	const char* ret;
	VAFORMAT(ret, formatString, formatString);
	return ret;
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
#ifdef _WIN32
	if (curtime < 0)
		curtime = 0;
#endif

	struct tm *timeinfo = localtime(&curtime);
	if (!timeinfo)
	{
		curtime = 0;
		timeinfo = localtime(&curtime);
	}

	static std::vector<char> str(32);
	while (strftime(&str[0], str.size(), "%a %b %d %H:%M:%S %Y\n", timeinfo) == 0)
	{
		str.resize(str.size() * 2);
	}

	return std::string(str);
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
#ifdef _WIN32
	{
		unsigned int uTemp;
		if(rand_s(&uTemp) != 0)
			output[i] = rand();
		else
			output[i] = uTemp;
	}
#else
		output[i] = random();
#endif
}

ModResult OnCheckExemptionHandler::Call(User* user, Channel* chan, const std::string& restriction)
{
	unsigned int mypfx = chan->GetPrefixValue(user);
	char minmode = 0;
	std::string current;

	irc::spacesepstream defaultstream(ServerInstance->Config->ConfValue("options")->getString("exemptchanops"));

	while (defaultstream.GetToken(current))
	{
		std::string::size_type pos = current.find(':');
		if (pos == std::string::npos)
			continue;
		if (current.substr(0,pos) == restriction)
			minmode = current[pos+1];
	}

	PrefixMode* mh = ServerInstance->Modes->FindPrefixMode(minmode);
	if (mh && mypfx >= mh->GetPrefixRank())
		return MOD_RES_ALLOW;
	if (mh || minmode == '*')
		return MOD_RES_DENY;
	return MOD_RES_PASSTHRU;
}
