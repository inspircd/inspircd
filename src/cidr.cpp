/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"

/* Used when comparing CIDR masks for the modulus bits left over.
 * A lot of ircd's seem to do this:
 * ((-1) << (8 - (mask % 8)))
 * But imho, it sucks in comparison to a nice neat lookup table.
 */
const unsigned char inverted_bits[8] = {	0x00, /* 00000000 - 0 bits - never actually used */
				0x80, /* 10000000 - 1 bits */
				0xC0, /* 11000000 - 2 bits */
				0xE0, /* 11100000 - 3 bits */
				0xF0, /* 11110000 - 4 bits */
				0xF8, /* 11111000 - 5 bits */
				0xFC, /* 11111100 - 6 bits */
				0xFE  /* 11111110 - 7 bits */
};


/* Match raw bytes using CIDR bit matching, used by higher level MatchCIDR() */
bool irc::sockets::MatchCIDRBits(const unsigned char* address, const unsigned char* mask, unsigned int mask_bits)
{
	unsigned int divisor = mask_bits / 8; /* Number of whole bytes in the mask */
	unsigned int modulus = mask_bits % 8; /* Remaining bits in the mask after whole bytes are dealt with */

	/* First (this is faster) compare the odd bits with logic ops */
	if (modulus)
		if ((address[divisor] & inverted_bits[modulus]) != (mask[divisor] & inverted_bits[modulus]))
			/* If they dont match, return false */
			return false;

	/* Secondly (this is slower) compare the whole bytes */
	if (memcmp(address, mask, divisor))
		return false;

	/* The address matches the mask, to mask_bits bits of mask */
	return true;
}

/* Match CIDR, but dont attempt to match() against leading *!*@ sections */
bool irc::sockets::MatchCIDR(const std::string &address, const std::string &cidr_mask)
{
	return MatchCIDR(address, cidr_mask, false);
}

/* Match CIDR strings, e.g. 127.0.0.1 to 127.0.0.0/8 or 3ffe:1:5:6::8 to 3ffe:1::0/32
 * If you have a lot of hosts to match, youre probably better off building your mask once
 * and then using the lower level MatchCIDRBits directly.
 *
 * This will also attempt to match any leading usernames or nicknames on the mask, using
 * match(), when match_with_username is true.
 */
bool irc::sockets::MatchCIDR(const std::string &address, const std::string &cidr_mask, bool match_with_username)
{
	unsigned char addr_raw[16];
	unsigned char mask_raw[16];
	unsigned int bits = 0;

	std::string address_copy;
	std::string cidr_copy;

	/* The caller is trying to match ident@<mask>/bits.
	 * Chop off the ident@ portion, use match() on it
	 * seperately.
	 */
	if (match_with_username)
	{
		/* Use strchr not strrchr, because its going to be nearer to the left */
		std::string::size_type username_mask_pos = cidr_mask.rfind('@');
		std::string::size_type username_addr_pos = address.rfind('@');

		/* Both strings have an @ symbol in them */
		if (username_mask_pos != std::string::npos && username_addr_pos != std::string::npos)
		{
			/* Try and match() the strings before the @
			 * symbols, and recursively call MatchCIDR without
			 * username matching enabled to match the host part.
			 */
			return (InspIRCd::Match(address.substr(0, username_addr_pos), cidr_mask.substr(0, username_mask_pos), ascii_case_insensitive_map) &&
					MatchCIDR(address.substr(username_addr_pos + 1), cidr_mask.substr(username_mask_pos + 1), false));
		}
		else
		{
			address_copy = address.substr(username_addr_pos + 1);
			cidr_copy = cidr_mask.substr(username_mask_pos + 1);
		}
	}
	else
	{
		address_copy.assign(address);
		cidr_copy.assign(cidr_mask);
	}

	in_addr  address_in4;
	in_addr  mask_in4;

	std::string::size_type bits_chars = cidr_copy.rfind('/');

	if (bits_chars != std::string::npos)
	{
		bits = atoi(cidr_copy.substr(bits_chars + 1).c_str());
		cidr_copy.erase(bits_chars, cidr_copy.length() - bits_chars);
	}
	else
	{
		/* No 'number of bits' field! */
		return false;
	}

#ifdef SUPPORT_IP6LINKS
	in6_addr address_in6;
	in6_addr mask_in6;

	if (inet_pton(AF_INET6, address_copy.c_str(), &address_in6) > 0)
	{
		if (inet_pton(AF_INET6, cidr_copy.c_str(), &mask_in6) > 0)
		{
			memcpy(&addr_raw, &address_in6.s6_addr, 16);
			memcpy(&mask_raw, &mask_in6.s6_addr, 16);

			if (bits > 128)
				bits = 128;
		}
		else
		{
			/* The address was valid ipv6, but the mask
			 * that goes with it wasnt.
			 */
			return false;
		}
	}
	else
#endif
	if (inet_pton(AF_INET, address_copy.c_str(), &address_in4) > 0)
	{
		if (inet_pton(AF_INET, cidr_copy.c_str(), &mask_in4) > 0)
		{
			memcpy(&addr_raw, &address_in4.s_addr, 4);
			memcpy(&mask_raw, &mask_in4.s_addr, 4);

			if (bits > 32)
				bits = 32;
		}
		else
		{
			/* The address was valid ipv4,
			 * but the mask that went with it wasnt.
			 */
			return false;
		}
	}
	else
	{
		/* The address was neither ipv4 or ipv6 */
		return false;
	}

	/* Low-level-match the bits in the raw data */
	return MatchCIDRBits(addr_raw, mask_raw, bits);
}


