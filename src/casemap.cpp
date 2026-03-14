/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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

const insp::casemap ascii_case_insensitive_map[256] = {
	0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,
	16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
	32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
	48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
	64,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
	112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 91,  92,  93,  94,  95,
	96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
	112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
	128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
	144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
	176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
	192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
	208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
	224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
};

const insp::casemap* national_case_insensitive_map = ascii_case_insensitive_map;

// These functions are from the Public Domain implementation of MurmurHash2
// by Austin Appleby but modified to remove C++ warnings and to hash case
// insensitively using the IRC casemapping. Do not make changes to them if you
// do not know what you are doing.
#if SIZE_MAX == UINT64_MAX
# define MURMUR_SEED_MAX UINT64_MAX
# define MURMUR_SEED_TYPE uint64_t
# define MURMUR_HASH MurmurHash64A
static uint64_t MurmurHash64A(const void* key, int len, uint64_t seed)
{
	const uint64_t m = 0xc6a4a7935bd1e995ULL;
	const int r = 47;

	uint64_t h = seed ^ (len * m);

	const uint64_t* data = (const uint64_t*)key;
	const uint64_t* end = data + (len / 8);

	while (data != end)
	{
		uint64_t k = *data++;

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	const unsigned char* data2 = (const unsigned char*)data;

	switch (len & 7)
	{
		case 7:
			h ^= uint64_t(national_case_insensitive_map[data2[6]]) << 48;
			[[fallthrough]];
		case 6:
			h ^= uint64_t(national_case_insensitive_map[data2[5]]) << 40;
			[[fallthrough]];
		case 5:
			h ^= uint64_t(national_case_insensitive_map[data2[4]]) << 32;
			[[fallthrough]];
		case 4:
			h ^= uint64_t(national_case_insensitive_map[data2[3]]) << 24;
			[[fallthrough]];
		case 3:
			h ^= uint64_t(national_case_insensitive_map[data2[2]]) << 16;
			[[fallthrough]];
		case 2:
			h ^= uint64_t(national_case_insensitive_map[data2[1]]) << 8;
			[[fallthrough]];
		case 1:
			h ^= uint64_t(national_case_insensitive_map[data2[0]]);
			h *= m;
			break;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}
#else
# if SIZE_MAX != UINT32_MAX
#  warning SIZE_MAX is an unknown size; falling back to 32-bit MurmurHash
# endif
# define MURMUR_SEED_MAX UINT32_MAX
# define MURMUR_SEED_TYPE uint32_t
# define MURMUR_HASH MurmurHash2A

# define mmix(h,k) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }
static uint32_t MurmurHash2A(const void * key, int len, uint32_t seed)
{
	const uint32_t m = 0x5bd1e995;
	const int r = 24;
	uint32_t l = len;

	const unsigned char* data = (const unsigned char*)key;

	uint32_t h = seed;

	while(len >= 4)
	{
		uint32_t k = *(uint32_t*)data;

		mmix(h, k);

		data += 4;
		len -= 4;
	}

	uint32_t t = 0;

	switch(len)
	{
		case 3:
			t ^= national_case_insensitive_map[data[2]] << 16;
			[[fallthrough]];
		case 2:
			t ^= national_case_insensitive_map[data[1]] << 8;
			[[fallthrough]];
		case 1:
			t ^= national_case_insensitive_map[data[0]];
			break;
	};

	mmix(h, t);
	mmix(h, l);

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}
# undef mmix
#endif


bool insp::casemapped_equals(const std::string_view& str1, const std::string_view& str2)
{
	if (str1.length() != str2.length())
		return false;

	for (size_t idx = 0; idx < str1.length(); ++idx)
	{
		const auto chr1 = static_cast<unsigned char>(str1[idx]);
		const auto chr2 = static_cast<unsigned char>(str2[idx]);
		if (national_case_insensitive_map[chr1] != national_case_insensitive_map[chr2])
			return false;
	}
	return true;
}

bool insp::casemapped_less(const std::string_view& str1, const std::string_view& str2)
{
	const auto maxsize = std::min(str1.length(), str2.length());
	for (size_t idx = 0; idx < maxsize; ++idx)
	{
		const auto chr1 = national_case_insensitive_map[static_cast<unsigned char>(str1[idx])];
		const auto chr2 = national_case_insensitive_map[static_cast<unsigned char>(str2[idx])];
		if (chr1 > chr2)
			return false;
		else if (chr1 < chr2)
			return true;
	}
	return str1.length() < str2.length();
}

size_t insp::casemapped_hash(const std::string_view& str)
{
	static auto seed_initialized = false;
	static MURMUR_SEED_TYPE seed;
	if (!seed_initialized) [[unlikely]]
	{
		seed = ServerInstance->GenRandomInt(MURMUR_SEED_MAX);
		seed_initialized = true;
	}
	return MURMUR_HASH(str.data(), static_cast<int>(str.length()), seed);
}
