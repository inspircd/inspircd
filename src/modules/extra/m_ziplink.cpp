/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <zlib.h>
#include "transport.h"
#include <iostream>

/* $ModDesc: Provides zlib link support for servers */
/* $LinkerFlags: -lz */
/* $ModDep: transport.h */

/*
 * ZLIB_BEST_COMPRESSION (9) is used for all sending of data with
 * a flush after each chunk. A frame may contain multiple lines
 * and should be treated as raw binary data.
 */

/* Status of a connection */
enum izip_status { IZIP_CLOSED = 0, IZIP_OPEN };

/** Represents an zipped connections extra data
 */
class izip_session : public classbase
{
 public:
	z_stream c_stream;	/* compression stream */
	z_stream d_stream;	/* uncompress stream */
	izip_status status;	/* Connection status */
	std::string outbuf;	/* Holds output buffer (compressed) */
	std::string inbuf;	/* Holds input buffer (compressed) */
};

class ModuleZLib : public Module
{
	izip_session* sessions;

	/* Used for stats z extensions */
	float total_out_compressed;
	float total_in_compressed;
	float total_out_uncompressed;
	float total_in_uncompressed;

	/* Used for reading data from the wire and compressing data to. */
	char *net_buffer;
	unsigned int net_buffer_size;
 public:

	ModuleZLib(InspIRCd* Me)
		: Module(Me)
	{
		ServerInstance->Modules->PublishInterface("BufferedSocketHook", this);

		sessions = new izip_session[ServerInstance->SE->GetMaxFds()];
		for (int i = 0; i < ServerInstance->SE->GetMaxFds(); i++)
			sessions[i].status = IZIP_CLOSED;

		total_out_compressed = total_in_compressed = 0;
		total_out_uncompressed = total_in_uncompressed = 0;
		Implementation eventlist[] = { I_OnRawSocketConnect, I_OnRawSocketAccept, I_OnRawSocketClose, I_OnRawSocketRead, I_OnRawSocketWrite, I_OnStats, I_OnRequest };
		ServerInstance->Modules->Attach(eventlist, this, 7);

		// Allocate a buffer which is used for reading and writing data
		net_buffer_size = ServerInstance->Config->NetBufferSize;
		net_buffer = new char[net_buffer_size];
	}

	virtual ~ModuleZLib()
	{
		ServerInstance->Modules->UnpublishInterface("BufferedSocketHook", this);
		delete[] sessions;
		delete[] net_buffer;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}


	/* Handle BufferedSocketHook API requests */
	virtual const char* OnRequest(Request* request)
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
			const char* ret = "OK";
			try
			{
				ret = ISR->Sock->AddIOHook((Module*)this) ? "OK" : NULL;
			}
			catch (ModuleException& e)
			{
				return NULL;
			}
			return ret;
		}
		else if (strcmp("IS_UNHOOK", request->GetId()) == 0)
		{
			/* Detach from an inspsocket */
			return ISR->Sock->DelIOHook() ? "OK" : NULL;
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
	virtual ModResult OnStats(char symbol, User* user, string_list &results)
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
			float outbound_r = (total_out_compressed / (total_out_uncompressed + 0.001)) * 100;
			float inbound_r = (total_in_compressed / (total_in_uncompressed + 0.001)) * 100;

			float total_compressed = total_in_compressed + total_out_compressed;
			float total_uncompressed = total_in_uncompressed + total_out_uncompressed;

			float total_r = (total_compressed / (total_uncompressed + 0.001)) * 100;

			char outbound_ratio[MAXBUF], inbound_ratio[MAXBUF], combined_ratio[MAXBUF];

			sprintf(outbound_ratio, "%3.2f%%", outbound_r);
			sprintf(inbound_ratio, "%3.2f%%", inbound_r);
			sprintf(combined_ratio, "%3.2f%%", total_r);

			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS outbound_compressed   = "+ConvToStr(total_out_compressed));
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS inbound_compressed    = "+ConvToStr(total_in_compressed));
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS outbound_uncompressed = "+ConvToStr(total_out_uncompressed));
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS inbound_uncompressed  = "+ConvToStr(total_in_uncompressed));
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS percentage_of_original_outbound_traffic        = "+outbound_ratio);
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS percentage_of_orignal_inbound_traffic         = "+inbound_ratio);
			results.push_back(sn+" 304 "+user->nick+" :ZIPSTATS total_size_of_original_traffic        = "+combined_ratio);
			return MOD_RES_PASSTHRU;
		}

		return MOD_RES_PASSTHRU;
	}

	virtual void OnRawSocketConnect(int fd)
	{
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds() - 1))
			return;

		izip_session* session = &sessions[fd];

		/* Just in case... */
		session->outbuf.clear();

		session->c_stream.zalloc = (alloc_func)0;
		session->c_stream.zfree = (free_func)0;
		session->c_stream.opaque = (voidpf)0;

		session->d_stream.zalloc = (alloc_func)0;
		session->d_stream.zfree = (free_func)0;
		session->d_stream.opaque = (voidpf)0;

		/* If we cant call this, well, we're boned. */
		if (inflateInit(&session->d_stream) != Z_OK)
		{
			session->status = IZIP_CLOSED;
			return;
		}

		/* Same here */
		if (deflateInit(&session->c_stream, Z_BEST_COMPRESSION) != Z_OK)
		{
			inflateEnd(&session->d_stream);
			session->status = IZIP_CLOSED;
			return;
		}

		/* Just in case, do this last */
		session->status = IZIP_OPEN;
	}

	virtual void OnRawSocketAccept(int fd, irc::sockets::sockaddrs*, irc::sockets::sockaddrs*)
	{
		/* Nothing special needs doing here compared to connect() */
		OnRawSocketConnect(fd);
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

		if (session->inbuf.length())
		{
			/* Our input buffer is filling up. This is *BAD*.
			 * We can't return more data than fits into buffer
			 * (count bytes), so we will generate another read
			 * event on purpose by *NOT* reading from 'fd' at all
			 * for now.
			 */
			readresult = 0;
		}
		else
		{
			/* Read read_buffer_size bytes at a time to the buffer (usually 2.5k) */
			readresult = read(fd, net_buffer, net_buffer_size);

			total_in_compressed += readresult;

			/* Copy the compressed data into our input buffer */
			session->inbuf.append(net_buffer, readresult);
		}

		size_t in_len = session->inbuf.length();

		/* Do we have anything to do? */
		if (in_len <= 0)
			return 0;

		/* Prepare decompression */
		session->d_stream.next_in = (Bytef *)session->inbuf.c_str();
		session->d_stream.avail_in = in_len;

		session->d_stream.next_out = (Bytef*)buffer;
		/* Last byte is reserved for NULL terminating that beast */
		session->d_stream.avail_out = count - 1;

		/* Z_SYNC_FLUSH: Do as much as possible */
		int ret = inflate(&session->d_stream, Z_SYNC_FLUSH);
		/* TODO CloseStream() in here at random places */
		switch (ret)
		{
			case Z_NEED_DICT:
			case Z_STREAM_ERROR:
				/* This is one of the 'not supposed to happen' things.
				 * Memory corruption, anyone?
				 */
				Error(session, "General Error. This is not supposed to happen :/");
				break;
			case Z_DATA_ERROR:
				Error(session, "Decompression failed, malformed data");
				break;
			case Z_MEM_ERROR:
				Error(session, "Out of memory");
				break;
			case Z_BUF_ERROR:
				/* This one is non-fatal, buffer is just full
				 * (can't happen here).
				 */
				Error(session, "Internal error. This is not supposed to happen.");
				break;
			case Z_STREAM_END:
				/* This module *never* generates these :/ */
				Error(session, "End-of-stream marker received");
				break;
			case Z_OK:
				break;
			default:
				/* NO WAI! This can't happen. All errors are handled above. */
				Error(session, "Unknown error");
				break;
		}
		if (ret != Z_OK)
		{
			readresult = 0;
			return 0;
		}

		/* Update the inbut buffer */
		unsigned int input_compressed = in_len - session->d_stream.avail_in;
		session->inbuf = session->inbuf.substr(input_compressed);

		/* Update counters (Old size - new size) */
		unsigned int uncompressed_length = (count - 1) - session->d_stream.avail_out;
		total_in_uncompressed += uncompressed_length;

		/* Null-terminate the buffer -- this doesnt harm binary data */
		buffer[uncompressed_length] = 0;

		/* Set the read size to the correct total size */
		readresult = uncompressed_length;

		return 1;
	}

	virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
	{
		izip_session* session = &sessions[fd];

		if (!count)	/* Nothing to do! */
			return 0;

		if(session->status != IZIP_OPEN)
			/* Seriously, wtf? */
			return 0;

		int ret;

		/* This loop is really only supposed to run once, but in case 'compr'
		 * is filled up somehow we are prepared to handle this situation.
		 */
		unsigned int offset = 0;
		do
		{
			/* Prepare compression */
			session->c_stream.next_in = (Bytef*)buffer + offset;
			session->c_stream.avail_in = count - offset;

			session->c_stream.next_out = (Bytef*)net_buffer;
			session->c_stream.avail_out = net_buffer_size;

			/* Compress the text */
			ret = deflate(&session->c_stream, Z_SYNC_FLUSH);
			/* TODO CloseStream() in here at random places */
			switch (ret)
			{
				case Z_OK:
					break;
				case Z_BUF_ERROR:
					/* This one is non-fatal, buffer is just full
					 * (can't happen here).
					 */
					Error(session, "Internal error. This is not supposed to happen.");
					break;
				case Z_STREAM_ERROR:
					/* This is one of the 'not supposed to happen' things.
					 * Memory corruption, anyone?
					 */
					Error(session, "General Error. This is also not supposed to happen.");
					break;
				default:
					Error(session, "Unknown error");
					break;
			}

			if (ret != Z_OK)
				return 0;

			/* Space before - space after stuff was added to this */
			unsigned int compressed = net_buffer_size - session->c_stream.avail_out;
			unsigned int uncompressed = count - session->c_stream.avail_in;

			/* Make it skip the data which was compressed already */
			offset += uncompressed;

			/* Update stats */
			total_out_uncompressed += uncompressed;
			total_out_compressed += compressed;

			/* Add compressed to the output buffer */
			session->outbuf.append((const char*)net_buffer, compressed);
		} while (session->c_stream.avail_in != 0);

		/* Lets see how much we can send out */
		ret = write(fd, session->outbuf.data(), session->outbuf.length());

		/* Check for errors, and advance the buffer if any was sent */
		if (ret > 0)
			session->outbuf = session->outbuf.substr(ret);
		else if (ret < 1)
		{
			if (errno == EAGAIN)
				return 0;
			else
			{
				session->outbuf.clear();
				return 0;
			}
		}

		/* ALL LIES the lot of it, we havent really written
		 * this amount, but the layer above doesnt need to know.
		 */
		return count;
	}

	void Error(izip_session* session, const std::string &text)
	{
		ServerInstance->SNO->WriteToSnoMask('l', "ziplink error: " + text);
	}

	void CloseSession(izip_session* session)
	{
		if (session->status == IZIP_OPEN)
		{
			session->status = IZIP_CLOSED;
			session->outbuf.clear();
			inflateEnd(&session->d_stream);
			deflateEnd(&session->c_stream);
		}
	}

};

MODULE_INIT(ModuleZLib)

