/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
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

#include "event.h"

namespace Stats
{
	class Context;
	class EventListener;
	class Row;
}

class Stats::EventListener : public Events::ModuleEventListener
{
 public:
	EventListener(Module* mod)
		: ModuleEventListener(mod, "event/stats")
	{
	}

	/** Called when the STATS command is executed.
	 * @param stats Context of the /STATS request, contains requesting user, list of answer rows etc.
	 * @return MOD_RES_DENY if the stats request has been fulfilled. Otherwise, MOD_RES_PASSTHRU.
	 */
	virtual ModResult OnStats(Stats::Context& stats) = 0;
};

class Stats::Row : public Numeric::Numeric
{
 public:
	Row(unsigned int num)
		: Numeric(num)
	{
	}
};

class Stats::Context
{
	/** Source user of the STATS request
	 */
	User* const source;

	/** List of reply rows
	 */
	std::vector<Row> rows;

	/** Symbol indicating the type of this STATS request (usually a letter)
	 */
	const char symbol;

 public:
	/** Constructor
	 * @param src Source user of the STATS request, can be a local or remote user
	 * @param sym Symbol (letter) indicating the type of the request
	 */
	Context(User* src, char sym)
		: source(src)
		, symbol(sym)
	{
	}

	/** Get the source user of the STATS request
	 * @return Source user of the STATS request
	 */
	User* GetSource() const { return source; }

	/** Get the list of reply rows
	 * @return List of rows generated as reply for the request
	 */
	const std::vector<Row>& GetRows() const { return rows; }

	/** Get the symbol (letter) indicating what type of STATS was requested
	 * @return Symbol specified by the requesting user
	 */
	char GetSymbol() const { return symbol; }

	/** Add a row to the reply list
	 * @param row Reply to add
	 */
	void AddRow(const Row& row) { rows.push_back(row); }

	template <typename... Param>
	void AddRow(unsigned int numeric, Param... p)
	{
		Row n(numeric);
		n.push(std::forward<Param>(p)...);
		AddRow(n);
	}
};
