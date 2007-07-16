/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <zlib.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "socket.h"
#include "hashcomp.h"
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

/* Status of a connection */
enum izip_status { IZIP_OPEN, IZIP_CLOSED };

/* Maximum transfer size per read operation */
const unsigned int CHUNK = 128 * 1024;

/* This class manages a compressed chunk of data preceeded by
 * a length count.
 *
 * It can handle having multiple chunks of data in the buffer
 * at any time.
 */
class CountedBuffer : public classbase
{
	std::string buffer;		/* Current buffer contents */
	unsigned int amount_expected;	/* Amount of data expected */
 public:
	CountedBuffer()
	{
		amount_expected = 0;
	}

	/** Adds arbitrary compressed data to the buffer.
	 * - Binsry safe, of course.
	 */
	void AddData(unsigned char* data, int data_length)
	{
		buffer.append((const char*)data, data_length);
		this->NextFrameSize();
	}

	/** Works out the size of the next compressed frame
	 */
	void NextFrameSize()
	{
		if ((!amount_expected) && (buffer.length() >= 4))
		{
			/* We have enough to read an int -
			 * Yes, this is safe, but its ugly. Give me
			 * a nicer way to read 4 bytes from a binary
			 * stream, and push them into a 32 bit int,
			 * and i'll consider replacing this.
			 */
			amount_expected = ntohl((buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0]);
			buffer = buffer.substr(4);
		}
	}

	/** Gets the next frame and returns its size, or returns
	 * zero if there isnt one available yet.
	 * A frame can contain multiple plaintext lines.
	 * - Binary safe.
	 */
	int GetFrame(unsigned char* frame, int maxsize)
	{
		if (amount_expected)
		{
			/* We know how much we're expecting...
			 * Do we have enough yet?
			 */
			if (buffer.length() >= amount_expected)
			{
				int j = 0;
				for (unsigned int i = 0; i < amount_expected; i++, j++)
					frame[i] = buffer[i];

				buffer = buffer.substr(j);
				amount_expected = 0;
				NextFrameSize();
				return j;
			}
		}
		/* Not enough for a frame yet, COME AGAIN! */
		return 0;
	}
};

/** Represents an zipped connections extra data
 */
class izip_session : public classbase
{
 public:
	z_stream c_stream;	/* compression stream */
	z_stream d_stream;	/* decompress stream */
	izip_status status;	/* Connection status */
	int fd;			/* File descriptor */
	CountedBuffer* inbuf;	/* Holds input buffer */
	std::string outbuf;	/* Holds output buffer */
};

class ModuleZLib : public Module
{
	izip_session sessions[MAX_DESCRIPTORS];

	/* Used for stats z extensions */
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
		ServerInstance->UnpublishInterface("InspSocketHook", this);
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

	/* Handle InspSocketHook API requests */
	virtual char* OnRequest(Request* request)
	{
		ISHRequest* ISR = (ISHRequest*)request;
		if (strcmp("IS_NAME", request->GetId()) == 0)
		{
			/* Return name */
			return "zip";
		}
		else if (strcmp("IS_HOOK", request->GetId()) == 0)
		{
			/* Attach to an inspsocket */
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
			/* Detatch from an inspsocket */
			return ServerInstance->Config->DelIOHook((InspSocket*)ISR->Sock) ? (char*)"OK" : NULL;
		}
		else if (strcmp("IS_HSDONE", request->GetId()) == 0)
		{
			/* Check for completion of handshake
			 * (actually, this module doesnt handshake)
			 */
			return "OK";
		}
		else if (strcmp("IS_ATTACH", request->GetId()) == 0)
		{
			/* Attach certificate data to the inspsocket
			 * (this module doesnt do that, either)
			 */
			return NULL;
		}
		return NULL;
	}

	/* Handle stats z (misc stats) */
	virtual int OnStats(char symbol, userrec* user, string_list &results)
	{
		if (symbol == 'z')
		{
			std::string sn = ServerInstance->Config->ServerName;

			/* Yeah yeah, i know, floats are ew.
			 * We used them here because we'd be casting to float anyway to do this maths,
			 * and also only floating point numbers can deal with the pretty large numbers
			 * involved in the total throughput of a server over a large period of time.
			 * (we dont count 64 bit ints because not all systems have 64 bit ints, and floats
			 * can still hold more.
			 */
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
	
		/* allocate state and buffers */
		session->fd = fd;
		session->status = IZIP_OPEN;
		session->inbuf = new CountedBuffer();

		session->c_stream.zalloc = (alloc_func)0;
		session->c_stream.zfree = (free_func)0;
		session->c_stream.opaque = (voidpf)0;

		session->d_stream.zalloc = (alloc_func)0;
		session->d_stream.zfree = (free_func)0;
		session->d_stream.opaque = (voidpf)0;
	}

	virtual void OnRawSocketConnect(int fd)
	{
		/* Nothing special needs doing here compared to accept() */
		OnRawSocketAccept(fd, "", 0);
	}

	virtual void OnRawSocketClose(int fd)
	{
		CloseSession(&sessions[fd]);
	}

	virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult)
	{
		/* Find the sockets session */
		izip_session* session = &sessions[fd];

		if (session->status == IZIP_CLOSED)
			return 0;

		unsigned char compr[CHUNK + 4];
		unsigned int offset = 0;
		unsigned int total_size = 0;

		/* Read CHUNK bytes at a time to the buffer (usually 128k) */
		readresult = read(fd, compr, CHUNK);

		/* Did we get anything? */
		if (readresult > 0)
		{
			/* Add it to the frame queue */
			session->inbuf->AddData(compr, readresult);
			total_in_compressed += readresult;
	
			/* Parse all completed frames */
			int size = 0;
			while ((size = session->inbuf->GetFrame(compr, CHUNK)) != 0)
			{
				session->d_stream.next_in  = (Bytef*)compr;
				session->d_stream.avail_in = 0;
				session->d_stream.next_out = (Bytef*)(buffer + offset);

				/* If we cant call this, well, we're boned. */
				if (inflateInit(&session->d_stream) != Z_OK)
					return 0;
	
				while ((session->d_stream.total_out < count) && (session->d_stream.total_in < (unsigned int)size))
				{
					session->d_stream.avail_in = session->d_stream.avail_out = 1;
					if (inflate(&session->d_stream, Z_NO_FLUSH) == Z_STREAM_END)
						break;
				}
	
				/* Stick a fork in me, i'm done */
				inflateEnd(&session->d_stream);

				/* Update counters and offsets */
				total_size += session->d_stream.total_out;
				total_in_uncompressed += session->d_stream.total_out;
				offset += session->d_stream.total_out;
			}

			/* Null-terminate the buffer -- this doesnt harm binary data */
			buffer[total_size] = 0;

			/* Set the read size to the correct total size */
			readresult = total_size;

		}
		return (readresult > 0);
	}

	virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
	{
		izip_session* session = &sessions[fd];
		int ocount = count;

		if (!count)	/* Nothing to do! */
			return 0;

		if(session->status != IZIP_OPEN)
		{
			/* Seriously, wtf? */
			CloseSession(session);
			return 0;
		}

		unsigned char compr[CHUNK + 4];

		/* Gentlemen, start your engines! */
		if (deflateInit(&session->c_stream, Z_BEST_COMPRESSION) != Z_OK)
		{
			CloseSession(session);
			return 0;
		}

		/* Set buffer sizes (we reserve 4 bytes at the start of the
		 * buffer for the length counters)
		 */
		session->c_stream.next_in  = (Bytef*)buffer;
		session->c_stream.next_out = compr + 4;

		/* Compress the text */
		while ((session->c_stream.total_in < (unsigned int)count) && (session->c_stream.total_out < CHUNK))
		{
			session->c_stream.avail_in = session->c_stream.avail_out = 1;
			if (deflate(&session->c_stream, Z_NO_FLUSH) != Z_OK)
			{
				CloseSession(session);
				return 0;
			}
		}
		/* Finish the stream */
		for (session->c_stream.avail_out = 1; deflate(&session->c_stream, Z_FINISH) != Z_STREAM_END; session->c_stream.avail_out = 1);
		deflateEnd(&session->c_stream);

		total_out_uncompressed += ocount;
		total_out_compressed += session->c_stream.total_out;

		/** Assemble the frame length onto the frame, in network byte order */
		compr[0] = (session->c_stream.total_out >> 24);
		compr[1] = (session->c_stream.total_out >> 16);
		compr[2] = (session->c_stream.total_out >> 8);
		compr[3] = (session->c_stream.total_out & 0xFF);

		/* Add compressed data plus leading length to the output buffer -
		 * Note, we may have incomplete half-sent frames in here.
		 */
		session->outbuf.append((const char*)compr, session->c_stream.total_out + 4);

		/* Lets see how much we can send out */
		int ret = write(fd, session->outbuf.data(), session->outbuf.length());

		/* Check for errors, and advance the buffer if any was sent */
		if (ret > 0)
			session->outbuf = session->outbuf.substr(ret);
		else if (ret < 1)
		{
			if (ret == -1)
			{
				if (errno == EAGAIN)
					return 0;
				else
				{
					session->outbuf.clear();
					return 0;
				}
			}
			else
			{
				session->outbuf.clear();
				return 0;
			}
		}

		/* ALL LIES the lot of it, we havent really written
		 * this amount, but the layer above doesnt need to know.
		 */
		return ocount;
	}
	
	void CloseSession(izip_session* session)
	{
		if (session->status == IZIP_OPEN)
		{
			session->status = IZIP_CLOSED;
			session->outbuf.clear();
			delete session->inbuf;
		}
	}

};

MODULE_INIT(ModuleZLib);

