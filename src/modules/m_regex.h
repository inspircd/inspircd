/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *          the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef M_REGEX_H
#define M_REGEX_H

#include "inspircd.h"

enum RegexFlags {
	REGEX_NONE = 0,
	REGEX_CASE_INSENSITIVE = 1,
	REGEX_IRC_LOWERCASE = 2,
	REGEX_SPACES_TO_UNDERSCORES = 4
};

class Regex : public classbase
{
protected:
	std::string regex_string; // The raw uncompiled regex string.

	// Constructor may as well be protected, as this class is abstract.
	Regex(const std::string& rx) : regex_string(rx)
	{
	}

public:

	virtual ~Regex()
	{
	}

	virtual bool Matches(const std::string& text) = 0;

	const std::string& GetRegexString() const
	{
		return regex_string;
	}
};

class RegexFactory : public DataProvider
{
 public:
	RegexFactory(Module* Creator, const std::string& Name) : DataProvider(Creator, Name) {}

	virtual Regex* Create(const std::string& expr, RegexFlags flags) = 0;
};

#endif
