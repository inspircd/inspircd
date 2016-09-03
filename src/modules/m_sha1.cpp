/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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

/*
SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain
*/

#include "inspircd.h"
#include "modules/hash.h"

union CHAR64LONG16
{
	unsigned char c[64];
	uint32_t l[16];
};

inline static uint32_t rol(uint32_t value, uint32_t bits) { return (value << bits) | (value >> (32 - bits)); }

// blk0() and blk() perform the initial expand.
// I got the idea of expanding during the round function from SSLeay
static bool big_endian;
inline static uint32_t blk0(CHAR64LONG16& block, uint32_t i)
{
	if (big_endian)
		return block.l[i];
	else
		return block.l[i] = (rol(block.l[i], 24) & 0xFF00FF00) | (rol(block.l[i], 8) & 0x00FF00FF);
}
inline static uint32_t blk(CHAR64LONG16 &block, uint32_t i) { return block.l[i & 15] = rol(block.l[(i + 13) & 15] ^ block.l[(i + 8) & 15] ^ block.l[(i + 2) & 15] ^ block.l[i & 15],1); }

// (R0+R1), R2, R3, R4 are the different operations used in SHA1
inline static void R0(CHAR64LONG16& block, uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, uint32_t i) { z += ((w & (x ^ y)) ^ y) + blk0(block, i) + 0x5A827999 + rol(v, 5); w = rol(w, 30); }
inline static void R1(CHAR64LONG16& block, uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, uint32_t i) { z += ((w & (x ^ y)) ^ y) + blk(block, i) + 0x5A827999 + rol(v, 5); w = rol(w, 30); }
inline static void R2(CHAR64LONG16& block, uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, uint32_t i) { z += (w ^ x ^ y) + blk(block, i) + 0x6ED9EBA1 + rol(v, 5); w = rol(w, 30); }
inline static void R3(CHAR64LONG16& block, uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, uint32_t i) { z += (((w | x) & y) | (w & x)) + blk(block, i) + 0x8F1BBCDC + rol(v, 5); w = rol(w, 30); }
inline static void R4(CHAR64LONG16& block, uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, uint32_t i) { z += (w ^ x ^ y) + blk(block, i) + 0xCA62C1D6 + rol(v, 5); w = rol(w, 30); }

static const uint32_t sha1_iv[5] =
{
	0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0
};

class SHA1Context
{
	uint32_t state[5];
	uint32_t count[2];
	unsigned char buffer[64];
	unsigned char digest[20];

	void Transform(const unsigned char buf[64])
	{
		uint32_t a, b, c, d, e;

		CHAR64LONG16 block;
		memcpy(block.c, buf, 64);

		// Copy state[] to working vars
		a = this->state[0];
		b = this->state[1];
		c = this->state[2];
		d = this->state[3];
		e = this->state[4];

		// 4 rounds of 20 operations each. Loop unrolled.
		R0(block, a, b, c, d, e, 0); R0(block, e, a, b, c, d, 1); R0(block, d, e, a, b, c, 2); R0(block, c, d, e, a, b, 3);
		R0(block, b, c, d, e, a, 4); R0(block, a, b, c, d, e, 5); R0(block, e, a, b, c, d, 6); R0(block, d, e, a, b, c, 7);
		R0(block, c, d, e, a, b, 8); R0(block, b, c, d, e, a, 9); R0(block, a, b, c, d, e, 10); R0(block, e, a, b, c, d, 11);
		R0(block, d, e, a, b, c, 12); R0(block, c, d, e, a, b, 13); R0(block, b, c, d, e, a, 14); R0(block, a, b, c, d, e, 15);
		R1(block, e, a, b, c, d, 16); R1(block, d, e, a, b, c, 17); R1(block, c, d, e, a, b, 18); R1(block, b, c, d, e, a, 19);
		R2(block, a, b, c, d, e, 20); R2(block, e, a, b, c, d, 21); R2(block, d, e, a, b, c, 22); R2(block, c, d, e, a, b, 23);
		R2(block, b, c, d, e, a, 24); R2(block, a, b, c, d, e, 25); R2(block, e, a, b, c, d, 26); R2(block, d, e, a, b, c, 27);
		R2(block, c, d, e, a, b, 28); R2(block, b, c, d, e, a, 29); R2(block, a, b, c, d, e, 30); R2(block, e, a, b, c, d, 31);
		R2(block, d, e, a, b, c, 32); R2(block, c, d, e, a, b, 33); R2(block, b, c, d, e, a, 34); R2(block, a, b, c, d, e, 35);
		R2(block, e, a, b, c, d, 36); R2(block, d, e, a, b, c, 37); R2(block, c, d, e, a, b, 38); R2(block, b, c, d, e, a, 39);
		R3(block, a, b, c, d, e, 40); R3(block, e, a, b, c, d, 41); R3(block, d, e, a, b, c, 42); R3(block, c, d, e, a, b, 43);
		R3(block, b, c, d, e, a, 44); R3(block, a, b, c, d, e, 45); R3(block, e, a, b, c, d, 46); R3(block, d, e, a, b, c, 47);
		R3(block, c, d, e, a, b, 48); R3(block, b, c, d, e, a, 49); R3(block, a, b, c, d, e, 50); R3(block, e, a, b, c, d, 51);
		R3(block, d, e, a, b, c, 52); R3(block, c, d, e, a, b, 53); R3(block, b, c, d, e, a, 54); R3(block, a, b, c, d, e, 55);
		R3(block, e, a, b, c, d, 56); R3(block, d, e, a, b, c, 57); R3(block, c, d, e, a, b, 58); R3(block, b, c, d, e, a, 59);
		R4(block, a, b, c, d, e, 60); R4(block, e, a, b, c, d, 61); R4(block, d, e, a, b, c, 62); R4(block, c, d, e, a, b, 63);
		R4(block, b, c, d, e, a, 64); R4(block, a, b, c, d, e, 65); R4(block, e, a, b, c, d, 66); R4(block, d, e, a, b, c, 67);
		R4(block, c, d, e, a, b, 68); R4(block, b, c, d, e, a, 69); R4(block, a, b, c, d, e, 70); R4(block, e, a, b, c, d, 71);
		R4(block, d, e, a, b, c, 72); R4(block, c, d, e, a, b, 73); R4(block, b, c, d, e, a, 74); R4(block, a, b, c, d, e, 75);
		R4(block, e, a, b, c, d, 76); R4(block, d, e, a, b, c, 77); R4(block, c, d, e, a, b, 78); R4(block, b, c, d, e, a, 79);
		// Add the working vars back into state[]
		this->state[0] += a;
		this->state[1] += b;
		this->state[2] += c;
		this->state[3] += d;
		this->state[4] += e;
	}

 public:
	SHA1Context()
	{
		for (int i = 0; i < 5; ++i)
			this->state[i] = sha1_iv[i];

		this->count[0] = this->count[1] = 0;
		memset(this->buffer, 0, sizeof(this->buffer));
		memset(this->digest, 0, sizeof(this->digest));
	}

	void Update(const unsigned char* data, size_t len)
	{
		uint32_t i, j;

		j = (this->count[0] >> 3) & 63;
		if ((this->count[0] += len << 3) < (len << 3))
			++this->count[1];
		this->count[1] += len >> 29;
		if (j + len > 63)
		{
			memcpy(&this->buffer[j], data, (i = 64 - j));
			this->Transform(this->buffer);
			for (; i + 63 < len; i += 64)
				this->Transform(&data[i]);
			j = 0;
		}
		else
			i = 0;
		memcpy(&this->buffer[j], &data[i], len - i);
	}

	void Finalize()
	{
		uint32_t i;
		unsigned char finalcount[8];

		for (i = 0; i < 8; ++i)
			finalcount[i] = static_cast<unsigned char>((this->count[i >= 4 ? 0 : 1] >> ((3 - (i & 3)) * 8)) & 255); /* Endian independent */
		this->Update(reinterpret_cast<const unsigned char *>("\200"), 1);
		while ((this->count[0] & 504) != 448)
			this->Update(reinterpret_cast<const unsigned char *>("\0"), 1);
		this->Update(finalcount, 8); // Should cause a SHA1Transform()
		for (i = 0; i < 20; ++i)
			this->digest[i] = static_cast<unsigned char>((this->state[i>>2] >> ((3 - (i & 3)) * 8)) & 255);

		this->Transform(this->buffer);
	}

	std::string GetRaw() const
	{
		return std::string((const char*)digest, sizeof(digest));
	}
};

class SHA1HashProvider : public HashProvider
{
 public:
 	SHA1HashProvider(Module* mod)
		: HashProvider(mod, "hash/sha1", 20, 64)
	{
	}

	std::string GenerateRaw(const std::string& data)
	{
		SHA1Context ctx;
		ctx.Update(reinterpret_cast<const unsigned char*>(data.data()), data.length());
		ctx.Finalize();
		return ctx.GetRaw();
	}
};

class ModuleSHA1 : public Module
{
	SHA1HashProvider sha1;

 public:
	ModuleSHA1()
		: sha1(this)
	{
		big_endian = (htonl(1337) == 1337);
	}

	Version GetVersion()
	{
		return Version("Implements SHA-1 hashing", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSHA1)
