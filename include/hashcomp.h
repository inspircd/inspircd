/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef HASHCOMP_H
#define HASHCOMP_H

#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <set>

/*******************************************************
 * This file contains classes and templates that deal
 * with the comparison and hashing of 'irc strings'.
 * An 'irc string' is a string which compares in a
 * case insensitive manner, and as per RFC 1459 will
 * treat [ identical to {, ] identical to }, and \
 * as identical to |.
 *
 * Our hashing functions are designed  to accept
 * std::string and compare/hash them as type irc::string
 * by converting them internally. This makes them
 * backwards compatible with other code which is not
 * aware of irc::string.
 *******************************************************/

/** Seperate from the other casemap tables so that code *can* still exclusively rely on RFC casemapping
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

/** Case sensitive (identity) map.
 */
CoreExport extern unsigned const char rfc_case_sensitive_map[256];

template<typename T> const T& SearchAndReplace(T& text, const T& pattern, const T& replace)
{
        T replacement;
        if ((!pattern.empty()) && (!text.empty()))
        {
                for (std::string::size_type n = 0; n != text.length(); ++n)
                {
                        if (text.length() >= pattern.length() && text.substr(n, pattern.length()) == pattern)
                        {
                                /* Found the pattern in the text, replace it, and advance */
                                replacement.append(replace);
                                n = n + pattern.length() - 1;
                        }
                        else
                        {
                                replacement += text[n];
                        }
                }
        }
        text = replacement;
        return text;
}

/** The irc namespace contains a number of helper classes.
 */
namespace irc
{

	/** This class returns true if two strings match.
	 * Case sensitivity is ignored, and the RFC 'character set'
	 * is adhered to
	 */
	struct StrHashComp
	{
		/** The operator () does the actual comparison in hash_map
		 */
		bool operator()(const std::string& s1, const std::string& s2) const;
	};

	/** The irc_char_traits class is used for RFC-style comparison of strings.
	 * This class is used to implement irc::string, a case-insensitive, RFC-
	 * comparing string class.
	 */
	struct irc_char_traits : std::char_traits<char> {

		/** Check if two chars match.
		 * @param c1st First character
		 * @param c2nd Second character
		 * @return true if the characters are equal
		 */
		static bool eq(char c1st, char c2nd);

		/** Check if two chars do NOT match.
		 * @param c1st First character
		 * @param c2nd Second character
		 * @return true if the characters are unequal
		 */
		static bool ne(char c1st, char c2nd);

		/** Check if one char is less than another.
		 * @param c1st First character
		 * @param c2nd Second character
		 * @return true if c1st is less than c2nd
		 */
		static bool lt(char c1st, char c2nd);

		/** Compare two strings of size n.
		 * @param str1 First string
		 * @param str2 Second string
		 * @param n Length to compare to
		 * @return similar to strcmp, zero for equal, less than zero for str1
		 * being less and greater than zero for str1 being greater than str2.
		 */
		static CoreExport int compare(const char* str1, const char* str2, size_t n);

		/** Find a char within a string up to position n.
		 * @param s1 String to find in
		 * @param n Position to search up to
		 * @param c Character to search for
		 * @return Pointer to the first occurance of c in s1
		 */
		static CoreExport const char* find(const char* s1, int  n, char c);

		/** Remap a string so that comparisons for std::string match those
		 * of irc::string; will be faster if doing many comparisons
		 */
		static std::string remap(const std::string& source);
	};

	/** Compose a hex string from raw data.
	 * @param raw The raw data to compose hex from
	 * @pram rawsz The size of the raw data buffer
	 * @return The hex string.
	 */
	CoreExport std::string hex(const unsigned char *raw, size_t rawsz);

	/** This typedef declares irc::string based upon irc_char_traits.
	 */
	class string
	{
	 public:
		std::string value;
		string() {}
		string(const std::string& v) : value(v) {}
		string(const char* v) : value(v) {}
		inline operator std::string&() { return value; }
		inline operator const std::string&() const { return value; }
		inline const char* c_str() const { return value.c_str(); }
		inline bool empty() const { return value.empty(); }
		inline bool operator<(const string& o) const
		{
			const std::string::size_type __mlen = value.length();
			const std::string::size_type __olen = o.value.length();
			const std::string::size_type __len = std::min(__mlen, __olen);
			const int __cmp = irc_char_traits::compare(value.c_str(), o.value.c_str(), __len);
			if (__cmp == 0)
				return __mlen < __olen;
			return __cmp < 0;
		}
		inline bool operator==(const string& o) const
		{
			if (value.length() != o.value.length())
				return false;
			return irc_char_traits::compare(value.c_str(), o.value.c_str(), value.length()) == 0;
		}
		inline bool operator!=(const string& o) const
		{
			if (value.length() != o.value.length())
				return true;
			return irc_char_traits::compare(value.c_str(), o.value.c_str(), value.length()) != 0;
		}
	};

	/** irc::stringjoiner joins string lists into a string, using
	 * the given seperator string.
	 * This class can join a vector of std::string, a deque of
	 * std::string, or a const char* const* array, using overloaded
	 * constructors.
	 */
	class CoreExport stringjoiner
	{
	 private:

		/** Output string
		 */
		std::string joined;

	 public:

		/** Join elements of a vector, between (and including) begin and end
		 * @param seperator The string to seperate values with
		 * @param sequence One or more items to seperate
		 * @param begin The starting element in the sequence to be joined
		 * @param end The ending element in the sequence to be joined
		 */
		stringjoiner(const std::string &seperator, const std::vector<std::string> &sequence, int begin, int end);

		/** Join elements of a deque, between (and including) begin and end
		 * @param seperator The string to seperate values with
		 * @param sequence One or more items to seperate
		 * @param begin The starting element in the sequence to be joined
		 * @param end The ending element in the sequence to be joined
		 */
		stringjoiner(const std::string &seperator, const std::deque<std::string> &sequence, int begin, int end);

		/** Join elements of an array of char arrays, between (and including) begin and end
		 * @param seperator The string to seperate values with
		 * @param sequence One or more items to seperate
		 * @param begin The starting element in the sequence to be joined
		 * @param end The ending element in the sequence to be joined
		 */
		stringjoiner(const std::string &seperator, const char* const* sequence, int begin, int end);

		/** Get the joined sequence
		 * @return A reference to the joined string
		 */
		std::string& GetJoined();
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
	class CoreExport tokenstream
	{
	 private:

		/** Original string
		 */
		std::string tokens;

		/** Last position of a seperator token
		 */
		std::string::iterator last_starting_position;

		/** Current string position
		 */
		std::string::iterator n;

		/** True if the last value was an ending value
		 */
		bool last_pushed;
	 public:

		/** Create a tokenstream and fill it with the provided data
		 */
		tokenstream(const std::string &source);

		/** Destructor
		 */
		~tokenstream();

		/** Fetch the next token from the stream as a std::string
		 * @param token The next token available, or an empty string if none remain
		 * @return True if tokens are left to be read, false if the last token was just retrieved.
		 */
		bool GetToken(std::string &token);

		/** Fetch the next token from the stream as an irc::string
		 * @param token The next token available, or an empty string if none remain
		 * @return True if tokens are left to be read, false if the last token was just retrieved.
		 */
		bool GetToken(irc::string &token);

		/** Fetch the next token from the stream as an integer
		 * @param token The next token available, or undefined if none remain
		 * @return True if tokens are left to be read, false if the last token was just retrieved.
		 */
		bool GetToken(int &token);

		/** Fetch the next token from the stream as a long integer
		 * @param token The next token available, or undefined if none remain
		 * @return True if tokens are left to be read, false if the last token was just retrieved.
		 */
		bool GetToken(long &token);
	};

	/** irc::sepstream allows for splitting token seperated lists.
	 * Each successive call to sepstream::GetToken() returns
	 * the next token, until none remain, at which point the method returns
	 * an empty string.
	 */
	class CoreExport sepstream
	{
	 private:
		/** Original string.
		 */
		const std::string tokens;
		/** Whether to suppress empty items
		 */
		const bool suppress_empty;
		/** Current string position
		 */
		std::string::const_iterator n;
		/** Seperator value
		 */
		const char sep;
		/** Whether the end has been reached
		 */
		bool endreached;
	 public:
		/** Create a sepstream and fill it with the provided data
		 */
		sepstream(const std::string &source, char seperator, bool suppress_empty_items = true);

		/** Destructor
		 */
		virtual ~sepstream();

		/** Fetch the next token from the stream
		 * @param token The next token from the stream is placed here
		 * @return True if tokens still remain, false if there are none left
		 */
		virtual bool GetToken(std::string &token);

		/** Fetch the entire remaining stream, without tokenizing
		 * @return The remaining part of the stream
		 */
		virtual std::string GetRemaining() const;

		/** Returns true if the end of the stream has been reached
		 * @return True if the end of the stream has been reached, otherwise false
		 */
		virtual bool StreamEnd() const;
	};

	/** A derived form of sepstream, which seperates on commas
	 */
	class CoreExport commasepstream : public sepstream
	{
	 public:
		/** Initialize with comma seperator
		 */
		commasepstream(const std::string &source, bool suppress_empty_items = true) : sepstream(source, ',', suppress_empty_items)
		{
		}
	};

	/** A derived form of sepstream, which seperates on spaces
	 */
	class CoreExport spacesepstream : public sepstream
	{
	 public:
		/** Initialize with space seperator
		 */
		spacesepstream(const std::string &source, bool suppress_empty_items = true) : sepstream(source, ' ', suppress_empty_items)
		{
		}
	};

	/** The portparser class seperates out a port range into integers.
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
		commasepstream* sep;

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
		std::map<long, bool> overlap_set;

		/** Returns true if val overlaps an existing range
		 */
		bool Overlaps(long val);
	 public:

		/** Create a portparser and fill it with the provided data
		 * @param source The source text to parse from
		 * @param allow_overlapped Allow overlapped ranges
		 */
		portparser(const std::string &source, bool allow_overlapped = true);

		/** Frees the internal commasepstream object
		 */
		~portparser();

		/** Fetch the next token from the stream
		 * @return The next port number is returned, or 0 if none remain
		 */
		long GetToken();
	};

	/** Turn _ characters in a string into spaces
	 * @param n String to translate
	 * @return The new value with _ translated to space.
	 */
	CoreExport const char* Spacify(const char* n);
}

/* Define operators for using >> and << with irc::string to an ostream on an istream. */
/* This was endless fun. No. Really. */
/* It was also the first core change Ommeh made, if anyone cares */

/** Operator << for irc::string
 */
inline std::ostream& operator<<(std::ostream &os, const irc::string &str) { return os << str.value; }

/** Operator >> for irc::string
 */
inline std::istream& operator>>(std::istream &is, irc::string &str)
{
	std::string tmp;
	is >> tmp;
	str = tmp;
	return is;
}

/* Define operators for + and == with irc::string to std::string for easy assignment
 * and comparison
 *
 * Operator +
 */
inline std::string operator+ (std::string& leftval, irc::string& rightval)
{
	return leftval + rightval.value;
}

/** Trim the leading and trailing spaces from a std::string.
 */
inline std::string& trim(std::string &str)
{
	std::string::size_type start = str.find_first_not_of(" ");
	std::string::size_type end = str.find_last_not_of(" ");
	if (start == std::string::npos || end == std::string::npos)
		str = "";
	else
		str = str.substr(start, end-start+1);

	return str;
}

/** Hashing stuff is totally different on vc++'s hash_map implementation, so to save a buttload of
 * #ifdefs we'll just do it all at once. Except, of course, with TR1, when it's the same as GCC.
 */
BEGIN_HASHMAP_NAMESPACE

	/** Hashing function to hash irc::string
	 */
#if defined(WINDOWS) && !defined(HAS_TR1_UNORDERED)
	template<> class CoreExport hash_compare<irc::string, std::less<irc::string> >
	{
	public:
		enum { bucket_size = 4, min_buckets = 8 }; /* Got these numbers from the CRT source, if anyone wants to change them feel free. */

		/** Compare two irc::string values for hashing in hash_map
		 */
		bool operator()(const irc::string & s1, const irc::string & s2) const
		{
			if(s1.length() != s2.length()) return true;
			return (irc::irc_char_traits::compare(s1.c_str(), s2.c_str(), (size_t)s1.length()) < 0);
		}

		/** Hash an irc::string value for hash_map
		 */
		size_t operator()(const irc::string & s) const;
	};

	template<> class CoreExport hash_compare<std::string, std::less<std::string> >
	{
	public:
		enum { bucket_size = 4, min_buckets = 8 }; /* Again, from the CRT source */

		/** Compare two std::string values for hashing in hash_map
		 */
		bool operator()(const std::string & s1, const std::string & s2) const
		{
			if(s1.length() != s2.length()) return true;
			return (irc::irc_char_traits::compare(s1.c_str(), s2.c_str(), (size_t)s1.length()) < 0);
		}

		/** Hash a std::string using RFC1459 case sensitivity rules
		* @param s A string to hash
		* @return The hash value
		*/
		size_t operator()(const std::string & s) const;
	};
#else

	template<> struct hash<irc::string>
	{
		/** Hash an irc::string using RFC1459 case sensitivity rules
		 * @param s A string to hash
		 * @return The hash value
		 */
		size_t CoreExport operator()(const irc::string &s) const;
	};

	/* XXX FIXME: Implement a hash function overriding std::string's that works with TR1! */

#ifdef HASHMAP_DEPRECATED
	struct insensitive
#else
	CoreExport template<> struct hash<std::string>
#endif
	{
		size_t CoreExport operator()(const std::string &s) const;
	};

#endif

	/** Convert a string to lower case respecting RFC1459
	* @param n A string to lowercase
	*/
	void strlower(char *n);

END_HASHMAP_NAMESPACE

#endif
