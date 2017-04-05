/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Peter Powell <petpow@saberuk.com>
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
 * This module is based on the public domain Keccak reference implementation by
 * the Keccak, Keyak and Ketje Teams, namely, Guido Bertoni, Joan Daemen,
 * Michaël Peeters, Gilles Van Assche and Ronny Van Keer.
 *
 * Source: https://github.com/gvanas/KeccakCodePackage/
 */


#include "inspircd.h"
#include "modules/hash.h"

class SHA3Provider : public HashProvider
{
 private:
	int capacity;

	/*
	================================================================
	Technicalities
	================================================================
	*/

	typedef uint64_t tKeccakLane;

	/** Function to load a 64-bit value using the little-endian (LE) convention.
	 * On a LE platform, this could be greatly simplified using a cast.
	 */
	static uint64_t load64(const uint8_t* x)
	{
		int i;
		uint64_t u=0;

		for(i=7; i>=0; --i) {
			u <<= 8;
			u |= x[i];
		}
		return u;
	}

	/** Function to store a 64-bit value using the little-endian (LE) convention.
	 * On a LE platform, this could be greatly simplified using a cast.
	 */
	static void store64(uint8_t* x, uint64_t u)
	{
		unsigned int i;

		for(i=0; i<8; ++i) {
			x[i] = u;
			u >>= 8;
		}
	}

	/** Function to XOR into a 64-bit value using the little-endian (LE) convention.
	 * On a LE platform, this could be greatly simplified using a cast.
	 */
	static void xor64(uint8_t* x, uint64_t u)
	{
		unsigned int i;

		for(i=0; i<8; ++i) {
			x[i] ^= u;
			u >>= 8;
		}
	}

	/*
	================================================================
	A readable and compact implementation of the Keccak-f[1600] permutation.
	================================================================
	*/

#define ROL64(a, offset) ((((uint64_t)a) << offset) ^ (((uint64_t)a) >> (64-offset)))
#define i(x, y) ((x)+5*(y))
#define readLane(x, y)          load64((uint8_t*)state+sizeof(tKeccakLane)*i(x, y))
#define writeLane(x, y, lane)   store64((uint8_t*)state+sizeof(tKeccakLane)*i(x, y), lane)
#define XORLane(x, y, lane)     xor64((uint8_t*)state+sizeof(tKeccakLane)*i(x, y), lane)

	/**
	 * Function that computes the linear feedback shift register (LFSR) used to
	 * define the round constants (see [Keccak Reference, Section 1.2]).
	 */
	int LFSR86540(uint8_t* LFSR)
	{
		int result = ((*LFSR) & 0x01) != 0;
		if (((*LFSR) & 0x80) != 0)
		/* Primitive polynomial over GF(2): x^8+x^6+x^5+x^4+1 */
			(*LFSR) = ((*LFSR) << 1) ^ 0x71;
		else
			(*LFSR) <<= 1;
		return result;
	}

	/**
	 * Function that computes the Keccak-f[1600] permutation on the given state.
	 */
	void KeccakF1600_StatePermute(void* state)
	{
		unsigned int round, x, y, j, t;
		uint8_t LFSRstate = 0x01;

		for(round=0; round<24; round++) {
			{   /* === θ step (see [Keccak Reference, Section 2.3.2]) === */
				tKeccakLane C[5], D;

				/* Compute the parity of the columns */
				for(x=0; x<5; x++)
					C[x] = readLane(x, 0) ^ readLane(x, 1) ^ readLane(x, 2) ^ readLane(x, 3) ^ readLane(x, 4);
				for(x=0; x<5; x++) {
					/* Compute the θ effect for a given column */
					D = C[(x+4)%5] ^ ROL64(C[(x+1)%5], 1);
					/* Add the θ effect to the whole column */
					for (y=0; y<5; y++)
						XORLane(x, y, D);
				}
			}

			{   /* === ρ and π steps (see [Keccak Reference, Sections 2.3.3 and 2.3.4]) === */
				tKeccakLane current, temp;
				/* Start at coordinates (1 0) */
				x = 1; y = 0;
				current = readLane(x, y);
				/* Iterate over ((0 1)(2 3))^t * (1 0) for 0 ≤ t ≤ 23 */
				for(t=0; t<24; t++) {
					/* Compute the rotation constant r = (t+1)(t+2)/2 */
					unsigned int r = ((t+1)*(t+2)/2)%64;
					/* Compute ((0 1)(2 3)) * (x y) */
					unsigned int Y = (2*x+3*y)%5; x = y; y = Y;
					/* Swap current and state(x,y), and rotate */
					temp = readLane(x, y);
					writeLane(x, y, ROL64(current, r));
					current = temp;
				}
			}

			{   /* === χ step (see [Keccak Reference, Section 2.3.1]) === */
				tKeccakLane temp[5];
				for(y=0; y<5; y++) {
					/* Take a copy of the plane */
					for(x=0; x<5; x++)
						temp[x] = readLane(x, y);
					/* Compute χ on the plane */
					for(x=0; x<5; x++)
						writeLane(x, y, temp[x] ^((~temp[(x+1)%5]) & temp[(x+2)%5]));
				}
			}

			{   /* === ι step (see [Keccak Reference, Section 2.3.5]) === */
				for(j=0; j<7; j++) {
					unsigned int bitPosition = (1<<j)-1; /* 2^j-1 */
					if (LFSR86540(&LFSRstate))
						XORLane(0, 0, (tKeccakLane)1<<bitPosition);
				}
			}
		}
	}

	/*
	================================================================
	A readable and compact implementation of the Keccak sponge functions
	that use the Keccak-f[1600] permutation.
	================================================================
	*/

	/**
	 * Function to compute the Keccak[r, c] sponge function over a given input.
	 * @param  rate            The value of the rate r.
	 * @param  input           Pointer to the input message.
	 * @param  inputByteLen    The number of input bytes provided in the input message.
	 * @param  delimitedSuffix Bits that will be automatically appended to the end
	 *                         of the input message, as in domain separation.
	 *                         This is a byte containing from 0 to 7 bits
	 *                         These <i>n</i> bits must be in the least significant bit positions
	 *                         and must be delimited with a bit 1 at position <i>n</i>
	 *                         (counting from 0=LSB to 7=MSB) and followed by bits 0
	 *                         from position <i>n</i>+1 to position 7.
	 *                         Some examples:
	 *                             - If no bits are to be appended, then @a delimitedSuffix must be 0x01.
	 *                             - If the 2-bit sequence 0,1 is to be appended (as for SHA3-*), @a delimitedSuffix must be 0x06.
	 *                             - If the 4-bit sequence 1,1,1,1 is to be appended (as for SHAKE*), @a delimitedSuffix must be 0x1F.
	 *                             - If the 7-bit sequence 1,1,0,1,0,0,0 is to be absorbed, @a delimitedSuffix must be 0x8B.
	 * @param  output          Pointer to the buffer where to store the output.
	 * @param  outputByteLen   The number of output bytes desired.
	 * @pre    One must have r+c=1600 and the rate a multiple of 8 bits in this implementation.
	 */
	void Keccak(unsigned int rate, const unsigned char* input, unsigned int inputByteLen, unsigned char delimitedSuffix, unsigned char* output, unsigned int outputByteLen)
	{
		uint8_t state[200];
		unsigned int rateInBytes = rate/8;
		unsigned int blockSize = 0;
		unsigned int i;

		if (((rate + this->capacity) != 1600) || ((rate % 8) != 0))
			return;

		/* === Initialize the state === */
		memset(state, 0, sizeof(state));

		/* === Absorb all the input blocks === */
		while(inputByteLen > 0) {
			blockSize = std::min(inputByteLen, rateInBytes);
			for(i=0; i<blockSize; i++)
				state[i] ^= input[i];
			input += blockSize;
			inputByteLen -= blockSize;

			if (blockSize == rateInBytes) {
				KeccakF1600_StatePermute(state);
				blockSize = 0;
			}
		}

		/* === Do the padding and switch to the squeezing phase === */
		/* Absorb the last few bits and add the first bit of padding (which coincides with the delimiter in delimitedSuffix) */
		state[blockSize] ^= delimitedSuffix;
		/* If the first bit of padding is at position rate-1, we need a whole new block for the second bit of padding */
		if (((delimitedSuffix & 0x80) != 0) && (blockSize == (rateInBytes-1)))
			KeccakF1600_StatePermute(state);
		/* Add the second bit of padding */
		state[rateInBytes-1] ^= 0x80;
		/* Switch to the squeezing phase */
		KeccakF1600_StatePermute(state);

		/* === Squeeze out all the output blocks === */
		while(outputByteLen > 0) {
			blockSize = std::min(outputByteLen, rateInBytes);
			memcpy(output, state, blockSize);
			output += blockSize;
			outputByteLen -= blockSize;

			if (outputByteLen > 0)
				KeccakF1600_StatePermute(state);
		}
	}

 public:
	std::string GenerateRaw(const std::string& data) CXX11_OVERRIDE
	{
		std::vector<unsigned char> output(out_size);
		Keccak(this->block_size * 8, (const unsigned char*)data.c_str(), data.length(), 0x06, &output[0], out_size);
		return std::string((char*)&output[0], out_size);
	}

	SHA3Provider(Module* Creator, const std::string& Name, unsigned int Rate, unsigned int Capacity, int OutputSize)
		: HashProvider(Creator, Name, OutputSize, Rate / 8)
		, capacity(Capacity)
	{
	}
};

#define SHA3_TEST(PROVIDER, NAME, EXPECTED) \
	do { \
		const std::string result = PROVIDER.Generate(""); \
		if (result != EXPECTED) { \
			throw ModuleException("CRITICAL: " NAME " implementation is producing incorrect results! Please report this as soon as possible."); \
		} \
	} while (0)

class ModuleSHA3 : public Module
{
	SHA3Provider sha3_224;
	SHA3Provider sha3_256;
	SHA3Provider sha3_384;
	SHA3Provider sha3_512;

 public:
	ModuleSHA3()
		: sha3_224(this, "sha3-224", 1152, 448,  28)
		, sha3_256(this, "sha3-256", 1088, 512,  32)
		, sha3_384(this, "sha3-384", 832,  768,  48)
		, sha3_512(this, "sha3-512", 576,  1024, 64)
	{
	}

	void init() CXX11_OVERRIDE
	{
		SHA3_TEST(sha3_224, "SHA3-224", "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7");
		SHA3_TEST(sha3_256, "SHA3-256", "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
		SHA3_TEST(sha3_384, "SHA3-384", "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004");
		SHA3_TEST(sha3_512, "SHA3-512", "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26");
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		static std::string testhash = sha3_224.Generate("testhash");
		return Version("Implements support for the SHA-3 hash algorithm.", VF_VENDOR, testhash);
	}
};

MODULE_INIT(ModuleSHA3)
