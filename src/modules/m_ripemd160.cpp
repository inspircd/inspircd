/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
 *
 *      AUTHOR:   Antoon Bosselaers, ESAT-COSIC
 *      DATE:     1 March 1996
 *      VERSION:  1.0
 *
 *      Copyright (c) Katholieke Universiteit Leuven
 *      1996, All Rights Reserved
 *
 *  Conditions for use of the RIPEMD-160 Software
 *
 *  The RIPEMD-160 software is freely available for use under the terms and
 *  conditions described hereunder, which shall be deemed to be accepted by
 *  any user of the software and applicable on any use of the software:
 *
 *  1. K.U.Leuven Department of Electrical Engineering-ESAT/COSIC shall for
 *     all purposes be considered the owner of the RIPEMD-160 software and of
 *     all copyright, trade secret, patent or other intellectual property
 *     rights therein.
 *  2. The RIPEMD-160 software is provided on an "as is" basis without
 *     warranty of any sort, express or implied. K.U.Leuven makes no
 *     representation that the use of the software will not infringe any
 *     patent or proprietary right of third parties. User will indemnify
 *     K.U.Leuven and hold K.U.Leuven harmless from any claims or liabilities
 *     which may arise as a result of its use of the software. In no
 *     circumstances K.U.Leuven R&D will be held liable for any deficiency,
 *     fault or other mishappening with regard to the use or performance of
 *     the software.
 *  3. User agrees to give due credit to K.U.Leuven in scientific publications
 *     or communications in relation with the use of the RIPEMD-160 software
 *     as follows: RIPEMD-160 software written by Antoon Bosselaers,
 *     available at http://www.esat.kuleuven.be/~cosicart/ps/AB-9601/.
 *
 */


/* $ModDesc: Allows for RIPEMD-160 encrypted oper passwords */

/* macro definitions */

#include "inspircd.h"
#ifdef HAS_STDINT
#include <stdint.h>
#endif
#include "hash.h"

#define RMDsize 160

#ifndef HAS_STDINT
typedef		unsigned char		byte;
typedef		unsigned int		dword;
#else
typedef		uint8_t			byte;
typedef		uint32_t		dword;
#endif

/* collect four bytes into one word: */
#define BYTES_TO_DWORD(strptr)                    \
            (((dword) *((strptr)+3) << 24) | \
             ((dword) *((strptr)+2) << 16) | \
             ((dword) *((strptr)+1) <<  8) | \
             ((dword) *(strptr)))

/* ROL(x, n) cyclically rotates x over n bits to the left */
/* x must be of an unsigned 32 bits type and 0 <= n < 32. */
#define ROL(x, n)        (((x) << (n)) | ((x) >> (32-(n))))

/* the five basic functions F(), G() and H() */
#define F(x, y, z)        ((x) ^ (y) ^ (z))
#define G(x, y, z)        (((x) & (y)) | (~(x) & (z)))
#define H(x, y, z)        (((x) | ~(y)) ^ (z))
#define I(x, y, z)        (((x) & (z)) | ((y) & ~(z)))
#define J(x, y, z)        ((x) ^ ((y) | ~(z)))

/* the ten basic operations FF() through III() */

#define FF(a, b, c, d, e, x, s)        {\
      (a) += F((b), (c), (d)) + (x);\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

#define GG(a, b, c, d, e, x, s)        {\
      (a) += G((b), (c), (d)) + (x) + 0x5a827999UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

#define HH(a, b, c, d, e, x, s)        {\
      (a) += H((b), (c), (d)) + (x) + 0x6ed9eba1UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

#define II(a, b, c, d, e, x, s)        {\
      (a) += I((b), (c), (d)) + (x) + 0x8f1bbcdcUL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

#define JJ(a, b, c, d, e, x, s)        {\
      (a) += J((b), (c), (d)) + (x) + 0xa953fd4eUL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

#define FFF(a, b, c, d, e, x, s)        {\
      (a) += F((b), (c), (d)) + (x);\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

#define GGG(a, b, c, d, e, x, s)        {\
      (a) += G((b), (c), (d)) + (x) + 0x7a6d76e9UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

#define HHH(a, b, c, d, e, x, s)        {\
      (a) += H((b), (c), (d)) + (x) + 0x6d703ef3UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

#define III(a, b, c, d, e, x, s)        {\
      (a) += I((b), (c), (d)) + (x) + 0x5c4dd124UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

#define JJJ(a, b, c, d, e, x, s)        {\
      (a) += J((b), (c), (d)) + (x) + 0x50a28be6UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }


class RIProv : public HashProvider
{

	void MDinit(dword *MDbuf, unsigned int* key)
	{
		if (key)
		{
			ServerInstance->Logs->Log("m_ripemd160.so", DEBUG, "initialize with custom mdbuf");
			MDbuf[0] = key[0];
			MDbuf[1] = key[1];
			MDbuf[2] = key[2];
			MDbuf[3] = key[3];
			MDbuf[4] = key[4];
		}
		else
		{
			ServerInstance->Logs->Log("m_ripemd160.so", DEBUG, "initialize with default mdbuf");
			MDbuf[0] = 0x67452301UL;
			MDbuf[1] = 0xefcdab89UL;
			MDbuf[2] = 0x98badcfeUL;
			MDbuf[3] = 0x10325476UL;
			MDbuf[4] = 0xc3d2e1f0UL;
		}
		return;
	}


	void compress(dword *MDbuf, dword *X)
	{
		dword aa = MDbuf[0],  bb = MDbuf[1],  cc = MDbuf[2],
			dd = MDbuf[3],  ee = MDbuf[4];
		dword aaa = MDbuf[0], bbb = MDbuf[1], ccc = MDbuf[2],
			ddd = MDbuf[3], eee = MDbuf[4];

		/* round 1 */
		FF(aa, bb, cc, dd, ee, X[ 0], 11);
		FF(ee, aa, bb, cc, dd, X[ 1], 14);
		FF(dd, ee, aa, bb, cc, X[ 2], 15);
		FF(cc, dd, ee, aa, bb, X[ 3], 12);
		FF(bb, cc, dd, ee, aa, X[ 4],  5);
		FF(aa, bb, cc, dd, ee, X[ 5],  8);
		FF(ee, aa, bb, cc, dd, X[ 6],  7);
		FF(dd, ee, aa, bb, cc, X[ 7],  9);
		FF(cc, dd, ee, aa, bb, X[ 8], 11);
		FF(bb, cc, dd, ee, aa, X[ 9], 13);
		FF(aa, bb, cc, dd, ee, X[10], 14);
		FF(ee, aa, bb, cc, dd, X[11], 15);
		FF(dd, ee, aa, bb, cc, X[12],  6);
		FF(cc, dd, ee, aa, bb, X[13],  7);
		FF(bb, cc, dd, ee, aa, X[14],  9);
		FF(aa, bb, cc, dd, ee, X[15],  8);

		/* round 2 */
		GG(ee, aa, bb, cc, dd, X[ 7],  7);
		GG(dd, ee, aa, bb, cc, X[ 4],  6);
		GG(cc, dd, ee, aa, bb, X[13],  8);
		GG(bb, cc, dd, ee, aa, X[ 1], 13);
		GG(aa, bb, cc, dd, ee, X[10], 11);
		GG(ee, aa, bb, cc, dd, X[ 6],  9);
		GG(dd, ee, aa, bb, cc, X[15],  7);
		GG(cc, dd, ee, aa, bb, X[ 3], 15);
		GG(bb, cc, dd, ee, aa, X[12],  7);
		GG(aa, bb, cc, dd, ee, X[ 0], 12);
		GG(ee, aa, bb, cc, dd, X[ 9], 15);
		GG(dd, ee, aa, bb, cc, X[ 5],  9);
		GG(cc, dd, ee, aa, bb, X[ 2], 11);
		GG(bb, cc, dd, ee, aa, X[14],  7);
		GG(aa, bb, cc, dd, ee, X[11], 13);
		GG(ee, aa, bb, cc, dd, X[ 8], 12);

		/* round 3 */
		HH(dd, ee, aa, bb, cc, X[ 3], 11);
		HH(cc, dd, ee, aa, bb, X[10], 13);
		HH(bb, cc, dd, ee, aa, X[14],  6);
		HH(aa, bb, cc, dd, ee, X[ 4],  7);
		HH(ee, aa, bb, cc, dd, X[ 9], 14);
		HH(dd, ee, aa, bb, cc, X[15],  9);
		HH(cc, dd, ee, aa, bb, X[ 8], 13);
		HH(bb, cc, dd, ee, aa, X[ 1], 15);
		HH(aa, bb, cc, dd, ee, X[ 2], 14);
		HH(ee, aa, bb, cc, dd, X[ 7],  8);
		HH(dd, ee, aa, bb, cc, X[ 0], 13);
		HH(cc, dd, ee, aa, bb, X[ 6],  6);
		HH(bb, cc, dd, ee, aa, X[13],  5);
		HH(aa, bb, cc, dd, ee, X[11], 12);
		HH(ee, aa, bb, cc, dd, X[ 5],  7);
		HH(dd, ee, aa, bb, cc, X[12],  5);

		/* round 4 */
		II(cc, dd, ee, aa, bb, X[ 1], 11);
		II(bb, cc, dd, ee, aa, X[ 9], 12);
		II(aa, bb, cc, dd, ee, X[11], 14);
		II(ee, aa, bb, cc, dd, X[10], 15);
		II(dd, ee, aa, bb, cc, X[ 0], 14);
		II(cc, dd, ee, aa, bb, X[ 8], 15);
		II(bb, cc, dd, ee, aa, X[12],  9);
		II(aa, bb, cc, dd, ee, X[ 4],  8);
		II(ee, aa, bb, cc, dd, X[13],  9);
		II(dd, ee, aa, bb, cc, X[ 3], 14);
		II(cc, dd, ee, aa, bb, X[ 7],  5);
		II(bb, cc, dd, ee, aa, X[15],  6);
		II(aa, bb, cc, dd, ee, X[14],  8);
		II(ee, aa, bb, cc, dd, X[ 5],  6);
		II(dd, ee, aa, bb, cc, X[ 6],  5);
		II(cc, dd, ee, aa, bb, X[ 2], 12);

		/* round 5 */
		JJ(bb, cc, dd, ee, aa, X[ 4],  9);
		JJ(aa, bb, cc, dd, ee, X[ 0], 15);
		JJ(ee, aa, bb, cc, dd, X[ 5],  5);
		JJ(dd, ee, aa, bb, cc, X[ 9], 11);
		JJ(cc, dd, ee, aa, bb, X[ 7],  6);
		JJ(bb, cc, dd, ee, aa, X[12],  8);
		JJ(aa, bb, cc, dd, ee, X[ 2], 13);
		JJ(ee, aa, bb, cc, dd, X[10], 12);
		JJ(dd, ee, aa, bb, cc, X[14],  5);
		JJ(cc, dd, ee, aa, bb, X[ 1], 12);
		JJ(bb, cc, dd, ee, aa, X[ 3], 13);
		JJ(aa, bb, cc, dd, ee, X[ 8], 14);
		JJ(ee, aa, bb, cc, dd, X[11], 11);
		JJ(dd, ee, aa, bb, cc, X[ 6],  8);
		JJ(cc, dd, ee, aa, bb, X[15],  5);
		JJ(bb, cc, dd, ee, aa, X[13],  6);

		/* parallel round 1 */
		JJJ(aaa, bbb, ccc, ddd, eee, X[ 5],  8);
		JJJ(eee, aaa, bbb, ccc, ddd, X[14],  9);
		JJJ(ddd, eee, aaa, bbb, ccc, X[ 7],  9);
		JJJ(ccc, ddd, eee, aaa, bbb, X[ 0], 11);
		JJJ(bbb, ccc, ddd, eee, aaa, X[ 9], 13);
		JJJ(aaa, bbb, ccc, ddd, eee, X[ 2], 15);
		JJJ(eee, aaa, bbb, ccc, ddd, X[11], 15);
		JJJ(ddd, eee, aaa, bbb, ccc, X[ 4],  5);
		JJJ(ccc, ddd, eee, aaa, bbb, X[13],  7);
		JJJ(bbb, ccc, ddd, eee, aaa, X[ 6],  7);
		JJJ(aaa, bbb, ccc, ddd, eee, X[15],  8);
		JJJ(eee, aaa, bbb, ccc, ddd, X[ 8], 11);
		JJJ(ddd, eee, aaa, bbb, ccc, X[ 1], 14);
		JJJ(ccc, ddd, eee, aaa, bbb, X[10], 14);
		JJJ(bbb, ccc, ddd, eee, aaa, X[ 3], 12);
		JJJ(aaa, bbb, ccc, ddd, eee, X[12],  6);

		/* parallel round 2 */
		III(eee, aaa, bbb, ccc, ddd, X[ 6],  9);
		III(ddd, eee, aaa, bbb, ccc, X[11], 13);
		III(ccc, ddd, eee, aaa, bbb, X[ 3], 15);
		III(bbb, ccc, ddd, eee, aaa, X[ 7],  7);
		III(aaa, bbb, ccc, ddd, eee, X[ 0], 12);
		III(eee, aaa, bbb, ccc, ddd, X[13],  8);
		III(ddd, eee, aaa, bbb, ccc, X[ 5],  9);
		III(ccc, ddd, eee, aaa, bbb, X[10], 11);
		III(bbb, ccc, ddd, eee, aaa, X[14],  7);
		III(aaa, bbb, ccc, ddd, eee, X[15],  7);
		III(eee, aaa, bbb, ccc, ddd, X[ 8], 12);
		III(ddd, eee, aaa, bbb, ccc, X[12],  7);
		III(ccc, ddd, eee, aaa, bbb, X[ 4],  6);
		III(bbb, ccc, ddd, eee, aaa, X[ 9], 15);
		III(aaa, bbb, ccc, ddd, eee, X[ 1], 13);
		III(eee, aaa, bbb, ccc, ddd, X[ 2], 11);

		/* parallel round 3 */
		HHH(ddd, eee, aaa, bbb, ccc, X[15],  9);
		HHH(ccc, ddd, eee, aaa, bbb, X[ 5],  7);
		HHH(bbb, ccc, ddd, eee, aaa, X[ 1], 15);
		HHH(aaa, bbb, ccc, ddd, eee, X[ 3], 11);
		HHH(eee, aaa, bbb, ccc, ddd, X[ 7],  8);
		HHH(ddd, eee, aaa, bbb, ccc, X[14],  6);
		HHH(ccc, ddd, eee, aaa, bbb, X[ 6],  6);
		HHH(bbb, ccc, ddd, eee, aaa, X[ 9], 14);
		HHH(aaa, bbb, ccc, ddd, eee, X[11], 12);
		HHH(eee, aaa, bbb, ccc, ddd, X[ 8], 13);
		HHH(ddd, eee, aaa, bbb, ccc, X[12],  5);
		HHH(ccc, ddd, eee, aaa, bbb, X[ 2], 14);
		HHH(bbb, ccc, ddd, eee, aaa, X[10], 13);
		HHH(aaa, bbb, ccc, ddd, eee, X[ 0], 13);
		HHH(eee, aaa, bbb, ccc, ddd, X[ 4],  7);
		HHH(ddd, eee, aaa, bbb, ccc, X[13],  5);

		/* parallel round 4 */
		GGG(ccc, ddd, eee, aaa, bbb, X[ 8], 15);
		GGG(bbb, ccc, ddd, eee, aaa, X[ 6],  5);
		GGG(aaa, bbb, ccc, ddd, eee, X[ 4],  8);
		GGG(eee, aaa, bbb, ccc, ddd, X[ 1], 11);
		GGG(ddd, eee, aaa, bbb, ccc, X[ 3], 14);
		GGG(ccc, ddd, eee, aaa, bbb, X[11], 14);
		GGG(bbb, ccc, ddd, eee, aaa, X[15],  6);
		GGG(aaa, bbb, ccc, ddd, eee, X[ 0], 14);
		GGG(eee, aaa, bbb, ccc, ddd, X[ 5],  6);
		GGG(ddd, eee, aaa, bbb, ccc, X[12],  9);
		GGG(ccc, ddd, eee, aaa, bbb, X[ 2], 12);
		GGG(bbb, ccc, ddd, eee, aaa, X[13],  9);
		GGG(aaa, bbb, ccc, ddd, eee, X[ 9], 12);
		GGG(eee, aaa, bbb, ccc, ddd, X[ 7],  5);
		GGG(ddd, eee, aaa, bbb, ccc, X[10], 15);
		GGG(ccc, ddd, eee, aaa, bbb, X[14],  8);

		/* parallel round 5 */
		FFF(bbb, ccc, ddd, eee, aaa, X[12] ,  8);
		FFF(aaa, bbb, ccc, ddd, eee, X[15] ,  5);
		FFF(eee, aaa, bbb, ccc, ddd, X[10] , 12);
		FFF(ddd, eee, aaa, bbb, ccc, X[ 4] ,  9);
		FFF(ccc, ddd, eee, aaa, bbb, X[ 1] , 12);
		FFF(bbb, ccc, ddd, eee, aaa, X[ 5] ,  5);
		FFF(aaa, bbb, ccc, ddd, eee, X[ 8] , 14);
		FFF(eee, aaa, bbb, ccc, ddd, X[ 7] ,  6);
		FFF(ddd, eee, aaa, bbb, ccc, X[ 6] ,  8);
		FFF(ccc, ddd, eee, aaa, bbb, X[ 2] , 13);
		FFF(bbb, ccc, ddd, eee, aaa, X[13] ,  6);
		FFF(aaa, bbb, ccc, ddd, eee, X[14] ,  5);
		FFF(eee, aaa, bbb, ccc, ddd, X[ 0] , 15);
		FFF(ddd, eee, aaa, bbb, ccc, X[ 3] , 13);
		FFF(ccc, ddd, eee, aaa, bbb, X[ 9] , 11);
		FFF(bbb, ccc, ddd, eee, aaa, X[11] , 11);

		/* combine results */
		ddd += cc + MDbuf[1];               /* final result for MDbuf[0] */
		MDbuf[1] = MDbuf[2] + dd + eee;
		MDbuf[2] = MDbuf[3] + ee + aaa;
		MDbuf[3] = MDbuf[4] + aa + bbb;
		MDbuf[4] = MDbuf[0] + bb + ccc;
		MDbuf[0] = ddd;

		return;
	}

	void MDfinish(dword *MDbuf, byte *strptr, dword lswlen, dword mswlen)
	{
		unsigned int i;                                 /* counter       */
		dword        X[16];                             /* message words */

		memset(X, 0, sizeof(X));

		/* put bytes from strptr into X */
		for (i=0; i<(lswlen&63); i++) {
			/* byte i goes into word X[i div 4] at pos.  8*(i mod 4)  */
			X[i>>2] ^= (dword) *strptr++ << (8 * (i&3));
		}

		/* append the bit m_n == 1 */
		X[(lswlen>>2)&15] ^= (dword)1 << (8*(lswlen&3) + 7);

		if ((lswlen & 63) > 55) {
			/* length goes to next block */
			compress(MDbuf, X);
			memset(X, 0, sizeof(X));
		}

		/* append length in bits*/
		X[14] = lswlen << 3;
		X[15] = (lswlen >> 29) | (mswlen << 3);
		compress(MDbuf, X);

		return;
	}

	byte *RMD(byte *message, dword length, unsigned int* key)
	{
		ServerInstance->Logs->Log("m_ripemd160", DEBUG, "RMD: '%s' length=%u", (const char*)message, length);
		dword         MDbuf[RMDsize/32];   /* contains (A, B, C, D(E))   */
		static byte   hashcode[RMDsize/8]; /* for final hash-value         */
		dword         X[16];               /* current 16-word chunk        */
		unsigned int  i;                   /* counter                      */
		dword         nbytes;              /* # of bytes not yet processed */

		/* initialize */
		MDinit(MDbuf, key);

		/* process message in 16-word chunks */
		for (nbytes=length; nbytes > 63; nbytes-=64) {
			for (i=0; i<16; i++) {
				X[i] = BYTES_TO_DWORD(message);
				message += 4;
			}
			compress(MDbuf, X);
		}                                    /* length mod 64 bytes left */

		MDfinish(MDbuf, message, length, 0);

		for (i=0; i<RMDsize/8; i+=4) {
			hashcode[i]   =  MDbuf[i>>2];         /* implicit cast to byte  */
			hashcode[i+1] = (MDbuf[i>>2] >>  8);  /*  extracts the 8 least  */
			hashcode[i+2] = (MDbuf[i>>2] >> 16);  /*  significant bits.     */
			hashcode[i+3] = (MDbuf[i>>2] >> 24);
		}

		return (byte *)hashcode;
	}
public:
	std::string sum(const std::string& data)
	{
		char* rv = (char*)RMD((byte*)data.data(), data.length(), NULL);
		return std::string(rv, RMDsize / 8);
	}

	std::string sumIV(unsigned int* IV, const char* HexMap, const std::string &sdata)
	{
		return "";
	}

	RIProv(Module* m) : HashProvider(m, "hash/ripemd160", 20, 64) {}
};

class ModuleRIPEMD160 : public Module
{
 public:
	RIProv mr;
	ModuleRIPEMD160() : mr(this)
	{
		ServerInstance->Modules->AddService(mr);
	}

	Version GetVersion()
	{
		return Version("Provides RIPEMD-160 hashing", VF_VENDOR);
	}

};

MODULE_INIT(ModuleRIPEMD160)

