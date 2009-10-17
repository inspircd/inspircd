/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef _HASHCOMP_H_
#define _HASHCOMP_H_

#include <cstring>
#include "hash_map.h"

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

#ifndef LOWERMAP
#define LOWERMAP

/** A mapping of uppercase to lowercase, including scandinavian
 * 'oddities' as specified by RFC1459, e.g. { -> [, and | -> \
 */
unsigned const char rfc_case_insensitive_map[256] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,					/* 0-19 */
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,				/* 20-39 */
	40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,				/* 40-59 */
	60, 61, 62, 63, 64, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,		/* 60-79 */
	112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 94, 95, 96, 97, 98, 99,		/* 80-99 */
	100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,	/* 100-119 */
	120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139,	/* 120-139 */
	140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,	/* 140-159 */
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,	/* 160-179 */
	180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199,	/* 180-199 */
	200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219,	/* 200-219 */
	220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,	/* 220-239 */
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255				/* 240-255 */
};

/** Seperate from the other casemap tables so that code *can* still exclusively rely on RFC casemapping
 * if it must.
 *
 * This is provided as a pointer so that modules can change it to their custom mapping tables,
 * e.g. for national character support.
 */
CoreExport extern unsigned const char *national_case_insensitive_map;

/** Case insensitive map, ASCII rules.
 * That is;
 * [ != {, but A == a.
 */
unsigned const char ascii_case_insensitive_map[256] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,                                   /* 0-19 */
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,                         /* 20-39 */
        40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,                         /* 40-59 */
        60, 61, 62, 63, 64, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,             /* 60-79 */
        112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 91, 92, 93, 94, 95, 96, 97, 98, 99,              /* 80-99 */
        100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,     /* 100-119 */
        120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139,     /* 120-139 */
        140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,     /* 140-159 */
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,     /* 160-179 */
        180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199,     /* 180-199 */
        200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219,     /* 200-219 */
        220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,     /* 220-239 */
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255                          /* 240-255 */
};

/** Case sensitive map.
 * Can technically also be used for ASCII case sensitive comparisons, as [ != {, etc.
 */
unsigned const char rfc_case_sensitive_map[256] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
        21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
        61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
        81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100,
        101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120,
        121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140,
        141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
        161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180,
        181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200,
        201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220,
        221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240,
        241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
};

#endif

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
	};

	/** Compose a hex string from raw data.
	 * @param raw The raw data to compose hex from
	 * @pram rawsz The size of the raw data buffer
	 * @return The hex string.
	 */
	CoreExport std::string hex(const unsigned char *raw, size_t rawsz);

	/** This typedef declares irc::string based upon irc_char_traits.
	 */
	typedef std::basic_string<char, irc_char_traits, std::allocator<char> > string;

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

	/** irc::modestacker stacks mode sequences into a list.
	 * It can then reproduce this list, clamped to a maximum of MAXMODES
	 * values per line.
	 */
	class CoreExport modestacker
	{
	 private:
		/** The mode sequence and its parameters
		 */
		std::deque<std::string> sequence;

		/** True if the mode sequence is initially adding
		 * characters, false if it is initially removing
		 * them
		 */
		bool adding;
	 public:

		/** Construct a new modestacker.
		 * @param add True if the stack is adding modes,
		 * false if it is removing them
		 */
		modestacker(bool add);

		/** Push a modeletter and its parameter onto the stack.
		 * No checking is performed as to if this mode actually
		 * requires a parameter. If you stack invalid mode
		 * sequences, they will be tidied if and when they are
		 * passed to a mode parser.
		 * @param modeletter The mode letter to insert
		 * @param parameter The parameter for the mode
		 */
		void Push(char modeletter, const std::string &parameter);

		/** Push a modeletter without parameter onto the stack.
		 * No checking is performed as to if this mode actually
		 * requires a parameter. If you stack invalid mode
		 * sequences, they will be tidied if and when they are
		 * passed to a mode parser.
		 * @param modeletter The mode letter to insert
		 */
		void Push(char modeletter);

		/** Push a '+' symbol onto the stack.
		 */
		void PushPlus();

		/** Push a '-' symbol onto the stack.
		 */
		void PushMinus();

		/** Return zero or more elements which form the
		 * mode line. This will be clamped to a max of
		 * MAXMODES items (MAXMODES-1 mode parameters and
		 * one mode sequence string), and max_line_size
		 * characters. As specified below, this function
		 * should be called in a loop until it returns zero,
		 * indicating there are no more modes to return.
		 * @param result The vector to populate. This will not
		 * be cleared before it is used.
		 * @param max_line_size The maximum size of the line
		 * to build, in characters, seperate to MAXMODES.
		 * @return The number of elements in the deque.
		 * The function should be called repeatedly until it
		 * returns 0, in case there are multiple lines of
		 * mode changes to be obtained.
		 */
		int GetStackedLine(std::vector<std::string> &result, int max_line_size = 360);

		/** deprecated compatability interface - TODO remove */
		int GetStackedLine(std::deque<std::string> &result, int max_line_size = 360) {
			std::vector<std::string> r;
			int n = GetStackedLine(r, max_line_size);
			result.clear();
			result.insert(result.end(), r.begin(), r.end());
			return n;
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
		std::string tokens;
		/** Last position of a seperator token
		 */
		std::string::iterator last_starting_position;
		/** Current string position
		 */
		std::string::iterator n;
		/** Seperator value
		 */
		char sep;
	 public:
		/** Create a sepstream and fill it with the provided data
		 */
		sepstream(const std::string &source, char seperator);

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
		virtual const std::string GetRemaining();

		/** Returns true if the end of the stream has been reached
		 * @return True if the end of the stream has been reached, otherwise false
		 */
		virtual bool StreamEnd();
	};

	/** A derived form of sepstream, which seperates on commas
	 */
	class CoreExport commasepstream : public sepstream
	{
	 public:
		/** Initialize with comma seperator
		 */
		commasepstream(const std::string &source) : sepstream(source, ',')
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
		spacesepstream(const std::string &source) : sepstream(source, ' ')
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
inline std::ostream& operator<<(std::ostream &os, const irc::string &str) { return os << str.c_str(); }

/** Operator >> for irc::string
 */
inline std::istream& operator>>(std::istream &is, irc::string &str)
{
	std::string tmp;
	is >> tmp;
	str = tmp.c_str();
	return is;
}

/* Define operators for + and == with irc::string to std::string for easy assignment
 * and comparison
 *
 * Operator +
 */
inline std::string operator+ (std::string& leftval, irc::string& rightval)
{
	return leftval + std::string(rightval.c_str());
}

/* Define operators for + and == with irc::string to std::string for easy assignment
 * and comparison
 *
 * Operator +
 */
inline irc::string operator+ (irc::string& leftval, std::string& rightval)
{
	return leftval + irc::string(rightval.c_str());
}

/* Define operators for + and == with irc::string to std::string for easy assignment
 * and comparison
 *
 * Operator ==
 */
inline bool operator== (const std::string& leftval, const irc::string& rightval)
{
	return (leftval.c_str() == rightval);
}

/* Define operators for + and == with irc::string to std::string for easy assignment
 * and comparison
 *
 * Operator ==
 */
inline bool operator== (const irc::string& leftval, const std::string& rightval)
{
	return (leftval == rightval.c_str());
}

/* Define operators != for irc::string to std::string for easy comparison
 */
inline bool operator!= (const irc::string& leftval, const std::string& rightval)
{
	return !(leftval == rightval.c_str());
}

/* Define operators != for std::string to irc::string for easy comparison
 */
inline bool operator!= (const std::string& leftval, const irc::string& rightval)
{
	return !(leftval.c_str() == rightval);
}

// FIXME MAXBUF messes up these
#if 0
template<std::size_t N>
static inline bool operator == (std::string const &lhs, char const (&rhs)[N])
{
	return lhs.length() == N - 1 && !std::memcmp(lhs.data(), rhs, N - 1);
}

template<std::size_t N>
static inline bool operator != (std::string const &lhs, char const (&rhs)[N])
{
	return !(lhs == rhs);
}
#endif

/** Assign an irc::string to a std::string.
 */
inline std::string assign(const irc::string &other) { return other.c_str(); }

/** Assign a std::string to an irc::string.
 */
inline irc::string assign(const std::string &other) { return other.c_str(); }

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
