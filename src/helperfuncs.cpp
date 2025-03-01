/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2014, 2017-2018, 2020-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
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

#ifndef _WIN32
# include <unistd.h>
#endif

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

void InspIRCd::StripColor(std::string& line)
{
	for (size_t idx = 0; idx < line.length(); )
	{
		switch (line[idx])
		{
			case '\x02': // Bold
			case '\x1D': // Italic
			case '\x11': // Monospace
			case '\x16': // Reverse
			case '\x1E': // Strikethrough
			case '\x1F': // Underline
			case '\x0F': // Reset
				line.erase(idx, 1);
				break;

			case '\x03': // Color
			{
				auto start = idx;
				while (++idx < line.length() && idx - start < 6)
				{
					const auto chr = line[idx];
					if (chr != ',' && (chr < '0' || chr > '9'))
						break;
				}
				line.erase(start, idx - start);
				idx = start;
				break;
			}
			case '\x04': // Hex Color
			{
				auto start = idx;
				while (++idx < line.length() && idx - start < 14)
				{
					const auto chr = line[idx];
					if (chr != ',' && (chr < '0' || chr > '9') && (chr < 'A' || chr > 'F') && (chr < 'a' || chr > 'f'))
						break;
				}
				line.erase(start, idx - start);
				idx = start;
				break;
			}

			default: // Non-formatting character.
				idx++;
				break;
		}
	}
}

void InspIRCd::ProcessColors(std::string& line)
{
	static const insp::flat_map<std::string::value_type, std::string> formats = {
		{ '\\', "\\"   }, // Escape
		{ '{',  "{"    }, // Escape
		{ '}',  "}"    }, // Escape
		{ 'b',  "\x02" }, // Bold
		{ 'c',  "\x03" }, // Color
		{ 'h',  "\x04" }, // Hex Color
		{ 'i',  "\x1D" }, // Italic
		{ 'm',  "\x11" }, // Monospace
		{ 'r',  "\x16" }, // Reverse
		{ 's',  "\x1E" }, // Strikethrough
		{ 'u',  "\x1F" }, // Underline
		{ 'x',  "\x0F" }, // Reset
	};
	static const insp::flat_map<std::string, uint8_t, irc::insensitive_swo> colors = {
		{ "white",       0  },
		{ "black",       1  },
		{ "blue",        2  },
		{ "green",       3  },
		{ "red",         4  },
		{ "brown",       5  },
		{ "magenta",     6  },
		{ "orange",      7  },
		{ "yellow",      8  },
		{ "light green", 9  },
		{ "cyan",        10 },
		{ "light cyan",  11 },
		{ "light blue",  12 },
		{ "pink",        13 },
		{ "gray",        14 },
		{ "grey",        14 },
		{ "light gray",  15 },
		{ "light grey",  15 },
		{ "default",     99 },
	};

	for (size_t idx = 0; idx < line.length(); )
	{
		if (line[idx] != '\\')
		{
			// Regular character.
			idx++;
			continue;
		}

		auto start = idx;
		if (++idx >= line.length())
			continue; // Stray \ at the end of the string; skip.

		const auto chr = line[idx];
		const auto it = formats.find(chr);
		if (it == formats.end())
			continue; // Unknown escape, skip.

		line.replace(start, 2, it->second);
		idx = start + it->second.length();

		if (chr != 'c')
			continue; // Only colors can have values.

		start = idx;
		if (idx >= line.length() || line[idx] != '{')
			continue; // No color value.

		const auto fgend = line.find_first_of(",}", idx + 1);
		if (fgend == std::string::npos)
		{
			// Malformed color value, strip.
			line.erase(start);
			break;
		}

		size_t bgend = std::string::npos;
		if (line[fgend] == ',')
		{
			bgend = line.find_first_of('}', fgend + 1);
			if (bgend == std::string::npos)
			{
				// Malformed color value, strip.
				line.erase(start);
				break;
			}
		}

		const auto fg = colors.find(line.substr(start + 1, fgend - start - 1));
		auto tmp = ConvToStr(fg == colors.end() ? 99 : fg->second);
		if (bgend != std::string::npos)
		{
			const auto bg = colors.find(line.substr(fgend + 1, bgend - fgend - 1));
			tmp.push_back(',');
			tmp.append(ConvToStr(bg == colors.end() ? 99 : bg->second));
		}

		const auto end = bgend == std::string::npos ? fgend : bgend;
		line.replace(start, end - start + 1, tmp);
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

bool InspIRCd::IsHost(const std::string_view& host, bool allowsimple)
{
	// Hostnames must be non-empty and shorter than the maximum hostname length.
	if (host.empty() || host.length() > ServerInstance->Config->Limits.MaxHost)
		return false;

	unsigned int numdashes = 0;
	unsigned int numdots = 0;
	bool seendot = false;
	const auto hostend = host.end() - 1;
	for (auto iter = host.begin(); iter != host.end(); ++iter)
	{
		const auto chr = static_cast<unsigned char>(*iter);

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

bool InspIRCd::IsSID(const std::string_view& str)
{
	/* Returns true if the string given is exactly 3 characters long,
	 * starts with a digit, and the other two characters are A-Z or digits
	 */
	return ((str.length() == 3) && isdigit(str[0]) &&
			((str[1] >= 'A' && str[1] <= 'Z') || isdigit(str[1])) &&
			((str[2] >= 'A' && str[2] <= 'Z') || isdigit(str[2])));
}

namespace
{
	constexpr const auto SECONDS_PER_MINUTE = 60;

	constexpr const auto SECONDS_PER_HOUR = SECONDS_PER_MINUTE * 60;

	constexpr const auto SECONDS_PER_DAY = SECONDS_PER_HOUR * 24;

	constexpr const auto SECONDS_PER_WEEK = SECONDS_PER_DAY * 7;

	constexpr const auto SECONDS_PER_YEAR = (SECONDS_PER_DAY * 365);

	constexpr const auto SECONDS_PER_AVG_YEAR = SECONDS_PER_YEAR + (SECONDS_PER_HOUR * 6);
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
	0, 0, 0, 0, SECONDS_PER_DAY, 0, 0, 0, SECONDS_PER_HOUR, 0, 0, 0, 0, SECONDS_PER_MINUTE, 0, 0,
	0, 0, 0, 1, 0, 0, 0, SECONDS_PER_WEEK, 0, SECONDS_PER_AVG_YEAR, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, SECONDS_PER_DAY, 0, 0, 0, SECONDS_PER_HOUR, 0, 0, 0, 0, SECONDS_PER_MINUTE, 0, 0,
	0, 0, 0, 1,	0, 0, 0, SECONDS_PER_WEEK, 0, SECONDS_PER_AVG_YEAR, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

bool Duration::TryFrom(const std::string& str, unsigned long& duration, time_t base)
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

unsigned long Duration::From(const std::string& str, time_t base)
{
	unsigned long out = 0;
	Duration::TryFrom(str, out, base);
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

	const auto years = (duration / SECONDS_PER_YEAR);
	if (years)
	{
		ret = FMT::format("{}y", years);
		duration -= (years * SECONDS_PER_YEAR);
	}

	const auto weeks = (duration / SECONDS_PER_WEEK);
	if (weeks)
	{
		ret += FMT::format("{}w", weeks);
		duration -= (weeks * SECONDS_PER_WEEK);
	}

	const auto days = (duration / SECONDS_PER_DAY);
	if (days)
	{
		ret += FMT::format("{}d", days);
		duration -= (days * SECONDS_PER_DAY);
	}

	const auto hours = (duration / SECONDS_PER_HOUR);
	if (hours)
	{
		ret += FMT::format("{}h", hours);
		duration -= (hours * SECONDS_PER_HOUR);
	}

	const auto minutes = (duration / SECONDS_PER_MINUTE);
	if (minutes)
	{
		ret += FMT::format("{}m", minutes);
		duration -= (minutes * SECONDS_PER_MINUTE);
	}

	if (duration)
		ret += FMT::format("{}s", duration);

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

std::string InspIRCd::GenRandomStr(size_t length) const
{
	static const char chars[] = {
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
		'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
		'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	};

	std::string buf;
	buf.reserve(length);
	for (size_t idx = 0; idx < length; ++idx)
		buf.push_back(chars[GenRandomInt(std::size(chars))]);
	return buf;
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
#ifdef HAS_GETENTROPY
	if (getentropy(output, max) == 0)
		return;
#endif
#ifdef HAS_ARC4RANDOM_BUF
	arc4random_buf(output, max);
#else
	static std::random_device device;
	static std::mt19937 engine(device());
	static std::uniform_int_distribution<short> dist(CHAR_MIN, CHAR_MAX);
	for (size_t i = 0; i < max; ++i)
		output[i] = static_cast<char>(dist(engine));
#endif
}
