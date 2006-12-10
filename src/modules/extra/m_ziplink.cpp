#include <string>
#include <vector>

#include "zlib.h"

#include "inspircd_config.h"
#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "socket.h"
#include "hashcomp.h"
#include "inspircd.h"

#include "transport.h"

/* $ModDesc: Provides zlib link support for servers */
/* $LinkerFlags: -lz */
/* $ModDep: transport.h */

/*
 * Compressed data is transmitted across the link in the following format:
 *
 *   0   1   2   3   4 ... n
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * |       n       |              Z0 -> Zn                         |
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *
 * Where: n is the size of a frame, in network byte order, 4 bytes.
 * Z0 through Zn are Zlib compressed data, n bytes in length.
 *
 * If the module fails to read the entire frame, then it will buffer
 * the portion of the last frame it received, then attempt to read
 * the next part of the frame next time a write notification arrives.
 *
 * ZLIB_BEST_COMPRESSION (9) is used for all sending of data with
 * a flush after each frame. A frame may contain multiple lines
 * and should be treated as raw binary data.
 *
 */


enum izip_status { IZIP_WAITFIRST, IZIP_OPEN, IZIP_CLOSED };

const unsigned int CHUNK = 16384;

/** Represents an ZIP user's extra data
 */
class izip_session : public classbase
{
 public:
	z_stream c_stream; /* compression stream */
	z_stream d_stream; /* decompress stream */
	izip_status status;
	int need_bytes;
	int fd;
	std::string inbuf;
};

class ModuleZLib : public Module
{
	izip_session sessions[MAX_DESCRIPTORS];
	float total_out_compressed;
	float total_in_compressed;
	float total_out_uncompressed;
	float total_in_uncompressed;
	
 public:
	
	ModuleZLib(InspIRCd* Me)
		: Module::Module(Me)
	{
		ServerInstance->PublishInterface("InspSocketHook", this);

		total_out_compressed = total_in_compressed = 0;
		total_out_uncompressed = total_out_uncompressed = 0;
	}

	virtual ~ModuleZLib()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnRawSocketConnect] = List[I_OnRawSocketAccept] = List[I_OnRawSocketClose] = List[I_OnRawSocketRead] = List[I_OnRawSocketWrite] = 1;
		List[I_OnStats] = List[I_OnRequest] = 1;
	}

        virtual char* OnRequest(Request* request)
	{
		ISHRequest* ISR = (ISHRequest*)request;
		if (strcmp("IS_NAME", request->GetId()) == 0)
		{
			return "zip";
		}
		else if (strcmp("IS_HOOK", request->GetId()) == 0)
		{
			return ServerInstance->Config->AddIOHook((Module*)this, (InspSocket*)ISR->Sock) ? (char*)"OK" : NULL;
		}
		else if (strcmp("IS_UNHOOK", request->GetId()) == 0)
		{
			return ServerInstance->Config->DelIOHook((InspSocket*)ISR->Sock) ? (char*)"OK" : NULL;
		}
		else if (strcmp("IS_HSDONE", request->GetId()) == 0)
		{
			return "OK";
		}
		else if (strcmp("IS_ATTACH", request->GetId()) == 0)
		{
			return NULL;
		}
		return NULL;
	}

	virtual int OnStats(char symbol, userrec* user, string_list &results)
	{
		if (symbol == 'C')
		{
			std::string sn = ServerInstance->Config->ServerName;

			float outbound_r = 100 - ((total_out_compressed / (total_out_uncompressed + 0.001)) * 100);
			float inbound_r = 100 - ((total_in_compressed / (total_in_uncompressed + 0.001)) * 100);

			float total_compressed = total_in_compressed + total_out_compressed;
			float total_uncompressed = total_in_uncompressed + total_out_uncompressed;

			float total_r = 100 - ((total_compressed / (total_uncompressed + 0.001)) * 100);

			char outbound_ratio[MAXBUF], inbound_ratio[MAXBUF], combined_ratio[MAXBUF];

			sprintf(outbound_ratio, "%3.2f%%", outbound_r);
			sprintf(inbound_ratio, "%3.2f%%", inbound_r);
			sprintf(combined_ratio, "%3.2f%%", total_r);

			results.push_back(sn+" 304 "+user->nick+" : ZIPSTATS outbound_compressed   = "+ConvToStr(total_out_compressed));
			results.push_back(sn+" 304 "+user->nick+" : ZIPSTATS inbound_compressed    = "+ConvToStr(total_in_compressed));
			results.push_back(sn+" 304 "+user->nick+" : ZIPSTATS outbound_uncompressed = "+ConvToStr(total_out_uncompressed));
			results.push_back(sn+" 304 "+user->nick+" : ZIPSTATS inbound_uncompressed  = "+ConvToStr(total_in_uncompressed));
			results.push_back(sn+" 304 "+user->nick+" : ZIPSTATS ----------------------------");
			results.push_back(sn+" 304 "+user->nick+" : ZIPSTATS OUTBOUND RATIO        = "+outbound_ratio);
			results.push_back(sn+" 304 "+user->nick+" : ZIPSTATS INBOUND RATIO         = "+inbound_ratio);
			results.push_back(sn+" 304 "+user->nick+" : ZIPSTATS COMBINED RATIO        = "+combined_ratio);
			return 0;
		}

		return 0;
	}

	virtual void OnRawSocketAccept(int fd, const std::string &ip, int localport)
	{
		izip_session* session = &sessions[fd];
	
		/* allocate deflate state */
		session->fd = fd;
		session->status = IZIP_WAITFIRST;

		session->need_bytes = 0;

		session->c_stream.zalloc = (alloc_func)0;
		session->c_stream.zfree = (free_func)0;
		session->c_stream.opaque = (voidpf)0;

		session->d_stream.zalloc = (alloc_func)0;
		session->d_stream.zfree = (free_func)0;
		session->d_stream.opaque = (voidpf)0;

	}

	virtual void OnRawSocketConnect(int fd)
	{
		OnRawSocketAccept(fd, "", 0);
	}

	virtual void OnRawSocketClose(int fd)
	{
		CloseSession(&sessions[fd]);
	}
	
	virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult)
	{
		izip_session* session = &sessions[fd];

		if (session->status == IZIP_CLOSED)
			return 1;

		int size = 0;

		if (session->need_bytes)
		{
			size = session->need_bytes;
		}
		else
		{
			if (read(fd, &size, sizeof(size)) != sizeof(size))
				return 0;
			size = ntohl(size);
		}

		ServerInstance->Log(DEBUG,"Size of frame to read: %d%s", size, session->need_bytes ? " (remainder of last frame)" : "");

		unsigned char compr[size+1+session->need_bytes];

		readresult = read(fd, compr + session->need_bytes, size);

		if (readresult == size)
		{
			if(session->status == IZIP_WAITFIRST)
			{
				session->status = IZIP_OPEN;
			}

			/* Reassemble first part of last frame */
			if (session->need_bytes)
			{
				for (size_t i = 0; i < session->inbuf.length(); i++)
					compr[i] = session->inbuf[i];
			}

			session->d_stream.next_in  = (Bytef*)compr;
			session->d_stream.avail_in = 0;
			session->d_stream.next_out = (Bytef*)buffer;
			if (inflateInit(&session->d_stream) != Z_OK)
				return -EBADF;
			session->status = IZIP_OPEN;

			while ((session->d_stream.total_out < count) && (session->d_stream.total_in < (unsigned int)readresult))
			{
				session->d_stream.avail_in = session->d_stream.avail_out = 1; /* force small buffers */
				if (inflate(&session->d_stream, Z_NO_FLUSH) == Z_STREAM_END)
					break;
			}

			inflateEnd(&session->d_stream);

			total_in_compressed += readresult;
			readresult = session->d_stream.total_out;
			total_in_uncompressed += session->d_stream.total_out;

			buffer[readresult] = 0;
			session->need_bytes = 0;
		}
		else
		{
			/* We need to buffer here */
			ServerInstance->Log(DEBUG,"Didnt read whole frame, got %d bytes of %d!", readresult, size);
			session->need_bytes = ((readresult > -1) ? (size - readresult) : (size));
			if (readresult > 0)
			{
				/* Do it this way because it needs to be binary safe */
				for (int i = 0; i < readresult; i++)
					session->inbuf += compr[i];
			}
		}

		return (readresult > 0);
	}

	virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
	{
		int ocount = count;
		if (!count)
		{
			ServerInstance->Log(DEBUG,"Nothing to do!");
			return 1;
		}

		unsigned char compr[count*2+4];

		izip_session* session = &sessions[fd];

		if(session->status == IZIP_WAITFIRST)
		{
			session->status = IZIP_OPEN;
		}

		if (deflateInit(&session->c_stream, Z_BEST_COMPRESSION) != Z_OK)
		{
			ServerInstance->Log(DEBUG,"Deflate init failed");
		}

		if(session->status != IZIP_OPEN)
		{
			ServerInstance->Log(DEBUG,"State not open!");
			CloseSession(session);
			return 0;
		}

		session->c_stream.next_in  = (Bytef*)buffer;
		session->c_stream.next_out = compr+4;

		while ((session->c_stream.total_in < (unsigned int)count) && (session->c_stream.total_out < (unsigned int)count*2))
		{
			session->c_stream.avail_in = session->c_stream.avail_out = 1; /* force small buffers */
			if (deflate(&session->c_stream, Z_NO_FLUSH) != Z_OK)
			{
				ServerInstance->Log(DEBUG,"Couldnt deflate!");
				CloseSession(session);
				return 0;
			}
		}
	        /* Finish the stream, still forcing small buffers: */
		for (;;)
		{
			session->c_stream.avail_out = 1;
			if (deflate(&session->c_stream, Z_FINISH) == Z_STREAM_END)
				break;
		}

		deflateEnd(&session->c_stream);

		total_out_uncompressed += ocount;
		total_out_compressed += session->c_stream.total_out;

		int x = htonl(session->c_stream.total_out);
		/** XXX: We memcpy it onto the start of the buffer like this to save ourselves a write().
		 * A memcpy of 4 or so bytes is less expensive and gives the tcp stack more chance of
		 * assembling the frame size into the same packet as the compressed frame.
		 */
		memcpy(compr, &x, sizeof(x));
		write(fd, compr, session->c_stream.total_out+4);

		return ocount;
	}
	
	void CloseSession(izip_session* session)
	{
		session->status = IZIP_CLOSED;
	}

};

class ModuleZLibFactory : public ModuleFactory
{
 public:
	ModuleZLibFactory()
	{
	}
	
	~ModuleZLibFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleZLib(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleZLibFactory;
}
