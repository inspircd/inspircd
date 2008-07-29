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

using irc::sockets::MatchCIDR;

/* Rewritten to operate on more effective C++ std::string types
 * rather than char* to avoid data copies.
 * - Brain
 */

CoreExport bool csmatch(const std::string &str, const std::string &mask)
{
	std::string::const_iterator cp, mp;

	//unsigned char *cp = NULL, *mp = NULL;
	//unsigned char* string = (unsigned char*)str;
	//unsigned char* wild = (unsigned char*)mask;

	std::string::const_iterator wild = mask.begin();
	std::string::const_iterator string = str.begin();

	if (mask.empty())
		return false;

	while ((string != str.end()) && (wild != mask.end()) && (*wild != '*'))
	{
		if ((*wild != *string) && (*wild != '?'))
			return 0;

		wild++;
		string++;
	}

	while (string != str.end())
	{
		if (wild != mask.end() && *wild == '*')
		{
			if (++wild == mask.end())
				return 1;

			mp = wild;
			cp = string;

			if (cp != str.end())
				cp++;
		}
		else
		if ((string != str.end() && wild != mask.end()) && ((*wild == *string) || (*wild == '?')))
		{
			wild++;
			string++;
		}
		else
		{
			wild = mp;
			if (cp == str.end())
				cp = str.end();
			else
				string = cp++;
		}

	}

	while ((wild != mask.end()) && (*wild == '*'))
		wild++;

	return wild == mask.end();
}

CoreExport bool match(const std::string &str, const std::string &mask)
{
	std::string::const_iterator cp, mp;
	std::string::const_iterator wild = mask.begin();
	std::string::const_iterator string = str.begin();

	if (mask.empty())
		return false;

	while ((string != str.end()) && (wild != mask.end()) && (*wild != '*'))
	{
		if ((lowermap[(unsigned char)*wild] != lowermap[(unsigned char)*string]) && (*wild != '?'))
			return 0;

		wild++;
		string++;
	}

	while (string != str.end())
	{
		if (wild != mask.end() && *wild == '*')
		{
			if (++wild == mask.end())
				return 1;

			mp = wild;
			cp = string;

			if (cp != str.end())
				cp++;

		}
		else
		if ((string != str.end() && wild != mask.end()) && ((lowermap[(unsigned char)*wild] == lowermap[(unsigned char)*string]) || (*wild == '?')))
		{
			wild++;
			string++;
		}
		else
		{
			wild = mp;
			if (cp == str.end())
				string = str.end();
			else
				string = cp++;
		}

	}

	while ((wild != mask.end()) && (*wild == '*'))
		wild++;

	return wild == mask.end();
}

/* Overloaded function that has the option of using cidr */
CoreExport bool match(const std::string &str, const std::string &mask, bool use_cidr_match)
{
	if (use_cidr_match && MatchCIDR(str, mask, true))
		return true;
	return match(str, mask);
}

CoreExport bool match(bool case_sensitive, const std::string &str, const std::string &mask, bool use_cidr_match)
{
	if (use_cidr_match && MatchCIDR(str, mask, true))
		return true;

	return case_sensitive ? csmatch(str, mask) : match(str, mask);
}

CoreExport bool match(bool case_sensitive, const std::string &str, const std::string &mask)
{
	return case_sensitive ? csmatch(str, mask) : match(str, mask);
}

