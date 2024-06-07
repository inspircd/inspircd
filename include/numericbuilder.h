/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015-2016 Attila Molnar <attilamolnar@hush.com>
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
	class WriteRemoteNumericSink;

	template <char Sep, bool SendEmpty, typename Sink>
	class GenericBuilder;

	template <char Sep = ',', bool SendEmpty = false>
	class Builder;

	template <unsigned int NumStaticParams, bool SendEmpty, typename Sink>
	class GenericParamBuilder;

	template <unsigned int NumStaticParams, bool SendEmpty = false>
	class ParamBuilder;
}

class Numeric::WriteNumericSink final
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

class Numeric::WriteRemoteNumericSink final
{
	User* const user;

public:
	WriteRemoteNumericSink(User* u)
		: user(u)
	{
	}

	void operator()(Numeric& numeric) const
	{
		user->WriteRemoteNumeric(numeric);
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
		, max(ServerInstance->Config->Limits.MaxLine - ServerInstance->Config->GetServerName().size() - additionalsize - 10)
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
			data.pop_back();
		}

		sink(numeric);
		data.clear();
	}

	bool IsEmpty() const { return (numeric.GetParams().back().empty()); }
};

template <char Sep, bool SendEmpty>
class Numeric::Builder
	: public GenericBuilder<Sep, SendEmpty, WriteNumericSink>
{
public:
	Builder(LocalUser* user, unsigned int num, bool addparam = true, size_t additionalsize = 0)
		: ::Numeric::GenericBuilder<Sep, SendEmpty, WriteNumericSink>(WriteNumericSink(user), num, addparam, additionalsize + user->nick.size())
	{
	}
};

template <unsigned int NumStaticParams, bool SendEmpty, typename Sink>
class Numeric::GenericParamBuilder
{
	Sink sink;
	Numeric numeric;
	std::string::size_type currlen = 0;
	std::string::size_type max;

	bool HasRoom(const std::string::size_type additional) const
	{
		return (currlen + additional <= max);
	}

public:
	GenericParamBuilder(Sink s, unsigned int num, size_t additionalsize)
		: sink(s)
		, numeric(num)
		, max(ServerInstance->Config->Limits.MaxLine - ServerInstance->Config->GetServerName().size() - additionalsize - 10)
	{
	}

	void AddStatic(const std::string& entry)
	{
		max -= (entry.length() + 1);
		numeric.GetParams().push_back(entry);
	}

	void Add(const std::string& entry)
	{
		if (!HasRoom(entry.size()))
			Flush();

		currlen += entry.size() + 1;
		numeric.GetParams().push_back(entry);
	}

	void Flush()
	{
		if ((!SendEmpty) && (IsEmpty()))
			return;

		sink(numeric);
		currlen = 0;
		numeric.GetParams().erase(numeric.GetParams().begin() + NumStaticParams, numeric.GetParams().end());
	}

	bool IsEmpty() const { return (numeric.GetParams().size() <= NumStaticParams); }
};

template <unsigned int NumStaticParams, bool SendEmpty>
class Numeric::ParamBuilder
	: public GenericParamBuilder<NumStaticParams, SendEmpty, WriteNumericSink>
{
public:
	ParamBuilder(LocalUser* user, unsigned int num)
		: ::Numeric::GenericParamBuilder<NumStaticParams, SendEmpty, WriteNumericSink>(WriteNumericSink(user), num, user->nick.size())
	{
	}
};
