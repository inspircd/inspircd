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

/* Find a user record by nickname and return a pointer to it */
User* InspIRCd::FindNick(const std::string &nick)
{
	if (!nick.empty() && isdigit(*nick.begin()))
		return FindUUID(nick);
	return FindNickOnly(nick);
}

User* InspIRCd::FindNickOnly(const std::string &nick)
{
	user_hash::iterator iter = this->Users->clientlist.find(nick);

	if (iter == this->Users->clientlist.end())
		return NULL;

	return iter->second;
}

User *InspIRCd::FindUUID(const std::string &uid)
{
	user_hash::iterator finduuid = this->Users->uuidlist.find(uid);

	if (finduuid == this->Users->uuidlist.end())
		return NULL;

	return finduuid->second;
}
/* find a channel record by channel name and return a pointer to it */

Channel* InspIRCd::FindChan(const std::string &chan)
{
	chan_hash::iterator iter = chanlist.find(chan);

	if (iter == chanlist.end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
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

void InspIRCd::StripColor(std::string &sentence)
{
	/* refactor this completely due to SQUIT bug since the old code would strip last char and replace with \0 --peavey */
	int seq = 0;

	for (std::string::iterator i = sentence.begin(); i != sentence.end();)
	{
		if (*i == 3)
			seq = 1;
		else if (seq && (( ((*i >= '0') && (*i <= '9')) || (*i == ',') ) ))
		{
			seq++;
			if ( (seq <= 4) && (*i == ',') )
				seq = 1;
			else if (seq > 3)
				seq = 0;
		}
		else
			seq = 0;

		// Strip all control codes too except \001 for CTCP
		if (seq || ((*i >= 0) && (*i < 32) && (*i != 1)))
			i = sentence.erase(i);
		else
			++i;
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
	if (n.empty() || n.length() > ServerInstance->Config->Limits.NickMax)
		return false;

	for (std::string::const_iterator i = n.begin(); i != n.end(); ++i)
	{
		if ((*i >= 'A') && (*i <= '}'))
		{
			/* "A"-"}" can occur anywhere in a nickname */
			continue;
		}

		if ((((*i >= '0') && (*i <= '9')) || (*i == '-')) && (i != n.begin()))
		{
			/* "0"-"9", "-" can occur anywhere BUT the first char of a nickname */
			continue;
		}

		/* invalid character! abort */
		return false;
	}

	return true;
}

/* return true for good ident, false else */
bool IsIdentHandler::Call(const std::string& n)
{
	if (n.empty())
		return false;

	for (std::string::const_iterator i = n.begin(); i != n.end(); ++i)
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

/** Refactored by Brain, Jun 2009. Much faster with some clever O(1) array
 * lookups and pointer maths.
 */
unsigned long InspIRCd::Duration(const std::string &str)
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

const char* InspIRCd::Format(va_list &vaList, const char* formatString)
{
	static std::vector<char> formatBuffer(1024);

	while (true)
	{
		va_list dst;
		va_copy(dst, vaList);

		int vsnret = vsnprintf(&formatBuffer[0], formatBuffer.size(), formatString, dst);
		va_end(dst);

		if (vsnret > 0 && static_cast<unsigned>(vsnret) < formatBuffer.size())
		{
			break;
		}

		formatBuffer.resize(formatBuffer.size() * 2);
	}

	return &formatBuffer[0];
}

const char* InspIRCd::Format(const char* formatString, ...)
{
	const char* ret;
	VAFORMAT(ret, formatString, formatString);
	return ret;
}

std::string InspIRCd::TimeString(time_t curtime, const char* format, bool utc)
{
#ifdef _WIN32
	if (curtime < 0)
		curtime = 0;
#endif

	struct tm* timeinfo = utc ? gmtime(&curtime) : localtime(&curtime);
	if (!timeinfo)
	{
		curtime = 0;
		timeinfo = localtime(&curtime);
	}

	// If the calculated year exceeds four digits or is less than the year 1000,
	// the behavior of asctime() is undefined
	if (timeinfo->tm_year + 1900 > 9999)
		timeinfo->tm_year = 9999 - 1900;
	else if (timeinfo->tm_year + 1900 < 1000)
		timeinfo->tm_year = 0;

	// This is the default format used by asctime without the terminating new line.
	if (!format)
		format = "%a %b %d %H:%M:%S %Y";

	char buffer[512];
	if (!strftime(buffer, sizeof(buffer), format, timeinfo))
		buffer[0] = '\0';

	return buffer;
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
