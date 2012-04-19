/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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
	InspIRCd* ServerInstance;

	// Constructor may as well be protected, as this class is abstract.
	Regex(const std::string& rx, InspIRCd* Me) : regex_string(rx), ServerInstance(Me)
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

class RegexFactoryRequest : public Request
{
private:
	std::string regex;

public:
	Regex* result;

	RegexFactoryRequest(Module* Me, Module* Target, const std::string& rx) : Request(Me, Target, "REGEX"), regex(rx), result(NULL)
	{
	}

	const std::string& GetRegex() const
	{
		return regex;
	}

	Regex* Create()
	{
		Send();
		return this->result;
	}
};

class RegexNameRequest : public Request
{
public:
	RegexNameRequest(Module* Me, Module* Target) : Request(Me, Target, "REGEX-NAME")
	{
	}
};

#endif
