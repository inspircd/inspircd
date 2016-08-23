/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2005-2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


#include "inspircd.h"

/******************************************************
 *
 * The hash functions of InspIRCd are the centrepoint
 * of the entire system. If these functions are
 * inefficient or wasteful, the whole program suffers
 * as a result. A lot of C programmers in the ircd
 * scene spend a lot of time debating (arguing) about
 * the best way to write hash functions to hash irc
 * nicknames, channels etc.
 * We are lucky as C++ developers as unordered_map does
 * a lot of this for us. It does intellegent memory
 * requests, bucketing, search functions, insertion
 * and deletion etc. All we have to do is write some
 * overloaded comparison and hash value operators which
 * cause it to act in an irc-like way. The features we
 * add to the standard hash_map are:
 *
 * Case insensitivity: The hash_map will be case
 * insensitive.
 *
 * Scandanavian Comparisons: The characters [, ], \ will
 * be considered the lowercase of {, } and |.
 *
 ******************************************************/


/**
 * A case insensitive mapping of characters from upper case to lower case for
 * the ASCII character set.
 */
unsigned const char ascii_case_insensitive_map[256] = {
	0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   // 0-9
	10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  // 10-19
	20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  // 20-29
	30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  // 30-39
	40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  // 40-49
	50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  // 50-59
	60,  61,  62,  63,  64,  97,  98,  99,  100, 101, // 60-69
	102, 103, 104, 105, 106, 107, 108, 109, 110, 111, // 70-79
	112, 113, 114, 115, 116, 117, 118, 119, 120, 121, // 80-89
	122, 91,  92,  93,  94,  95,  96,  97,  98,  99,  // 90-99
	100, 101, 102, 103, 104, 105, 106, 107, 108, 109, // 100-109
	110, 111, 112, 113, 114, 115, 116, 117, 118, 119, // 110-119
	120, 121, 122, 123, 124, 125, 126, 127, 128, 129, // 120-129
	130, 131, 132, 133, 134, 135, 136, 137, 138, 139, // 130-139
	140, 141, 142, 143, 144, 145, 146, 147, 148, 149, // 140-149
	150, 151, 152, 153, 154, 155, 156, 157, 158, 159, // 150-159
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169, // 160-169
	170, 171, 172, 173, 174, 175, 176, 177, 178, 179, // 170-179
	180, 181, 182, 183, 184, 185, 186, 187, 188, 189, // 180-189
	190, 191, 192, 193, 194, 195, 196, 197, 198, 199, // 190-199
	200, 201, 202, 203, 204, 205, 206, 207, 208, 209, // 200-209
	210, 211, 212, 213, 214, 215, 216, 217, 218, 219, // 210-219
	220, 221, 222, 223, 224, 225, 226, 227, 228, 229, // 220-229
	230, 231, 232, 233, 234, 235, 236, 237, 238, 239, // 230-249
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249, // 240-249
	250, 251, 252, 253, 254, 255,                     // 250-255
};



/**
 * A case insensitive mapping of characters from upper case to lower case for
 * the character set of RFC 1459. This is identical to ASCII with the small
 * exception of {}| being considered to be the lower case equivalents of the
 * characters []\ respectively.
 */
unsigned const char rfc_case_insensitive_map[256] = {
	0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   // 0-9
	10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  // 10-19
	20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  // 20-29
	30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  // 30-39
	40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  // 40-49
	50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  // 50-59
	60,  61,  62,  63,  64,  97,  98,  99,  100, 101, // 60-69
	102, 103, 104, 105, 106, 107, 108, 109, 110, 111, // 70-79
	112, 113, 114, 115, 116, 117, 118, 119, 120, 121, // 80-89
	122, 123, 124, 125, 94,  95,  96,  97,  98,  99,  // 90-99
	100, 101, 102, 103, 104, 105, 106, 107, 108, 109, // 100-109
	110, 111, 112, 113, 114, 115, 116, 117, 118, 119, // 110-119
	120, 121, 122, 123, 124, 125, 126, 127, 128, 129, // 120-129
	130, 131, 132, 133, 134, 135, 136, 137, 138, 139, // 130-139
	140, 141, 142, 143, 144, 145, 146, 147, 148, 149, // 140-149
	150, 151, 152, 153, 154, 155, 156, 157, 158, 159, // 150-159
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169, // 160-169
	170, 171, 172, 173, 174, 175, 176, 177, 178, 179, // 170-179
	180, 181, 182, 183, 184, 185, 186, 187, 188, 189, // 180-189
	190, 191, 192, 193, 194, 195, 196, 197, 198, 199, // 190-199
	200, 201, 202, 203, 204, 205, 206, 207, 208, 209, // 200-209
	210, 211, 212, 213, 214, 215, 216, 217, 218, 219, // 210-219
	220, 221, 222, 223, 224, 225, 226, 227, 228, 229, // 220-229
	230, 231, 232, 233, 234, 235, 236, 237, 238, 239, // 230-239
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249, // 240-249
	250, 251, 252, 253, 254, 255,                     // 250-255
};

/**
 * A case sensitive mapping of characters from upper case to lower case for the
 * character set of RFC 1459. This is identical to ASCII.
 */
unsigned const char rfc_case_sensitive_map[256] = {
	0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   // 0-9
	10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  // 10-19
	20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  // 20-29
	30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  // 30-39
	40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  // 40-49
	50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  // 50-59
	60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  // 60-69
	70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  // 70-79
	80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  // 80-89
	90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  // 90-99
	100, 101, 102, 103, 104, 105, 106, 107, 108, 109, // 100-109
	110, 111, 112, 113, 114, 115, 116, 117, 118, 119, // 110-119
	120, 121, 122, 123, 124, 125, 126, 127, 128, 129, // 120-129
	130, 131, 132, 133, 134, 135, 136, 137, 138, 139, // 130-139
	140, 141, 142, 143, 144, 145, 146, 147, 148, 149, // 140-149
	150, 151, 152, 153, 154, 155, 156, 157, 158, 159, // 150-159
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169, // 160-169
	170, 171, 172, 173, 174, 175, 176, 177, 178, 179, // 170-179
	180, 181, 182, 183, 184, 185, 186, 187, 188, 189, // 180-189
	190, 191, 192, 193, 194, 195, 196, 197, 198, 199, // 190-199
	200, 201, 202, 203, 204, 205, 206, 207, 208, 209, // 200-209
	210, 211, 212, 213, 214, 215, 216, 217, 218, 219, // 210-219
	220, 221, 222, 223, 224, 225, 226, 227, 228, 229, // 220-229
	230, 231, 232, 233, 234, 235, 236, 237, 238, 239, // 230-239
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249, // 240-249
	250, 251, 252, 253, 254, 255,                     // 250-255
};

bool irc::equals(const std::string& s1, const std::string& s2)
{
	const unsigned char* n1 = (const unsigned char*)s1.c_str();
	const unsigned char* n2 = (const unsigned char*)s2.c_str();
	for (; *n1 && *n2; n1++, n2++)
		if (national_case_insensitive_map[*n1] != national_case_insensitive_map[*n2])
			return false;
	return (national_case_insensitive_map[*n1] == national_case_insensitive_map[*n2]);
}

bool irc::insensitive_swo::operator()(const std::string& a, const std::string& b) const
{
	const unsigned char* charmap = national_case_insensitive_map;
	std::string::size_type asize = a.size();
	std::string::size_type bsize = b.size();
	std::string::size_type maxsize = std::min(asize, bsize);

	for (std::string::size_type i = 0; i < maxsize; i++)
	{
		unsigned char A = charmap[(unsigned char)a[i]];
		unsigned char B = charmap[(unsigned char)b[i]];
		if (A > B)
			return false;
		else if (A < B)
			return true;
	}
	return (asize < bsize);
}

size_t irc::insensitive::operator()(const std::string &s) const
{
	/* XXX: NO DATA COPIES! :)
	 * The hash function here is practically
	 * a copy of the one in STL's hash_fun.h,
	 * only with *x replaced with national_case_insensitive_map[*x].
	 * This avoids a copy to use hash<const char*>
	 */
	size_t t = 0;
	for (std::string::const_iterator x = s.begin(); x != s.end(); ++x) /* ++x not x++, as its faster */
		t = 5 * t + national_case_insensitive_map[(unsigned char)*x];
	return t;
}

/******************************************************
 *
 * This is the implementation of our special irc::string
 * class which is a case-insensitive equivalent to
 * std::string which is not only case-insensitive but
 * can also do scandanavian comparisons, e.g. { = [, etc.
 *
 * This class depends on the const array 'national_case_insensitive_map'.
 *
 ******************************************************/

bool irc::irc_char_traits::eq(char c1st, char c2nd)
{
	return national_case_insensitive_map[(unsigned char)c1st] == national_case_insensitive_map[(unsigned char)c2nd];
}

bool irc::irc_char_traits::ne(char c1st, char c2nd)
{
	return national_case_insensitive_map[(unsigned char)c1st] != national_case_insensitive_map[(unsigned char)c2nd];
}

bool irc::irc_char_traits::lt(char c1st, char c2nd)
{
	return national_case_insensitive_map[(unsigned char)c1st] < national_case_insensitive_map[(unsigned char)c2nd];
}

int irc::irc_char_traits::compare(const char* str1, const char* str2, size_t n)
{
	for(unsigned int i = 0; i < n; i++)
	{
		if(national_case_insensitive_map[(unsigned char)*str1] > national_case_insensitive_map[(unsigned char)*str2])
			return 1;

		if(national_case_insensitive_map[(unsigned char)*str1] < national_case_insensitive_map[(unsigned char)*str2])
			return -1;

		if(*str1 == 0 || *str2 == 0)
		   	return 0;

		str1++;
		str2++;
	}
	return 0;
}

const char* irc::irc_char_traits::find(const char* s1, int  n, char c)
{
	while(n-- > 0 && national_case_insensitive_map[(unsigned char)*s1] != national_case_insensitive_map[(unsigned char)c])
		s1++;
	return (n >= 0) ? s1 : NULL;
}

irc::tokenstream::tokenstream(const std::string &source) : spacesepstream(source)
{
}

bool irc::tokenstream::GetToken(std::string &token)
{
	bool first = !pos;

	if (!spacesepstream::GetToken(token))
		return false;

	/* This is the last parameter */
	if (token[0] == ':' && !first)
	{
		token.erase(token.begin());
		if (!StreamEnd())
		{
			token += ' ';
			token += GetRemaining();
		}
		pos = tokens.length() + 1;
	}

	return true;
}

bool irc::tokenstream::GetToken(int &token)
{
	std::string tok;
	bool returnval = GetToken(tok);
	token = ConvToInt(tok);
	return returnval;
}

bool irc::tokenstream::GetToken(long &token)
{
	std::string tok;
	bool returnval = GetToken(tok);
	token = ConvToInt(tok);
	return returnval;
}

irc::sepstream::sepstream(const std::string& source, char separator, bool allowempty)
	: tokens(source), sep(separator), pos(0), allow_empty(allowempty)
{
}

bool irc::sepstream::GetToken(std::string &token)
{
	if (this->StreamEnd())
	{
		token.clear();
		return false;
	}

	if (!this->allow_empty)
	{
		this->pos = this->tokens.find_first_not_of(this->sep, this->pos);
		if (this->pos == std::string::npos)
		{
			this->pos = this->tokens.length() + 1;
			token.clear();
			return false;
		}
	}

	size_t p = this->tokens.find(this->sep, this->pos);
	if (p == std::string::npos)
		p = this->tokens.length();

	token.assign(tokens, this->pos, p - this->pos);
	this->pos = p + 1;

	return true;
}

const std::string irc::sepstream::GetRemaining()
{
	return !this->StreamEnd() ? this->tokens.substr(this->pos) : "";
}

bool irc::sepstream::StreamEnd()
{
	return this->pos > this->tokens.length();
}

std::string irc::stringjoiner(const std::vector<std::string>& sequence, char separator)
{
	std::string joined;
	if (sequence.empty())
		return joined; // nothing to do here

	for (std::vector<std::string>::const_iterator i = sequence.begin(); i != sequence.end(); ++i)
		joined.append(*i).push_back(separator);
	joined.erase(joined.end()-1);
	return joined;
}

irc::portparser::portparser(const std::string &source, bool allow_overlapped)
	: sep(source), in_range(0), range_begin(0), range_end(0), overlapped(allow_overlapped)
{
}

bool irc::portparser::Overlaps(long val)
{
	if (overlapped)
		return false;

	return (!overlap_set.insert(val).second);
}

long irc::portparser::GetToken()
{
	if (in_range > 0)
	{
		in_range++;
		if (in_range <= range_end)
		{
			if (!Overlaps(in_range))
			{
				return in_range;
			}
			else
			{
				while (((Overlaps(in_range)) && (in_range <= range_end)))
					in_range++;

				if (in_range <= range_end)
					return in_range;
			}
		}
		else
			in_range = 0;
	}

	std::string x;
	sep.GetToken(x);

	if (x.empty())
		return 0;

	while (Overlaps(atoi(x.c_str())))
	{
		if (!sep.GetToken(x))
			return 0;
	}

	std::string::size_type dash = x.rfind('-');
	if (dash != std::string::npos)
	{
		std::string sbegin(x, 0, dash);
		range_begin = atoi(sbegin.c_str());
		range_end = atoi(x.c_str()+dash+1);

		if ((range_begin > 0) && (range_end > 0) && (range_begin < 65536) && (range_end < 65536) && (range_begin < range_end))
		{
			in_range = range_begin;
			return in_range;
		}
		else
		{
			/* Assume its just the one port */
			return atoi(sbegin.c_str());
		}
	}
	else
	{
		return atoi(x.c_str());
	}
}
