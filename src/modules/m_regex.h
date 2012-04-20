/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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
