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

enum izip_status { IZIP_OPEN, IZIP_CLOSED };

const unsigned int CHUNK = 128 * 1024;

class CountedBuffer : public classbase
{
	std::deque<unsigned char> buffer; /* Current buffer contents */
	unsigned int amount_expected;   /* Amount of data expected */
 public:
	CountedBuffer()
	{
		amount_expected = 0;
	}

	void AddData(unsigned char* data, int data_length)
	{
		for (int i = 0; i < data_length; i++)
			buffer.push_back(data[i]);

		this->NextFrameSize();
	}

	void NextFrameSize()
	{
		if ((!amount_expected) && (buffer.size() >= 4))
		{
			/* We have enough to read an int */
			char sz[4];
			for (int i = 0; i < 4; i++)
			{
				sz[i] = buffer.front();
				buffer.pop_front();
			}
			int* size = (int*)sz;
			amount_expected = ntohl(*size);
		}
	}

	int GetFrame(unsigned char* frame, int maxsize)
	{
		if (amount_expected)
		{
			/* We know how much we're expecting...
			 * Do we have enough yet?
			 */
			if (buffer.size() >= amount_expected)
			{
				int j = 0;
				for (unsigned int i = 0; i < amount_expected; i++, j++)
				{
					frame[i] = buffer.front();
					buffer.pop_front();
				}

				amount_expected = 0;
				NextFrameSize();

				return j;
			}
		}
		/* Not enough for a frame yet, COME AGAIN! */
		return 0;
	}
};

/** Represents an ZIP user's extra data
 */
class izip_session : public classbase
{
 public:
	z_stream c_stream; /* compression stream */
	z_stream d_stream; /* decompress stream */
	izip_status status;
	int fd;
	CountedBuffer* inbuf;
	std::string outbuf;
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
			char* ret = "OK";
			try
			{
				ret = ServerInstance->Config->AddIOHook((Module*)this, (InspSocket*)ISR->Sock) ? (char*)"OK" : NULL;
			}
			catch (ModuleException& e)
			{
				return NULL;
			}
			return ret;
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
		if (symbol == 'z')
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

			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS outbound_compressed   = "+ConvToStr(total_out_compressed));
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS inbound_compressed    = "+ConvToStr(total_in_compressed));
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS outbound_uncompressed = "+ConvToStr(total_out_uncompressed));
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS inbound_uncompressed  = "+ConvToStr(total_in_uncompressed));
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS outbound_ratio        = "+outbound_ratio);
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS inbound_ratio         = "+inbound_ratio);
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS combined_ratio        = "+combined_ratio);
			return 0;
		}

		return 0;
	}

	virtual void OnRawSocketAccept(int fd, const std::string &ip, int localport)
	{
		izip_session* session = &sessions[fd];
	
		/* allocate deflate state */
		session->fd = fd;
		session->status = IZIP_OPEN;

		session->inbuf = new CountedBuffer();
		ServerInstance->Log(DEBUG,"session->inbuf ALLOC = %d, %08x", fd, session->inbuf);

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

		unsigned char compr[CHUNK + 1];

		readresult = read(fd, compr, CHUNK);

		if (readresult > 0)
		{
			session->inbuf->AddData(compr, readresult);
	
			int size = 0;
			std::string str_out;
			while ((size = session->inbuf->GetFrame(compr, CHUNK)) != 0)
			{
				unsigned char localbuf[count + 1];

				session->d_stream.next_in  = (Bytef*)compr;
				session->d_stream.avail_in = 0;
				session->d_stream.next_out = (Bytef*)localbuf;
				if (inflateInit(&session->d_stream) != Z_OK)
					return -EBADF;
	
				while ((session->d_stream.total_out < count) && (session->d_stream.total_in < (unsigned int)size))
				{
					session->d_stream.avail_in = session->d_stream.avail_out = 1;
					if (inflate(&session->d_stream, Z_NO_FLUSH) == Z_STREAM_END)
						break;
				}
	
				inflateEnd(&session->d_stream);

				localbuf[session->d_stream.total_out] = 0;
				str_out.append((const char*)localbuf);
				total_in_compressed += readresult;
				readresult = session->d_stream.total_out;
				total_in_uncompressed += session->d_stream.total_out;
			}

			memcpy(buffer, str_out.data(), str_out.length() > count ? count : str_out.length());
			readresult = str_out.length();
		}
		return (readresult > 0);
	}

	virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
	{
		ServerInstance->Log(DEBUG,"Compressing %d bytes", count);

		izip_session* session = &sessions[fd];
		int ocount = count;

		if (!count)
		{
			ServerInstance->Log(DEBUG,"Nothing to do!");
			return 1;
		}

		if(session->status != IZIP_OPEN)
		{
			CloseSession(session);
			return 0;
		}

		unsigned char compr[CHUNK];

		if (deflateInit(&session->c_stream, Z_BEST_COMPRESSION) != Z_OK)
		{
			ServerInstance->Log(DEBUG,"Deflate init failed");
		}

		session->c_stream.next_in  = (Bytef*)buffer;
		session->c_stream.next_out = compr+4;

		while ((session->c_stream.total_in < (unsigned int)count) && (session->c_stream.total_out < CHUNK))
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

		const char* string_likes_signed_chars = (const char*)compr;
		session->outbuf.append(string_likes_signed_chars, session->c_stream.total_out+4);

		int ret = write(fd, session->outbuf.data(), session->outbuf.length());

		ServerInstance->Log(DEBUG,"Sending frame of size %d", ntohl(x));

		session->outbuf = session->outbuf.substr(ret);

		return ocount;
	}
	
	void CloseSession(izip_session* session)
	{
		if (session->status = IZIP_OPEN)
		{
			session->status = IZIP_CLOSED;
			session->outbuf = "";
			delete session->inbuf;
		}
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
