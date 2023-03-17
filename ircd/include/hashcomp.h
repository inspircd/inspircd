/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2009 Craig Edwards <brain@inspircd.org>
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

/*******************************************************
 * This file contains classes and templates that deal
 * with the comparison and hashing of 'irc strings'.
 * An 'irc string' is a string which compares in a
 * case insensitive manner, and as per RFC 1459 will
 * treat [ identical to {, ] identical to }, and \
 * as identical to |.
 *
 * There are functors that accept std::string and
 * compare/hash them as type irc::string by using
 * mapping arrays internally.
 *******************************************************/

/** Separate from the other casemap tables so that code *can* still exclusively rely on RFC casemapping
 * if it must.
 *
 * This is provided as a pointer so that modules can change it to their custom mapping tables,
 * e.g. for national character support.
 */
CoreExport extern unsigned const char *national_case_insensitive_map;

/** A mapping of uppercase to lowercase, including scandinavian
 * 'oddities' as specified by RFC1459, e.g. { -> [, and | -> \
 */
CoreExport extern unsigned const char rfc_case_insensitive_map[256];

/** Case insensitive map, ASCII rules.
 * That is;
 * [ != {, but A == a.
 */
CoreExport extern unsigned const char ascii_case_insensitive_map[256];

/** The irc namespace contains a number of helper classes.
 */
namespace irc {
/** Check if two IRC object (e.g. nick or channel) names are equal.
 * This function uses national_case_insensitive_map to determine equality, which, by default does comparison
 * according to RFC 1459, treating certain otherwise non-identical characters as identical.
 * @param s1 First string to compare
 * @param s2 Second string to compare
 * @return True if the two names are equal, false otherwise
 */
CoreExport bool equals(const std::string& s1, const std::string& s2);

/** Check whether \p needle exists within \p haystack.
 * @param haystack The string to search within.
 * @param needle The string to search for.
 * @return Either the index at which \p needle was found or std::string::npos.
 */
CoreExport size_t find(const std::string& haystack, const std::string& needle);

/** This class returns true if two strings match.
 * Case sensitivity is ignored, and the RFC 'character set'
 * is adhered to
 */
struct StrHashComp {
    /** The operator () does the actual comparison in hash_map
     */
    bool operator()(const std::string& s1, const std::string& s2) const {
        return equals(s1, s2);
    }
};

struct insensitive {
    size_t CoreExport operator()(const std::string &s) const;
};

struct insensitive_swo {
    bool CoreExport operator()(const std::string& a, const std::string& b) const;
};

/** irc::sepstream allows for splitting token separated lists.
 * Each successive call to sepstream::GetToken() returns
 * the next token, until none remain, at which point the method returns
 * false.
 */
class CoreExport sepstream {
  protected:
    /** Original string.
     */
    std::string tokens;
    /** Separator value
     */
    char sep;
    /** Current string position
     */
    size_t pos;
    /** If set then GetToken() can return an empty string
     */
    bool allow_empty;
  public:
    /** Create a sepstream and fill it with the provided data
     */
    sepstream(const std::string &source, char separator, bool allowempty = false);

    /** Fetch the next token from the stream
     * @param token The next token from the stream is placed here
     * @return True if tokens still remain, false if there are none left
     */
    bool GetToken(std::string& token);

    /** Fetch the next numeric token from the stream
     * @param token The next token from the stream is placed here
     * @return True if tokens still remain, false if there are none left
     */
    template<typename Numeric>
    bool GetNumericToken(Numeric& token) {
        std::string str;
        if (!GetToken(str)) {
            return false;
        }

        token = ConvToNum<Numeric>(str);
        return true;
    }

    /** Fetch the entire remaining stream, without tokenizing
     * @return The remaining part of the stream
     */
    const std::string GetRemaining();

    /** Returns true if the end of the stream has been reached
     * @return True if the end of the stream has been reached, otherwise false
     */
    bool StreamEnd();

    /** Returns true if the specified value exists in the stream
     * @param value The value to search for
     * @return True if the value was found, False otherwise
     */
    bool Contains(const std::string& value);
};

/** A derived form of sepstream, which separates on commas
 */
class CoreExport commasepstream : public sepstream {
  public:
    /** Initialize with comma separator
     */
    commasepstream(const std::string &source,
                   bool allowempty = false) : sepstream(source, ',', allowempty) {
    }
};

/** A derived form of sepstream, which separates on spaces
 */
class CoreExport spacesepstream : public sepstream {
  public:
    /** Initialize with space separator
     */
    spacesepstream(const std::string &source,
                   bool allowempty = false) : sepstream(source, ' ', allowempty) {
    }
};

/** irc::tokenstream reads a string formatted as per RFC1459 and RFC2812.
 * It will split the string into 'tokens' each containing one parameter
 * from the string.
 * For instance, if it is instantiated with the string:
 * "PRIVMSG #test :foo bar baz qux"
 * then each successive call to tokenstream::GetToken() will return
 * "PRIVMSG", "#test", "foo bar baz qux", "".
 * Note that if the whole string starts with a colon this is not taken
 * to mean the string is all one parameter, and the first item in the
 * list will be ":item". This is to allow for parsing 'source' fields
 * from data.
 */
class CoreExport tokenstream {
  private:
    /** The message we are parsing tokens from. */
    std::string message;

    /** The current position within the message. */
    size_t position;

  public:
    /** Create a tokenstream and fill it with the provided data. */
    tokenstream(const std::string& msg, size_t start = 0,
                size_t end = std::string::npos);

    /** Retrieves the underlying message. */
    std::string& GetMessage() {
        return message;
    }

    /** Retrieve the next \<middle> token in the token stream.
     * @param token The next token available, or an empty string if none remain.
     * @return True if tokens are left to be read, false if the last token was just retrieved.
     */
    bool GetMiddle(std::string& token);

    /** Retrieve the next \<trailing> token in the token stream.
     * @param token The next token available, or an empty string if none remain.
     * @return True if tokens are left to be read, false if the last token was just retrieved.
     */
    bool GetTrailing(std::string& token);
};

/** The portparser class separates out a port range into integers.
 * A port range may be specified in the input string in the form
 * "6660,6661,6662-6669,7020". The end of the stream is indicated by
 * a return value of 0 from portparser::GetToken(). If you attempt
 * to specify an illegal range (e.g. one where start >= end, or
 * start or end < 0) then GetToken() will return the first element
 * of the pair of numbers.
 */
class CoreExport portparser {
  private:

    /** Used to split on commas
     */
    commasepstream sep;

    /** Current position in a range of ports
     */
    long in_range;

    /** Starting port in a range of ports
     */
    long range_begin;

    /** Ending port in a range of ports
     */
    long range_end;

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
    portparser(const std::string &source, bool allow_overlapped = true);

    /** Fetch the next token from the stream
     * @return The next port number is returned, or 0 if none remain
     */
    long GetToken();
};
}
