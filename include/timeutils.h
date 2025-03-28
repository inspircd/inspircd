/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023-2025 Sadie Powell <sadie@witchery.services>
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

namespace Duration
{
	/** Converts a string containing a duration to the number of seconds it
	 * represents returning 0 on error.
	 *
	 * The string should should be in the format 1y2w3d4h6m5s which represents
	 * one year, two weeks, three days, four hours, six minutes, and five
	 * seconds. If called with this duration 33,019,565 will be returned.
	 *
	 * @param str A string containing a duration.
	 * @return Either the number of seconds in the duration or 0 on error.
	 */
	CoreExport unsigned long From(const std::string& str);

	/** Determines whether a duration string is valid.
	 * @param str The duration string to check.
	 */
	CoreExport bool IsValid(const std::string& str);

	/** Converts a number of seconds to a human-readable duration string.
	 *
	 * e.g. 33,019,565 will result in 1 year, 2 weeks, 3 days, 4 hours, 6
	 * minutes, 5 seconds.
	 *
	 * @param duration The number of seconds to convert.
	 * @param brief Whether to round to the nearest time period.
	 */
	CoreExport std::string ToLongString(unsigned long duration, bool brief = false);

	/** Converts a number of seconds to a duration string.
	 *
	 * e.g. 33,019,565 will result in 1y2w3d4h6m5s which represents one year,
	 * two weeks, three days, four hours, six minutes, and five seconds.
	 *
	 * @param duration The number of seconds to convert.
	 */
	CoreExport std::string ToString(unsigned long duration);

	/** Attempts to converts a string containing a duration to the number of
	 * seconds it represents returning whether the conversion succeeded.
	 *
	 * The string should should be in the format 1y2w3d4h6m5s which represents
	 * one year, two weeks, three days, four hours, six minutes, and five
	 * seconds. If called with this duration 33,019,565 will be returned.
	 *
	 * @param str A string containing a duration.
	 * @param duration The location to store the resulting duration.
	 * @param base The base time to use for leap year calculation.
	 * @return True if the conversion succeeded; otherwise, false.
	 */
	CoreExport bool TryFrom(const std::string& str, unsigned long& duration);
}

namespace Time
{
	/** A short time format picked for being readable (e.g. "Sun 23 Mar 2025 10:20:30") */
	inline constexpr const char* DEFAULT_SHORT = "%a %d %b %Y %H:%M:%S";

	/** A long time format picked for being readable (e.g. "Sunday, 23 March 2025 @ 10:20:30 GMT"). */
	inline constexpr const char* DEFAULT_LONG = "%A, %d %B %Y @ %H:%M:%S %Z";

	/** The time format specified in ISO 8601 (e.g. "2025-03-23T10:20:30+0000") */
	inline constexpr const char* ISO_8601 = "%Y-%m-%dT%H:%M:%S%z";

	/** The time format specified in RFC 1123 (e.g. "Sun, 23 Mar 2025 10:20:30 GMT") */
	inline constexpr const char* RFC_1123 = "%a, %d %b %Y %H:%M:%S %Z";

	/** Converts a UNIX timestamp to a time string.
	 *
	 * @param ts The timestamp to convert to a string.
	 * @param format A snprintf format string to output the timestamp in.
	 * @param utc If the timestamp is a UTC timestamp then true or false if the
	 *            timestamp is a local timestamp.
	 */
	CoreExport std::string ToString(time_t ts, const char* format = nullptr, bool utc = false);

	/** Converts a duration from now to a time string.
	 *
	 * @param duration The duration from now to convert to a string.
	 * @param format A snprintf format string to output the timestamp in.
	 * @param utc If the timestamp is a UTC timestamp then true or false if the
	 *            timestamp is a local timestamp.
	 */
	inline std::string FromNow(unsigned long duration, const char* format = nullptr, bool utc = false)
	{
		return ToString(ServerInstance->Time() + duration, format, utc);
	}
}
