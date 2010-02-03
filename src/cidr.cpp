/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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


/* Match CIDR strings, e.g. 127.0.0.1 to 127.0.0.0/8 or 3ffe:1:5:6::8 to 3ffe:1::0/32
 * If you have a lot of hosts to match, youre probably better off building your mask once
 * and then using the lower level MatchCIDRBits directly.
 *
 * This will also attempt to match any leading usernames or nicknames on the mask, using
 * match(), when match_with_username is true.
 */
bool irc::sockets::MatchCIDR(const std::string &address, const std::string &cidr_mask, bool match_with_username)
{
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

	if (cidr_copy.find('/') == std::string::npos)
		return false;

	irc::sockets::sockaddrs addr;
	irc::sockets::aptosa(address_copy, 0, addr);

	irc::sockets::cidr_mask mask(cidr_copy);
	irc::sockets::cidr_mask mask2(addr, mask.length);

	return mask == mask2;
}


