/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDcidr */

#include "inspircd.h"
#include "wildcard.h"

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
bool irc::sockets::MatchCIDRBits(unsigned char* address, unsigned char* mask, unsigned int mask_bits)
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
bool irc::sockets::MatchCIDR(const char* address, const char* cidr_mask)
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
bool irc::sockets::MatchCIDR(const char* address, const char* cidr_mask, bool match_with_username)
{
	unsigned char addr_raw[16];
	unsigned char mask_raw[16];
	unsigned int bits = 0;
	char* mask = NULL;

	/* The caller is trying to match ident@<mask>/bits.
	 * Chop off the ident@ portion, use match() on it
	 * seperately.
	 */
	if (match_with_username)
	{
		/* Duplicate the strings, and try to find the position
		 * of the @ symbol in each
		 */
		char* address_dupe = strdup(address);
		char* cidr_dupe = strdup(cidr_mask);
	
		/* Use strchr not strrchr, because its going to be nearer to the left */
		char* username_mask_pos = strrchr(cidr_dupe, '@');
		char* username_addr_pos = strrchr(address_dupe, '@');

		/* Both strings have an @ symbol in them */
		if (username_mask_pos && username_addr_pos)
		{
			/* Zero out the location of the @ symbol */
			*username_mask_pos = *username_addr_pos = 0;

			/* Try and match() the strings before the @
			 * symbols, and recursively call MatchCIDR without
			 * username matching enabled to match the host part.
			 */
			bool result = (match(address_dupe, cidr_dupe) && MatchCIDR(username_addr_pos + 1, username_mask_pos + 1, false));

			/* Free the stuff we created */
			free(address_dupe);
			free(cidr_dupe);

			/* Return a result */
			return result;
		}
		else
		{
			/* One or both didnt have an @ in,
			 * just match as CIDR
			 */
			free(address_dupe);
			free(cidr_dupe);
			mask = strdup(cidr_mask);
		}
	}
	else
	{
		/* Make a copy of the cidr mask string,
		 * we're going to change it
		 */
		mask = strdup(cidr_mask);
	}

	in_addr  address_in4;
	in_addr  mask_in4;


	/* Use strrchr for this, its nearer to the right */
	char* bits_chars = strrchr(mask,'/');

	if (bits_chars)
	{
		bits = atoi(bits_chars + 1);
		*bits_chars = 0;
	}
	else
	{
		/* No 'number of bits' field! */
		free(mask);
		return false;
	}

#ifdef SUPPORT_IP6LINKS
	in6_addr address_in6;
	in6_addr mask_in6;

	if (inet_pton(AF_INET6, address, &address_in6) > 0)
	{
		if (inet_pton(AF_INET6, mask, &mask_in6) > 0)
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
			free(mask);
			return false;
		}
	}
	else
#endif
	if (inet_pton(AF_INET, address, &address_in4) > 0)
	{
		if (inet_pton(AF_INET, mask, &mask_in4) > 0)
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
			free(mask);
			return false;
		}
	}
	else
	{
		/* The address was neither ipv4 or ipv6 */
		free(mask);
		return false;
	}

	/* Low-level-match the bits in the raw data */
	free(mask);
	return MatchCIDRBits(addr_raw, mask_raw, bits);
}



