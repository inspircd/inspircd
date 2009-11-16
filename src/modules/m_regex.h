/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *          the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef _M_REGEX_H
#define _M_REGEX_H

#include "inspircd.h"

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

	virtual Regex* Create(const std::string& expr) = 0;
};

#endif
