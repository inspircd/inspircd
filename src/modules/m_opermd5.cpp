/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Allows for MD5 encrypted oper passwords */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"


/* The four core functions - F1 is optimized somewhat */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f,w,x,y,z,in,s) \
         (w += f(x,y,z) + in, w = (w<<s | w>>(32-s)) + x)

typedef unsigned long word32;
typedef unsigned char byte;

struct MD5Context {
	word32 buf[4];
	word32 bytes[2];
	word32 in[16];
};

Server *Srv;

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, byte const *buf, int len);
void MD5Final(byte digest[16], struct MD5Context *context);
void MD5Transform(word32 buf[4], word32 const in[16]);
void MyMD5(void *dest, void *orig, int len);
void GenHash(const char* src, char* dest);

void byteSwap(word32 *buf, unsigned words)
{
	byte *p = (byte *)buf;

	do
	{
		*buf++ = (word32)((unsigned)p[3] << 8 | p[2]) << 16 |
			((unsigned)p[1] << 8 | p[0]);
		p += 4;
	} while (--words);
}

/* XXX - maybe if we had an md5/encryption moduletype? *shrug* */
void MD5Init(struct MD5Context *ctx)
{
	ctx->buf[0] = 0x67452301;
	ctx->buf[1] = 0xefcdab89;
	ctx->buf[2] = 0x98badcfe;
	ctx->buf[3] = 0x10325476;

	ctx->bytes[0] = 0;
	ctx->bytes[1] = 0;
}

void MD5Update(struct MD5Context *ctx, byte const *buf, int len)
{
	word32 t;

	/* Update byte count */

	t = ctx->bytes[0];
	if ((ctx->bytes[0] = t + len) < t)
		ctx->bytes[1]++;	/* Carry from low to high */

	t = 64 - (t & 0x3f);	/* Space available in ctx->in (at least 1) */
	if ((unsigned)t > (unsigned)len)
	{
		memcpy((byte *)ctx->in + 64 - (unsigned)t, buf, len);
		return;
	}
	/* First chunk is an odd size */
	memcpy((byte *)ctx->in + 64 - (unsigned)t, buf, (unsigned)t);
	byteSwap(ctx->in, 16);
	MD5Transform(ctx->buf, ctx->in);
	buf += (unsigned)t;
	len -= (unsigned)t;

	/* Process data in 64-byte chunks */
	while (len >= 64)
	{
		memcpy(ctx->in, buf, 64);
		byteSwap(ctx->in, 16);
		MD5Transform(ctx->buf, ctx->in);
		buf += 64;
		len -= 64;
	}

	/* Handle any remaining bytes of data. */
	memcpy(ctx->in, buf, len);
}

void MD5Final(byte digest[16], struct MD5Context *ctx)
{
	int count = (int)(ctx->bytes[0] & 0x3f); /* Bytes in ctx->in */
	byte *p = (byte *)ctx->in + count;	/* First unused byte */

	/* Set the first char of padding to 0x80.  There is always room. */
	*p++ = 0x80;

	/* Bytes of padding needed to make 56 bytes (-8..55) */
	count = 56 - 1 - count;

	if (count < 0)
	{	/* Padding forces an extra block */
		memset(p, 0, count+8);
		byteSwap(ctx->in, 16);
		MD5Transform(ctx->buf, ctx->in);
		p = (byte *)ctx->in;
		count = 56;
	}
	memset(p, 0, count+8);
	byteSwap(ctx->in, 14);

	/* Append length in bits and transform */
	ctx->in[14] = ctx->bytes[0] << 3;
	ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
	MD5Transform(ctx->buf, ctx->in);

	byteSwap(ctx->buf, 4);
	memcpy(digest, ctx->buf, 16);
	memset(ctx, 0, sizeof(ctx));
}

void MD5Transform(word32 buf[4], word32 const in[16])
{
	register word32 a, b, c, d;

	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];

	MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
	MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
	MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
	MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
	MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
	MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
	MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
	MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
	MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
	MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
	MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
	MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
	MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
	MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
	MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
	MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
	MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
	MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
	MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
	MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
	MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
	MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
	MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}


void MyMD5(void *dest, void *orig, int len)
{
	struct MD5Context context;

	MD5Init(&context);
	MD5Update(&context, (const unsigned char*)orig, len);
	MD5Final((unsigned char*)dest, &context);
}


void GenHash(const char* src, char* dest)
{
	int i = 0;
	unsigned char bytes[16];
	char hash[1024];
	*hash = 0;
	MyMD5((char*)bytes,(void*)src,strlen(src));
	for (i = 0; i < 16; i++)
	{
		const char* xtab = "0123456789abcdef";
		unsigned char lo = xtab[bytes[i] % 16];
		unsigned char hi = xtab[bytes[i] / 16];
		char hx[3];
		hx[0] = hi;
		hx[1] = lo;
		hx[2] = '\0';
		strcat(hash,hx);
	}
	strcpy(dest,hash);
}

class cmd_mkpasswd : public command_t
{
 public:
	cmd_mkpasswd () : command_t("MKPASSWD", 'o', 1)
	{
		this->source = "m_opermd5.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		char buffer[MAXBUF];
		GenHash(parameters[0],buffer);
		WriteServ(user->fd,"NOTICE %s :MD5 hashed password for %s is %s",user->nick,parameters[0],buffer);
	}
};

class ModuleOperMD5 : public Module
{
	cmd_mkpasswd* mycommand;
 public:

	ModuleOperMD5(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_mkpasswd();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleOperMD5()
	{
	}

	void Implements(char* List)
	{
		List[I_OnOperCompare] = 1;
	}

	virtual int OnOperCompare(std::string data, std::string input)
	{
		char buffer[MAXBUF];
		if (data.length() == 32)	// if its 32 chars long, try it as an md5
		{
			GenHash(input.c_str(),buffer);
			if (!strcasecmp(data.c_str(),buffer))
			{
				return 1;
			}
			else return -1;
		}
		return 0;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
};


class ModuleOperMD5Factory : public ModuleFactory
{
 public:
	ModuleOperMD5Factory()
	{
	}
	
	~ModuleOperMD5Factory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleOperMD5(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOperMD5Factory;
}
