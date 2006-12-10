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

#include "ssl.h"

/* $ModDesc: Provides zlib link support for servers */
/* $LinkerFlags: -lz */
/* $ModDep: ssl.h */


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
	int fd;
};

class ModuleZLib : public Module
{
	izip_session sessions[MAX_DESCRIPTORS];
	
 public:
	
	ModuleZLib(InspIRCd* Me)
		: Module::Module(Me)
	{
		ServerInstance->PublishInterface("InspSocketHook", this);
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
		List[I_OnRequest] = 1;
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


	virtual void OnRawSocketAccept(int fd, const std::string &ip, int localport)
	{
		izip_session* session = &sessions[fd];
	
		/* allocate deflate state */
		session->fd = fd;
		session->status = IZIP_WAITFIRST;

		session->c_stream.zalloc = (alloc_func)0;
		session->c_stream.zfree = (free_func)0;
		session->c_stream.opaque = (voidpf)0;

	        if (deflateInit(&session->c_stream, Z_DEFAULT_COMPRESSION) != Z_OK)
			return;

		session->d_stream.zalloc = (alloc_func)0;
		session->d_stream.zfree = (free_func)0;
		session->d_stream.opaque = (voidpf)0;

		if (deflateInit(&session->d_stream, Z_DEFAULT_COMPRESSION) != Z_OK)
			return;
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
		if (read(fd, &size, sizeof(size)) != sizeof(size))
			return 0;

		size = ntohl(size);

		ServerInstance->Log(DEBUG,"Size of frame to read: %d", size);

		unsigned char compr[size+1];

		readresult = read(fd, compr, size);

		if (readresult == size)
		{
			if(session->status == IZIP_WAITFIRST)
			{
				session->status = IZIP_OPEN;
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

			readresult = session->d_stream.total_out;

			buffer[readresult] = 0;

			ServerInstance->Log(DEBUG,"DECOMPRESSED: '%s'", buffer);
		}
		else
		{
			/* XXX: We need to buffer here, really. */
			if (readresult == -1)
			{
				ServerInstance->Log(DEBUG,"Error: %s", strerror(errno));
				if (errno == EAGAIN)
					ServerInstance->Log(DEBUG,"(EAGAIN)");
			}
			ServerInstance->Log(DEBUG,"Didnt read whole frame, got %d bytes of %d!", readresult, size);
		}

		return (readresult > 0);
	}

	virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
	{
		int ocount = count;
		ServerInstance->Log(DEBUG,"Write event of %d uncompressed bytes: '%s'", count, buffer);

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

		// Z_BEST_COMPRESSION
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

		ServerInstance->Log(DEBUG,"Write %d compressed bytes", session->c_stream.total_out);
		int x = htonl(session->c_stream.total_out);
		memcpy(compr, &x, sizeof(x));
		write(fd, compr, session->c_stream.total_out+4);

		deflateEnd(&session->c_stream);

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
