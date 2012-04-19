/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
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

