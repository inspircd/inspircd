/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2014, 2017-2018, 2020, 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2018 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005, 2007 Craig Edwards <brain@inspircd.org>
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


#include <random>

#include "inspircd.h"
#include "timeutils.h"
#include "utility/string.h"
#include "xline.h"

bool InspIRCd::CheckPassword(const std::string& password, const std::string& passwordhash, const std::string& value)
{
	ModResult res;
	FIRST_MOD_RESULT(OnCheckPassword, res, (password, passwordhash, value));

	if (res == MOD_RES_ALLOW)
		return true; // Password explicitly valid.

	if (res == MOD_RES_DENY)
		return false; // Password explicitly invalid.

	// The hash algorithm wasn't recognised by any modules. If its plain
	// text then we can check it internally.
	if (passwordhash.empty() || insp::equalsci(passwordhash, "plaintext"))
		return TimingSafeCompare(password, value);

	// The password was invalid.
	return false;
}

bool InspIRCd::IsValidMask(const std::string& mask)
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

	if (mask.length() > ServerInstance->Config->Limits.GetMaxMask())
		return false;

	return true;
}

void InspIRCd::StripColor(std::string& sentence)
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

void InspIRCd::ProcessColors(std::vector<std::string>& input)
{
	/*
	 * Replace all color codes from the special[] array to actual
	 * color code chars using C++ style escape sequences. You
	 * can append other chars to replace if you like -- Justasic
	 */
	static struct special_chars final
	{
		std::string character;
		std::string replace;
		special_chars(const std::string& c, const std::string& r)
			: character(c)
			, replace(r)
		{
		}
	} special[] = {
		special_chars("\\b", "\x02"), // Bold
		special_chars("\\c", "\x03"), // Color
		special_chars("\\h", "\x04"), // Hex Color
		special_chars("\\i", "\x1D"), // Italic
		special_chars("\\m", "\x11"), // Monospace
		special_chars("\\r", "\x16"), // Reverse
		special_chars("\\s", "\x1E"), // Strikethrough
		special_chars("\\u", "\x1F"), // Underline
		special_chars("\\x", "\x0F"), // Reset
		special_chars("", "")
	};

	for (auto& ret : input)
	{
		for(int i = 0; !special[i].character.empty(); ++i)
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
	}
}

/* true for valid nickname, false else */
bool InspIRCd::DefaultIsNick(const std::string_view& n)
{
	if (n.empty() || n.length() > ServerInstance->Config->Limits.MaxNick)
		return false;

	for (std::string_view::const_iterator i = n.begin(); i != n.end(); ++i)
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

/* return true for good username, false else */
bool InspIRCd::DefaultIsUser(const std::string_view& n)
{
	if (n.empty())
		return false;

	for (const auto chr : n)
	{
		if (chr >= 'A' && chr <= '}')
			continue;

		if ((chr >= '0' && chr <= '9') || chr == '-' || chr == '.')
			continue;

		return false;
	}

	return true;
}

bool InspIRCd::IsHost(const std::string& host, bool allowsimple)
{
	// Hostnames must be non-empty and shorter than the maximum hostname length.
	if (host.empty() || host.length() > ServerInstance->Config->Limits.MaxHost)
		return false;

	unsigned int numdashes = 0;
	unsigned int numdots = 0;
	bool seendot = false;
	const std::string::const_iterator hostend = host.end() - 1;
	for (std::string::const_iterator iter = host.begin(); iter != host.end(); ++iter)
	{
		unsigned char chr = static_cast<unsigned char>(*iter);

		// If the current character is a label separator.
		if (chr == '.')
		{
			numdots++;

			// Consecutive separators are not allowed and dashes can not exist at the start or end
			// of labels and separators must only exist between labels.
			if (seendot || numdashes || iter == host.begin() || iter == hostend)
				return false;

			seendot = true;
			continue;
		}

		// If this point is reached then the character is not a dot.
		seendot = false;

		// If the current character is a dash.
		if (chr == '-')
		{
			// Consecutive separators are not allowed and dashes can not exist at the start or end
			// of labels and separators must only exist between labels.
			if (seendot || numdashes >= 2 || iter == host.begin() || iter == hostend)
				return false;

			numdashes += 1;
			continue;
		}

		// If this point is reached then the character is not a dash.
		numdashes = 0;

		// Alphanumeric characters are allowed at any position.
		if ((chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'Z') || (chr >= 'a' && chr <= 'z'))
			continue;

		return false;
	}

	// Whilst simple hostnames (e.g. localhost) are valid we do not allow the server to use
	// them to prevent issues with clients that differentiate between short client and server
	// prefixes by checking whether the nickname contains a dot.
	return numdots || allowsimple;
}

bool InspIRCd::IsSID(const std::string& str)
{
	/* Returns true if the string given is exactly 3 characters long,
	 * starts with a digit, and the other two characters are A-Z or digits
	 */
	return ((str.length() == 3) && isdigit(str[0]) &&
			((str[1] >= 'A' && str[1] <= 'Z') || isdigit(str[1])) &&
			((str[2] >= 'A' && str[2] <= 'Z') || isdigit(str[2])));
}

/** A lookup table of values for multiplier characters used by
 * Duration::{Try,}From(). In this lookup table, the indexes for
 * the ascii values 'm' and 'M' have the value '60', the indexes
 * for the ascii values 'D' and 'd' have a value of '86400', etc.
 */
static constexpr unsigned int duration_multi[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 86400, 0, 0, 0, 3600, 0, 0, 0, 0, 60, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 604800, 0, 31557600, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 86400, 0, 0, 0, 3600, 0, 0, 0, 0, 60, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 604800, 0, 31557600, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

bool Duration::TryFrom(const std::string& str, unsigned long& duration)
{
	unsigned long total = 0;
	unsigned long subtotal = 0;

	/* Iterate each item in the string, looking for number or multiplier */
	for (const auto chr : str)
	{
		/* Found a number, queue it onto the current number */
		if (chr >= '0' && chr <= '9')
		{
			subtotal = (subtotal * 10) + (chr - '0');
		}
		else
		{
			/* Found something that's not a number, find out how much
			 * it multiplies the built up number by, multiply the total
			 * and reset the built up number.
			 */
			unsigned int multiplier = duration_multi[static_cast<unsigned char>(chr)];
			if (multiplier == 0)
				return false;

			total += subtotal * multiplier;

			/* Next subtotal please */
			subtotal = 0;
		}
	}
	/* Any trailing values built up are treated as raw seconds */
	duration = total + subtotal;
	return true;
}

unsigned long Duration::From(const std::string& str)
{
	unsigned long out = 0;
	Duration::TryFrom(str, out);
	return out;
}

bool Duration::IsValid(const std::string& duration)
{
	for (const auto c : duration)
	{
		if (((c >= '0') && (c <= '9')))
			continue;

		if (!duration_multi[static_cast<unsigned char>(c)])
			return false;
	}
	return true;
}

std::string Duration::ToString(unsigned long duration)
{
	if (duration == 0)
		return "0s";

	std::string ret;

	unsigned long years = duration / 31449600;
	if (years)
		ret += ConvToStr(years) + "y";

	unsigned long weeks = (duration / 604800) % 52;
	if (weeks)
		ret += ConvToStr(weeks) + "w";

	unsigned long days = (duration / 86400) % 7;
	if (days)
		ret += ConvToStr(days) + "d";

	unsigned long hours = (duration / 3600) % 24;
	if (hours)
		ret += ConvToStr(hours) + "h";

	unsigned long minutes = (duration / 60) % 60;
	if (minutes)
		ret += ConvToStr(minutes) + "m";

	unsigned long seconds = duration % 60;
	if (seconds)
		ret += ConvToStr(seconds) + "s";

	return ret;
}

std::string Time::ToString(time_t curtime, const char* format, bool utc)
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
		format = "%a %b %d %Y %H:%M:%S";

	char buffer[512];
	if (!strftime(buffer, sizeof(buffer), format, timeinfo))
		buffer[0] = '\0';

	return buffer;
}

std::string InspIRCd::GenRandomStr(size_t length, bool printable) const
{
	std::vector<char> str(length);
	GenRandom(str.data(), length);
	if (printable)
		for (size_t i = 0; i < length; i++)
			str[i] = 0x3F + (str[i] & 0x3F);
	return std::string(str.data(), str.size());
}

// NOTE: this has a slight bias for lower values if max is not a power of 2.
// Don't use it if that matters.
unsigned long InspIRCd::GenRandomInt(unsigned long max) const
{
	unsigned long rv;
	GenRandom(reinterpret_cast<char*>(&rv), sizeof(rv));
	return rv % max;
}

// This is overridden by a higher-quality algorithm when TLS support is loaded
void InspIRCd::DefaultGenRandom(char* output, size_t max)
{
#if defined HAS_ARC4RANDOM_BUF
	arc4random_buf(output, max);
#else
	static std::random_device device;
	static std::mt19937 engine(device());
	static std::uniform_int_distribution<short> dist(CHAR_MIN, CHAR_MAX);
	for (size_t i = 0; i < max; ++i)
		output[i] = static_cast<char>(dist(engine));
#endif
}
