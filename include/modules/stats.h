/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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

namespace Stats
{
	class Context;
	class EventListener;
	class Row;
}

enum
{
	// From aircd.
	RPL_STATS = 210,
};

class Stats::EventListener
	: public Events::ModuleEventListener
{
public:
	EventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/stats", eventprio)
	{
	}

	/** Called when the STATS command is executed.
	 * @param stats Context of the /STATS request, contains requesting user, list of answer rows etc.
	 * @return MOD_RES_DENY if the stats request has been fulfilled. Otherwise, MOD_RES_PASSTHRU.
	 */
	virtual ModResult OnStats(Stats::Context& stats) = 0;
};

class Stats::Row final
	: public Numeric::Numeric
{
public:
	Row(unsigned int num)
		: Numeric(num)
	{
	}

	/** Attaches a stats tag to the response.
	 * @param stats The stats request that is being responded to.
	 * @param name The name of the stats tag. Will be prefixed with `inspircd.org/stats-`.
	 * @param value The value of the stats tag. Will be escaped before attaching.
	 */
	inline Row& AddTag(Stats::Context& stats, const std::string& name, const std::string& value);

	/** Attaches multiple tags to the response.
	 * @param stats The stats request that is being responded to.
	 * @param tags An list of tags to attach. See AddTag for how this will be processed.
	 */
	inline Row& AddTags(Stats::Context& stats, std::initializer_list<std::pair<std::string, std::string>>&& tags)
	{
		for (const auto& [name, value] : tags)
			AddTag(stats, name, value);
		return *this;
	}
};

class Stats::Context final
{
	/** Source user of the STATS request
	 */
	User* const source;

	/** The provider for inspircd.org/stats-* tags. */
	ClientProtocol::MessageTagProvider& tagprov;

	/** List of reply rows
	 */
	std::vector<Row> rows;

	/** Symbol indicating the type of this STATS request (usually a letter)
	 */
	const char symbol;

public:
	/** Constructor
	 * @param prov Provider for the inspircd.org/stats-* tags.
	 * @param src Source user of the STATS request, can be a local or remote user
	 * @param sym Symbol (letter) indicating the type of the request
	 */
	Context(ClientProtocol::MessageTagProvider& prov, User* src, char sym)
		: source(src)
		, tagprov(prov)
		, symbol(sym)
	{
	}

	/** Retrieves the provider of inspircd.org/stats-* tags. */
	auto& GetTagProvider() const { return tagprov; }

	/** Get the source user of the STATS request
	 * @return Source user of the STATS request
	 */
	User* GetSource() const { return source; }

	/** Get the list of reply rows
	 * @return List of rows generated as reply for the request
	 */
	const std::vector<Row>& GetRows() const { return rows; }
	std::vector<Row>& GetRows() { return rows; }

	/** Get the symbol (letter) indicating what type of STATS was requested
	 * @return Symbol specified by the requesting user
	 */
	char GetSymbol() const { return symbol; }

	/** Add a row to the reply list
	 * @param row Reply to add
	 */
	Row& AddRow(const Row& row)
	{
		rows.push_back(row);
		return rows.back();
	}

	template <typename... Param>
	Row& AddRow(unsigned int numeric, Param&&... p)
	{
		Row n(numeric);
		n.push(std::forward<Param>(p)...);
		return AddRow(n);
	}

	/** Adds a row to the stats response using a generic numeric.
	 * @param p One or more fields to add to the response.
	 */
	template <typename... Param>
	Row& AddGenericRow(Param&&... p)
	{
		Row n(RPL_STATS);
		n.push(GetSymbol());
		n.push(std::forward<Param>(p)...);
		return AddRow(n);
	}
};

inline Stats::Row& Stats::Row::AddTag(Stats::Context& stats, const std::string& name, const std::string& value)
{
	Numeric::Numeric::AddTag("inspircd.org/stats-" + name, &stats.GetTagProvider(), ClientProtocol::Message::EscapeTag(value));
	return *this;
}
