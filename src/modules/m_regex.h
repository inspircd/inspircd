/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


#ifndef M_REGEX_H
#define M_REGEX_H

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
