/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
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

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED `AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HAS_STRLCPY
CoreExport size_t strlcat(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz, dlen;

	while (n-- != 0 && *d != '\0')
		d++;

	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));

	while (*s != '\0')
	{
		if (n != 1)
		{
			*d++ = *s;
			n--;
		}

		s++;
	}

	*d = '\0';
	return(dlen + (s - src)); /* count does not include NUL */
}

CoreExport size_t strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0)
	{
		do
		{
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0)
	{
		if (siz != 0)
			*d = '\0'; /* NUL-terminate dst */
		while (*s++);
	}

	return(s - src - 1); /* count does not include NUL */
}
#endif

CoreExport int charlcat(char* x,char y,int z)
{
	char* x__n = x;
	int v = 0;

	while(*x__n++)
		v++;

	if (v < z - 1)
	{
		*--x__n = y;
		*++x__n = 0;
	}

	return v;
}

CoreExport bool charremove(char* mp, char remove)
{
	char* mptr = mp;
	bool shift_down = false;

	while (*mptr)
	{
		if (*mptr == remove)
		shift_down = true;

		if (shift_down)
			*mptr = *(mptr+1);

		mptr++;
	}

	return shift_down;
}

static const char hextable[] = "0123456789abcdef";

std::string BinToHex(const std::string& data)
{
	int l = data.length();
	std::string rv;
	rv.reserve(l * 2);
	for(int i=0; i < l; i++)
	{
		unsigned char c = data[i];
		rv.append(1, hextable[c >> 4]);
		rv.append(1, hextable[c & 0xF]);
	}
	return rv;
}

static const char b64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string BinToBase64(const std::string& data_str, const char* table, char pad)
{
	if (!table)
		table = b64table;

	uint32_t buffer;
	uint8_t* data = (uint8_t*)data_str.data();
	std::string rv;
	size_t i = 0;
	while (i + 2 < data_str.length())
	{
		buffer = (data[i] << 16 | data[i+1] << 8 | data[i+2]);
		rv.push_back(table[0x3F & (buffer >> 18)]);
		rv.push_back(table[0x3F & (buffer >> 12)]);
		rv.push_back(table[0x3F & (buffer >>  6)]);
		rv.push_back(table[0x3F & (buffer >>  0)]);
		i += 3;
	}
	if (data_str.length() == i)
	{
		// no extra characters
	}
	else if (data_str.length() == i + 1)
	{
		buffer = data[i] << 16;
		rv.push_back(table[0x3F & (buffer >> 18)]);
		rv.push_back(table[0x3F & (buffer >> 12)]);
		if (pad)
		{
			rv.push_back(pad);
			rv.push_back(pad);
		}
	}
	else if (data_str.length() == i + 2)
	{
		buffer = (data[i] << 16 | data[i+1] << 8);
		rv.push_back(table[0x3F & (buffer >> 18)]);
		rv.push_back(table[0x3F & (buffer >> 12)]);
		rv.push_back(table[0x3F & (buffer >>  6)]);
		if (pad)
			rv.push_back(pad);
	}
	return rv;
}

std::string Base64ToBin(const std::string& data_str, const char* table)
{
	if (!table)
		table = b64table;

	int bitcount = 0;
	uint32_t buffer = 0;
	const char* data = data_str.c_str();
	std::string rv;
	while (true)
	{
		const char* find = strchr(table, *data++);
		if (!find || find >= table + 64)
			break;
		buffer = (buffer << 6) | (find - table);
		bitcount += 6;
		if (bitcount >= 8)
		{
			bitcount -= 8;
			rv.push_back((buffer >> bitcount) & 0xFF);
		}
	}
	return rv;
}
