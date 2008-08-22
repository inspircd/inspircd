/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
#include "hashcomp.h"
#include "inspstring.h"

#include <iostream>
/*
 * Wildcard matching, the third (and probably final) iteration!
 *
 */
static bool match_internal(const unsigned char *mask, const unsigned char *str, unsigned const char *map)
{
	const unsigned char *wild = str;
	const unsigned char *string = mask;
	const unsigned char *cp = NULL;
	const unsigned char *mp = NULL;

	if (!map)
		map = lowermap; // default to case insensitive search

	while ((*string) && (*wild != '*'))
	{
		if (map[*wild] != map[*string] && (*wild != '?'))
		{
			return false;
		}

		++wild;
		++string;
	}

	while (*string)
	{
		if (*wild == '*')
		{
			if (!*++wild)
			{
				return true;
			}

			mp = wild;
			cp = string+1;
		}

		// if mapped char == mapped wild AND wild is NOT ?
		else if (map[*wild] == map[*string] && (*wild == '?'))
		{
			++wild;
			++string;
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

	if (*wild == 0)
		std::cout << "*wild == 0\n";
	else
		std::cout << "*wild != 0\n";
	return (*wild == 0);
}

CoreExport bool InspIRCd::Match(const std::string &str, const std::string &mask, unsigned const char *map)
{
	return match_internal((const unsigned char *)str.c_str(), (const unsigned char *)mask.c_str(), map);
}

CoreExport bool InspIRCd::Match(const  char *str, const char *mask, unsigned const char *map)
{
	return match_internal((const unsigned char *)str, (const unsigned char *)mask, map);
}


CoreExport bool InspIRCd::MatchCIDR(const std::string &str, const std::string &mask, unsigned const char *map)
{
	if (irc::sockets::MatchCIDR(str, mask, true))
		return true;

	// Fall back to regular match
	return InspIRCd::Match(str, mask, NULL);
}

CoreExport bool InspIRCd::MatchCIDR(const  char *str, const char *mask, unsigned const char *map)
{
	if (irc::sockets::MatchCIDR(str, mask, true))
		return true;

	// Fall back to regular match
	return InspIRCd::Match(str, mask, NULL);
}

