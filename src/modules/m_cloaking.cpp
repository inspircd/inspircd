/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

// Hostname cloaking (+x mode) module for inspircd.
// version 1.0.0.1 by brain (C. J. Edwards) Mar 2004.
//
// When loaded this module will automatically set the
// +x mode on all connecting clients.
//
// Setting +x on a client causes the module to change the
// dhost entry (displayed host) for each user who has the
// mode, cloaking their host. Unlike unreal, the algorithm
// is non-reversible as uncloaked hosts are passed along
// the server->server link, and all encoding of hosts is
// done locally on the server by this module.

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides masking of user hostnames */


/* The four core functions - F1 is optimized somewhat */

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))
#define MD5STEP(f,w,x,y,z,in,s) (w += f(x,y,z) + in, w = (w<<s | w>>(32-s)) + x)

typedef uint32_t word32; /* NOT unsigned long. We don't support 16 bit platforms, anyway. */
typedef unsigned char byte;

struct xMD5Context {
	word32 buf[4];
	word32 bytes[2];
	word32 in[16];
};

class ModuleCloaking : public Module
{
 private:

	Server *Srv;
	std::string prefix;
	word32 key1;
	word32 key2;
	word32 key3;
	word32 key4;

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

	void xMD5Init(struct xMD5Context *ctx)
	{
		ctx->buf[0] = key1;
		ctx->buf[1] = key2;
		ctx->buf[2] = key3;
		ctx->buf[3] = key4;

		ctx->bytes[0] = 0;
		ctx->bytes[1] = 0;
	}

	void xMD5Update(struct xMD5Context *ctx, byte const *buf, int len)
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
		xMD5Transform(ctx->buf, ctx->in);
		buf += (unsigned)t;
		len -= (unsigned)t;

		/* Process data in 64-byte chunks */
		while (len >= 64)
		{
			memcpy(ctx->in, buf, 64);
			byteSwap(ctx->in, 16);
			xMD5Transform(ctx->buf, ctx->in);
			buf += 64;
			len -= 64;
		}

		/* Handle any remaining bytes of data. */
		memcpy(ctx->in, buf, len);
	}

	void xMD5Final(byte digest[16], struct xMD5Context *ctx)
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
			xMD5Transform(ctx->buf, ctx->in);
			p = (byte *)ctx->in;
			count = 56;
		}
		memset(p, 0, count+8);
		byteSwap(ctx->in, 14);

		/* Append length in bits and transform */
		ctx->in[14] = ctx->bytes[0] << 3;
		ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
		xMD5Transform(ctx->buf, ctx->in);

		byteSwap(ctx->buf, 4);
		memcpy(digest, ctx->buf, 16);
		memset(ctx, 0, sizeof(ctx));
	}

	void xMD5Transform(word32 buf[4], word32 const in[16])
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
		struct xMD5Context context;
	
		xMD5Init(&context);
		xMD5Update(&context, (const unsigned char*)orig, len);
		xMD5Final((unsigned char*)dest, &context);
	}


	void GenHash(char* src, char* dest)
	{
		// purposefully lossy md5 - only gives them the most significant 4 bits
		// of every md5 output byte.
		int i = 0;
		unsigned char bytes[16];
		char hash[MAXBUF];
		*hash = 0;
		MyMD5((char*)bytes,src,strlen(src));
		for (i = 0; i < 16; i++)
		{
			const char* xtab = "F92E45D871BCA630";
			unsigned char hi = xtab[bytes[i] / 16];
			char hx[2];
			hx[0] = hi;
			hx[1] = '\0';
			strlcat(hash,hx,MAXBUF);
		}
		strlcpy(dest,hash,MAXBUF);
	}
	 
 public:
	ModuleCloaking(Server* Me)
		: Module::Module(Me)
	{
		// We must take a copy of the Server class to work with
		Srv = Me;
		
		// we must create a new mode. Set the parameters so the
		// mode doesn't require oper, and is a client usermode
  		// with no parameters (actually, you cant have params for a umode!)
		Srv->AddExtendedMode('x',MT_CLIENT,false,0,0);

		OnRehash("");
	}
	
	virtual ~ModuleCloaking()
	{
	}
	
	virtual Version GetVersion()
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version(1,0,0,1,VF_STATIC|VF_VENDOR);
	}

	virtual void OnRehash(const std::string &parameter)
	{
		ConfigReader* Conf = new ConfigReader();
		key1 = key2 = key3 = key4 = 0;
		key1 = Conf->ReadInteger("cloak","key1",0,false);
		key2 = Conf->ReadInteger("cloak","key2",0,false);
		key3 = Conf->ReadInteger("cloak","key3",0,false);
		key4 = Conf->ReadInteger("cloak","key4",0,false);
		prefix = Conf->ReadValue("cloak","prefix",0);
		if (prefix == "")
		{
			prefix = Srv->GetNetworkName();
		}
		if (!key1 && !key2 && !key3 && !key4)
		{
			ModuleException ex("You have not defined cloak keys for m_cloaking!!! THIS IS INSECURE AND SHOULD BE CHECKED!");
			throw (ex);
		}

		/*ctx->buf[0] = 0x67452301;
		ctx->buf[1] = 0xefcdab89;
		ctx->buf[2] = 0x98badcfe;
		ctx->buf[3] = 0x10325476;*/
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnExtendedMode] = List[I_OnUserConnect] = 1;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// this method is called for any extended mode character.
		// all module modes for all modules pass through here
		// (unless another module further up the chain claims them)
		// so we must be VERY careful to only act upon modes which
		// we have claimed ourselves. This is a feature to allow
		// modules to 'spy' on extended mode activity if they so wish.
		if ((modechar == 'x') && (type == MT_CLIENT))
  		{
  			// OnExtendedMode gives us a void* as the target, we must cast
  			// it into a userrec* or a chanrec* depending on the value of
  			// the 'type' parameter (MT_CLIENT or MT_CHANNEL)
  			userrec* dest = (userrec*)target;
  			
			// we've now determined that this is our mode character...
			// is the user adding the mode to their list or removing it?
			if (mode_on)
			{
				// the mode is being turned on - so attempt to
				// allocate the user a cloaked host using a non-reversible
				// algorithm (its simple, but its non-reversible so the
				// simplicity doesnt really matter). This algorithm
				// will not work if the user has only one level of domain
				// naming in their hostname (e.g. if they are on a lan or
				// are connecting via localhost) -- this doesnt matter much.
				if (strchr(dest->host,'.'))
				{
					// in inspircd users have two hostnames. A displayed
					// hostname which can be modified by modules (e.g.
					// to create vhosts, implement chghost, etc) and a
					// 'real' hostname which you shouldnt write to.
					std::string a = strstr(dest->host,".");
					char ra[64];
					this->GenHash(dest->host,ra);
					std::string b = "";
					in_addr testaddr;
					std::string hostcloak = prefix + "-" + std::string(ra) + a;
					/* Fix by brain - if the cloaked host is > the max length of a host (64 bytes
					 * according to the DNS RFC) then tough titty, they get cloaked as an IP. 
					 * Their ISP shouldnt go to town on subdomains, or they shouldnt have a kiddie
					 * vhost.
					 */
					if ((!inet_aton(dest->host,&testaddr)) && (hostcloak.length() < 64))
					{
						// if they have a hostname, make something appropriate
						b = hostcloak;
					}
					else
					{
						// else, they have an ip
						b = std::string(ra) + "." + prefix + ".cloak";
					}
					Srv->Log(DEBUG,"cloak: allocated "+b);
					Srv->ChangeHost(dest,b);
				}
			}
			else
  			{
  				// user is removing the mode, so just restore their real host
  				// and make it match the displayed one.
				Srv->ChangeHost(dest,dest->host);
			}
			// this mode IS ours, and we have handled it. If we chose not to handle it,
			// for example the user cannot cloak as they have a vhost or such, then
			// we could return 0 here instead of 1 and the core would not send the mode
			// change to the user.
			return 1;
		}
		else
		{
			// this mode isn't ours, we have to bail and return 0 to not handle it.
			return 0;
		}
	}

	virtual void OnUserConnect(userrec* user)
	{
		// Heres the weird bit. When a user connects we must set +x on them, so
		// we're going to use the SendMode method of the Server class to send
		// the mode to the client. This is basically the same as sending an
		// SAMODE in unreal. Note that to the user it will appear as if they set
		// the mode on themselves.
		
		char* modes[2];			// only two parameters
		modes[0] = user->nick;		// first parameter is the nick
		modes[1] = "+x";		// second parameter is the mode
		Srv->SendMode(modes,2,user);	// send these, forming the command "MODE <nick> +x"
	}

};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleCloakingFactory : public ModuleFactory
{
 public:
	ModuleCloakingFactory()
	{
	}
	
	~ModuleCloakingFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleCloaking(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleCloakingFactory;
}

