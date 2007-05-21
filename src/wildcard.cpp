/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <string>
#include "hashcomp.h"
#include "inspstring.h"

using irc::sockets::MatchCIDR;

// Wed 27 Apr 2005 - Brain
// I've taken our our old wildcard routine -
// although comprehensive, it was topheavy and very
// slow, and ate masses of cpu when doing lots of
// comparisons. This is the 'de-facto' routine used
// by many, nobody really knows who wrote it first
// or what license its under, i've seen examples of it
// (unattributed to any author) all over the 'net.
// For now, we'll just consider this public domain.

CoreExport bool csmatch(const char *str, const char *mask)
{
	unsigned char *cp = NULL, *mp = NULL;
	unsigned char* string = (unsigned char*)str;
	unsigned char* wild = (unsigned char*)mask;

	while ((*string) && (*wild != '*'))
	{
		if ((*wild != *string) && (*wild != '?'))
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
		if ((*wild == *string) || (*wild == '?'))
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

CoreExport bool match(const char *str, const char *mask)
{
	unsigned char *cp = NULL, *mp = NULL;
	unsigned char* string = (unsigned char*)str;
	unsigned char* wild = (unsigned char*)mask;

	while ((*string) && (*wild != '*'))
	{
		if ((lowermap[*wild] != lowermap[*string]) && (*wild != '?'))
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
		if ((lowermap[*wild] == lowermap[*string]) || (*wild == '?'))
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

/* Overloaded function that has the option of using cidr */
CoreExport bool match(const char *str, const char *mask, bool use_cidr_match)
{
	if (use_cidr_match && MatchCIDR(str, mask, true))
		return true;
	return match(str, mask);
}

CoreExport bool match(bool case_sensitive, const char *str, const char *mask, bool use_cidr_match)
{
	if (use_cidr_match && MatchCIDR(str, mask, true))
		return true;
	return csmatch(str, mask);
}

CoreExport bool match(bool case_sensitive, const char *str, const char *mask)
{
	return case_sensitive ? csmatch(str, mask) : match(str, mask);
}

