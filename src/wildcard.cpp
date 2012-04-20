/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2003, 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
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
#include "hashcomp.h"
#include "inspstring.h"

static bool match_internal(const unsigned char *str, const unsigned char *mask, unsigned const char *map)
{
	unsigned char *cp = NULL, *mp = NULL;
	unsigned char* string = (unsigned char*)str;
	unsigned char* wild = (unsigned char*)mask;

	while ((*string) && (*wild != '*'))
	{
		if ((map[*wild] != map[*string]) && (*wild != '?'))
		{
			return 0;
		}
		wild++;
		string++;
	}

	while (*string)
	{
		if (*wild == '*')
		{
			if (!*++wild)
			{
				return 1;
			}
			mp = wild;
			cp = string+1;
		}
		else
			if ((map[*wild] == map[*string]) || (*wild == '?'))
			{
				wild++;
				string++;
			}
			else
			{
				wild = mp;
				string = cp++;
			}

	}

	while (*wild == '*')
	{
		wild++;
	}

	return !*wild;
}

/********************************************************************
 * Below here is all wrappers around match_internal
 ********************************************************************/

CoreExport bool InspIRCd::Match(const std::string &str, const std::string &mask, unsigned const char *map)
{
	if (!map)
		map = national_case_insensitive_map;

	return match_internal((const unsigned char *)str.c_str(), (const unsigned char *)mask.c_str(), map);
}

CoreExport bool InspIRCd::Match(const  char *str, const char *mask, unsigned const char *map)
{
	if (!map)
		map = national_case_insensitive_map;
	return match_internal((const unsigned char *)str, (const unsigned char *)mask, map);
}

CoreExport bool InspIRCd::MatchCIDR(const std::string &str, const std::string &mask, unsigned const char *map)
{
	if (irc::sockets::MatchCIDR(str, mask, true))
		return true;

	if (!map)
		map = national_case_insensitive_map;

	// Fall back to regular match
	return InspIRCd::Match(str, mask, map);
}

CoreExport bool InspIRCd::MatchCIDR(const  char *str, const char *mask, unsigned const char *map)
{
	if (irc::sockets::MatchCIDR(str, mask, true))
		return true;

	if (!map)
		map = national_case_insensitive_map;

	// Fall back to regular match
	return InspIRCd::Match(str, mask, map);
}

