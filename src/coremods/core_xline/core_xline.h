/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017-2018, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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


#pragma once

#include "inspircd.h"
#include "timeutils.h"

class InsaneBan final
{
public:
	class MatcherBase
	{
	public:
		virtual long Run(const std::string& mask) = 0;
	};

	template <typename T>
	class Matcher
		: public MatcherBase
	{
	public:
		long Run(const std::string& mask) override
		{
			long matches = 0;
			auto c = static_cast<T*>(this);
			for (const auto& [_, user] : ServerInstance->Users.GetUsers())
			{
				if (c->Check(user, mask))
					matches++;
			}
			return matches;
		}
	};

	class IPHostMatcher final
		: public Matcher<IPHostMatcher>
	{
	public:
		bool Check(User* user, const std::string& mask) const;
	};

	/** Check if the given mask matches too many users according to the config, send an announcement if yes
	 * @param mask A mask to match against
	 * @param test The test that determines if a user matches the mask or not
	 * @param user A user whose nick will be included in the announcement if one is made
	 * @param bantype Type of the ban being set, will be used in the announcement if one is made
	 * @param confkey Name of the config key (inside the insane tag) which if false disables any checking
	 * @return True if the given mask matches too many users, false if not
	 */
	static bool MatchesEveryone(const std::string& mask, MatcherBase& test, User* user, char bantype, const char* confkey);
};

class CommandEline final
	: public Command
{
public:
	CommandEline(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
};

class CommandGline final
	: public Command
{
public:
	CommandGline(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
};

class CommandKline final
	: public Command
{
public:
	CommandKline(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
};

class CommandQline final
	: public Command
{
private:
	class NickMatcher final
		: public InsaneBan::Matcher<NickMatcher>
	{
	public:
		bool Check(User* user, const std::string& mask) const;
	};

public:
	CommandQline(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
};

class CommandZline final
	: public Command
{
private:
	class IPMatcher final
		: public InsaneBan::Matcher<IPMatcher>
	{
	public:
		bool Check(User* user, const std::string& mask) const;
	};

public:
	CommandZline(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
};
