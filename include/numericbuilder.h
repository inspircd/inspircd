/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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

namespace Numeric
{
	class WriteNumericSink;

	template <char Sep, bool SendEmpty, typename Sink>
	class GenericBuilder;

	template <char Sep = ',', bool SendEmpty = false>
	class Builder;
}

class Numeric::WriteNumericSink
{
	LocalUser* const user;

 public:
	WriteNumericSink(LocalUser* u)
		: user(u)
	{
	}

	void operator()(Numeric& numeric) const
	{
		user->WriteNumeric(numeric);
	}
};

template <char Sep, bool SendEmpty, typename Sink>
class Numeric::GenericBuilder
{
	Sink sink;
	Numeric numeric;
	const std::string::size_type max;

	bool HasRoom(const std::string::size_type additional) const
	{
		return (numeric.GetParams().back().size() + additional <= max);
	}

 public:
	GenericBuilder(Sink s, unsigned int num, bool addparam = true, size_t additionalsize = 0)
		: sink(s)
		, numeric(num)
		, max(ServerInstance->Config->Limits.MaxLine - ServerInstance->Config->ServerName.size() - additionalsize - 9)
	{
		if (addparam)
			numeric.push(std::string());
	}

	Numeric& GetNumeric() { return numeric; }

	void Add(const std::string& entry)
	{
		if (!HasRoom(entry.size()))
			Flush();
		numeric.GetParams().back().append(entry).push_back(Sep);
	}

	void Add(const std::string& entry1, const std::string& entry2)
	{
		if (!HasRoom(entry1.size() + entry2.size()))
			Flush();
		numeric.GetParams().back().append(entry1).append(entry2).push_back(Sep);
	}

	void Flush()
	{
		std::string& data = numeric.GetParams().back();
		if (IsEmpty())
		{
			if (!SendEmpty)
				return;
		}
		else
		{
			data.erase(data.size()-1);
		}

		sink(numeric);
		data.clear();
	}

	bool IsEmpty() const { return (numeric.GetParams().back().empty()); }
};

template <char Sep, bool SendEmpty>
class Numeric::Builder : public GenericBuilder<Sep, SendEmpty, WriteNumericSink>
{
 public:
	Builder(LocalUser* user, unsigned int num, bool addparam = true, size_t additionalsize = 0)
		: ::Numeric::GenericBuilder<Sep, SendEmpty, WriteNumericSink>(WriteNumericSink(user), num, addparam, additionalsize + user->nick.size())
	{
	}
};
