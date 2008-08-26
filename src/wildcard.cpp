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

/*
 * Wildcard matching!
 *
 *  Iteration 1)
 *   Slow, horrible, etc.
 *	Iteration 2)
 *   The vastly available 'public domain' one
 *	Iteration 3)
 *   ZNC's, thought to be faster than ours, but it turned out that we could do better ;-)
 *	Iteration 4)
 *   Largely from work by peavey and myself (w00t) :)
 *	Iteration 5)
 *   peavey: Fix glob scan similar to 1.1, but scan ahead on glob in inner loop to retain speedup
 *   this fixes another case which we forgot to test. Add early return for obvious fail condition.
 */
static bool match_internal(const unsigned char *string, const unsigned char *wild, unsigned const char *map)
{
	const unsigned char *s, *m; m = wild;

	if (*string && !*wild)
		return false;

	if (!map)
		map = lowermap;

	while (*string)
	{
		if (*wild == '*')
		{
			while (*wild && *wild == '*')
				wild++;

			m = wild;

			if (!*wild)
				return true;
			else if (*wild != '?')
			{
				s = string;
				while (*s)
				{
					if ((map[*wild] == map[*s]))
					{
						string = s;
						if (*(wild+1) || !*(s+1))
						{
							wild++;
						}
						break;
					}
					s++;
				}
			}
		}
		else if ( (map[*wild] == map[*string]) || (*wild == '?') )
			wild++;
		else
			wild = m;

		string++;
	}

	while (*wild && *wild == '*')
		wild++;

	return !*wild;
}

/********************************************************************
 * Below here is all wrappers around match_internal
 ********************************************************************/

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

