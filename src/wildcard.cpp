/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <string>
#include "inspircd_config.h"
#include "inspircd.h"
#include "helperfuncs.h"
#include "inspstring.h"

// Wed 27 Apr 2005 - Brain
// I've taken our our old wildcard routine -
// although comprehensive, it was topheavy and very
// slow, and ate masses of cpu when doing lots of
// comparisons. This is the 'de-facto' routine used
// by many, nobody really knows who wrote it first
// or what license its under, i've seen examples of it
// (unattributed to any author) all over the 'net.
// For now, we'll just consider this public domain.

int wildcmp(char *wild, char *string)
{
	char *cp, *mp;
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

// This wrapper function is required to convert both
// strings to 'scandanavian lowercase' and make copies
// of them to a safe location. It also ensures we don't
// bite off more than we can chew with the length of
// the string.

bool match(const char* literal, const char* mask)
{
	static char L[10240];
	static char M[10240];
	strlcpy(L,literal,10240);
	strlcpy(M,mask,10240);
	strlower(L);
	strlower(M);
	return wildcmp(M,L);
}
