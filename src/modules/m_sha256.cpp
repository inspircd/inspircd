/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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


/* m_sha256 - Based on m_opersha256 written by Special <john@yarbbles.com>
 * Modified and improved by Craig Edwards, December 2006.
 *
 *
 * FIPS 180-2 SHA-224/256/384/512 implementation
 * Last update: 05/23/2005
 * Issue date:  04/30/2005
 *
 * Copyright (C) 2005 Olivier Gay <olivier.gay@a3.epfl.ch>
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $ModDesc: Allows for SHA-256 encrypted oper passwords */

#include "inspircd.h"
#ifdef HAS_STDINT
#include <stdint.h>
#endif
#include "hash.h"

#ifndef HAS_STDINT
typedef unsigned int uint32_t;
#endif

#define SHA256_DIGEST_SIZE (256 / 8)
#define SHA256_BLOCK_SIZE  (512 / 8)

/** An sha 256 context, used by m_opersha256
 */
class SHA256Context
{
 public:
	unsigned int tot_len;
	unsigned int len;
	unsigned char block[2 * SHA256_BLOCK_SIZE];
	uint32_t h[8];
};

#define SHFR(x, n)    (x >> n)
#define ROTR(x, n)   ((x >> n) | (x << ((sizeof(x) << 3) - n)))
#define ROTL(x, n)   ((x << n) | (x >> ((sizeof(x) << 3) - n)))
#define CH(x, y, z)  ((x & y) ^ (~x & z))
#define MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))

#define SHA256_F1(x) (ROTR(x,  2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SHA256_F2(x) (ROTR(x,  6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SHA256_F3(x) (ROTR(x,  7) ^ ROTR(x, 18) ^ SHFR(x,  3))
#define SHA256_F4(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHFR(x, 10))

#define UNPACK32(x, str)		       \
{					      \
	*((str) + 3) = (uint8_t) ((x)      );      \
	*((str) + 2) = (uint8_t) ((x) >>  8);      \
	*((str) + 1) = (uint8_t) ((x) >> 16);      \
	*((str) + 0) = (uint8_t) ((x) >> 24);      \
}

#define PACK32(str, x)			 \
{					      \
	*(x) = ((uint32_t) *((str) + 3)      )     \
	| ((uint32_t) *((str) + 2) <<  8)     \
	| ((uint32_t) *((str) + 1) << 16)     \
	| ((uint32_t) *((str) + 0) << 24);    \
}

/* Macros used for loops unrolling */

#define SHA256_SCR(i)			  \
{					      \
	w[i] =  SHA256_F4(w[i - 2]) + w[i - 7]     \
	+ SHA256_F3(w[i - 15]) + w[i - 16];  \
}

const unsigned int sha256_h0[8] =
{
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

uint32_t sha256_k[64] =
{
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

class HashSHA256 : public HashProvider
{
	void SHA256Init(SHA256Context *ctx, const unsigned int* ikey)
	{
		if (ikey)
		{
			for (int i = 0; i < 8; i++)
				ctx->h[i] = ikey[i];
		}
		else
		{
			for (int i = 0; i < 8; i++)
				ctx->h[i] = sha256_h0[i];
		}
		ctx->len = 0;
		ctx->tot_len = 0;
	}

	void SHA256Transform(SHA256Context *ctx, unsigned char *message, unsigned int block_nb)
	{
		uint32_t w[64];
		uint32_t wv[8];
		unsigned char *sub_block;
		for (unsigned int i = 1; i <= block_nb; i++)
		{
			int j;
			sub_block = message + ((i - 1) << 6);

			for (j = 0; j < 16; j++)
				PACK32(&sub_block[j << 2], &w[j]);
			for (j = 16; j < 64; j++)
				SHA256_SCR(j);
			for (j = 0; j < 8; j++)
				wv[j] = ctx->h[j];
			for (j = 0; j < 64; j++)
			{
				uint32_t t1 = wv[7] + SHA256_F2(wv[4]) + CH(wv[4], wv[5], wv[6]) + sha256_k[j] + w[j];
				uint32_t t2 = SHA256_F1(wv[0]) + MAJ(wv[0], wv[1], wv[2]);
				wv[7] = wv[6];
				wv[6] = wv[5];
				wv[5] = wv[4];
				wv[4] = wv[3] + t1;
				wv[3] = wv[2];
				wv[2] = wv[1];
				wv[1] = wv[0];
				wv[0] = t1 + t2;
			}
			for (j = 0; j < 8; j++)
				ctx->h[j] += wv[j];
		}
	}

	void SHA256Update(SHA256Context *ctx, unsigned char *message, unsigned int len)
	{
		/*
		 * XXX here be dragons!
		 * After many hours of pouring over this, I think I've found the problem.
		 * When Special created our module from the reference one, he used:
		 *
		 *     unsigned int rem_len = SHA256_BLOCK_SIZE - ctx->len;
		 *
		 * instead of the reference's version of:
		 *
		 *     unsigned int tmp_len = SHA256_BLOCK_SIZE - ctx->len;
		 *     unsigned int rem_len = len < tmp_len ? len : tmp_len;
		 *
		 * I've changed back to the reference version of this code, and it seems to work with no errors.
		 * So I'm inclined to believe this was the problem..
		 *             -- w00t (January 06, 2008)
		 */
		unsigned int tmp_len = SHA256_BLOCK_SIZE - ctx->len;
		unsigned int rem_len = len < tmp_len ? len : tmp_len;


		memcpy(&ctx->block[ctx->len], message, rem_len);
		if (ctx->len + len < SHA256_BLOCK_SIZE)
		{
			ctx->len += len;
			return;
		}
		unsigned int new_len = len - rem_len;
		unsigned int block_nb = new_len / SHA256_BLOCK_SIZE;
		unsigned char *shifted_message = message + rem_len;
		SHA256Transform(ctx, ctx->block, 1);
		SHA256Transform(ctx, shifted_message, block_nb);
		rem_len = new_len % SHA256_BLOCK_SIZE;
		memcpy(ctx->block, &shifted_message[block_nb << 6],rem_len);
		ctx->len = rem_len;
		ctx->tot_len += (block_nb + 1) << 6;
	}

	void SHA256Final(SHA256Context *ctx, unsigned char *digest)
	{
		unsigned int block_nb = (1 + ((SHA256_BLOCK_SIZE - 9) < (ctx->len % SHA256_BLOCK_SIZE)));
		unsigned int len_b = (ctx->tot_len + ctx->len) << 3;
		unsigned int pm_len = block_nb << 6;
		memset(ctx->block + ctx->len, 0, pm_len - ctx->len);
		ctx->block[ctx->len] = 0x80;
		UNPACK32(len_b, ctx->block + pm_len - 4);
		SHA256Transform(ctx, ctx->block, block_nb);
		for (int i = 0 ; i < 8; i++)
			UNPACK32(ctx->h[i], &digest[i << 2]);
	}

	void SHA256(const char *src, unsigned char *dest, unsigned int len)
	{
		SHA256Context ctx;
		SHA256Init(&ctx, NULL);
		SHA256Update(&ctx, (unsigned char *)src, len);
		SHA256Final(&ctx, dest);
	}

 public:
	std::string sum(const std::string& data)
	{
		unsigned char bytes[SHA256_DIGEST_SIZE];
		SHA256(data.data(), bytes, data.length());
		return std::string((char*)bytes, SHA256_DIGEST_SIZE);
	}

	std::string sumIV(unsigned int* IV, const char* HexMap, const std::string &sdata)
	{
		return "";
	}

	HashSHA256(Module* parent) : HashProvider(parent, "hash/sha256", 32, 64) {}
};

class ModuleSHA256 : public Module
{
	HashSHA256 sha;
 public:
	ModuleSHA256() : sha(this)
	{
		ServerInstance->Modules->AddService(sha);
	}

	Version GetVersion()
	{
		return Version("Implements SHA-256 hashing", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSHA256)
