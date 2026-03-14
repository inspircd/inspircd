/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2008 Craig Edwards <brain@inspircd.org>
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
#include "convto.h"
#include "stringutils.h"

/** The irc namespace contains a number of helper classes.
 */
namespace irc
{
	/** The portparser class separates out a port range into integers.
	 * A port range may be specified in the input string in the form
	 * "6660,6661,6662-6669,7020". The end of the stream is indicated by
	 * a return value of 0 from portparser::GetToken(). If you attempt
	 * to specify an illegal range (e.g. one where start >= end, or
	 * start or end < 0) then GetToken() will return the first element
	 * of the pair of numbers.
	 */
	class CoreExport portparser
	{
	private:

		/** Used to split on commas
		 */
		StringSplitter sep;

		/** Current position in a range of ports
		 */
		long in_range = 0;

		/** Starting port in a range of ports
		 */
		long range_begin = 0;

		/** Ending port in a range of ports
		 */
		long range_end = 0;

		/** Allow overlapped port ranges
		 */
		bool overlapped;

		/** Used to determine overlapping of ports
		 * without O(n) algorithm being used
		 */
		std::set<long> overlap_set;

		/** Returns true if val overlaps an existing range
		 */
		bool Overlaps(long val);
	public:

		/** Create a portparser and fill it with the provided data
		 * @param source The source text to parse from
		 * @param allow_overlapped Allow overlapped ranges
		 */
		portparser(const std::string& source, bool allow_overlapped = true);

		/** Fetch the next token from the stream
		 * @return The next port number is returned, or 0 if none remain
		 */
		long GetToken();
	};
}
