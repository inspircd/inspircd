#include <string>
#include <vector>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "inspircd_config.h"
#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "socket.h"
#include "hashcomp.h"

/* $ModDesc: Provides SSL support for clients */
/* $CompileFlags: -I/usr/include -I/usr/local/include */
/* $LinkerFlags: -L/usr/local/lib -Wl,--rpath -Wl,/usr/local/lib -L/usr/lib -Wl,--rpath -Wl,/usr/lib -lssl */

enum issl_status { ISSL_NONE, ISSL_HANDSHAKING, ISSL_OPEN };
enum issl_io_status { ISSL_WRITE, ISSL_READ };

bool isin(int port, const std::vector<int> &portlist)
{
	for(unsigned int i = 0; i < portlist.size(); i++)
		if(portlist[i] == port)
			return true;
			
	return false;
}

char* get_error()
{
	return ERR_error_string(ERR_get_error(), NULL);
}

class issl_session : public classbase
{
public:
	SSL* sess;
	issl_status status;
	issl_io_status rstat;
	issl_io_status wstat;

	char* dummy;

	unsigned int inbufoffset;
	char* inbuf; 			// Buffer OpenSSL reads into.
	std::string outbuf;	// Buffer for outgoing data that OpenSSL will not take.
	int fd;
	
	issl_session()
	{
		rstat = ISSL_READ;
		wstat = ISSL_WRITE;
	}
};

class ModuleSSLOpenSSL : public Module
{
	Server* Srv;
	ServerConfig* SrvConf;
	ConfigReader* Conf;
	
	CullList culllist;
	
	std::vector<int> listenports;
	
	int inbufsize;
	issl_session sessions[MAX_DESCRIPTORS];
	
	SSL_CTX* ctx;
	
	std::string keyfile;
	std::string certfile;
	std::string cafile;
	// std::string crlfile;
	std::string dhfile;
	
 public:
	
	ModuleSSLOpenSSL(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		SrvConf = Srv->GetConfig();
		
		// Not rehashable...because I cba to reduce all the sizes of existing buffers.
		inbufsize = SrvConf->NetBufferSize;
		
		/* Global SSL library initialization*/
		SSL_library_init();
		SSL_load_error_strings();
		
		/* Build our SSL context*/
		ctx = SSL_CTX_new( SSLv23_server_method() );

		// Needs the flag as it ignores a plain /rehash
		OnRehash("ssl");
	}
	
	virtual void OnRehash(const std::string &param)
	{
		if(param != "ssl")
			return;
	
		Conf = new ConfigReader;
			
		for(unsigned int i = 0; i < listenports.size(); i++)
		{
			SrvConf->DelIOHook(listenports[i]);
		}
		
		listenports.clear();
		
		for(int i = 0; i < Conf->Enumerate("bind"); i++)
		{
			// For each <bind> tag
			if(((Conf->ReadValue("bind", "type", i) == "") || (Conf->ReadValue("bind", "type", i) == "clients")) && (Conf->ReadValue("bind", "ssl", i) == "openssl"))
			{
				// Get the port we're meant to be listening on with SSL
				unsigned int port = Conf->ReadInteger("bind", "port", i, true);
				if(SrvConf->AddIOHook(port, this))
				{
					// We keep a record of which ports we're listening on with SSL
					listenports.push_back(port);
				
					log(DEFAULT, "m_ssl_openssl.so: Enabling SSL for port %d", port);
				}
				else
				{
					log(DEFAULT, "m_ssl_openssl.so: FAILED to enable SSL on port %d, maybe you have another ssl or similar module loaded?", port);
				}
			}
		}
		
		std::string confdir(CONFIG_FILE);
		// +1 so we the path ends with a /
		confdir = confdir.substr(0, confdir.find_last_of('/') + 1);
		
		cafile	= Conf->ReadValue("openssl", "cafile", 0);
		// crlfile	= Conf->ReadValue("openssl", "crlfile", 0);
		certfile	= Conf->ReadValue("openssl", "certfile", 0);
		keyfile	= Conf->ReadValue("openssl", "keyfile", 0);
		dhfile	= Conf->ReadValue("openssl", "dhfile", 0);
		
		// Set all the default values needed.
		if(cafile == "")
			cafile = "ca.pem";
			
		//if(crlfile == "")
		//	crlfile = "crl.pem";
			
		if(certfile == "")
			certfile = "cert.pem";
			
		if(keyfile == "")
			keyfile = "key.pem";
			
		if(dhfile == "")
			dhfile = "dhparams.pem";
			
		// Prepend relative paths with the path to the config directory.	
		if(cafile[0] != '/')
			cafile = confdir + cafile;
		
		//if(crlfile[0] != '/')
		//	crlfile = confdir + crlfile;
			
		if(certfile[0] != '/')
			certfile = confdir + certfile;
			
		if(keyfile[0] != '/')
			keyfile = confdir + keyfile;
			
		if(dhfile[0] != '/')
			dhfile = confdir + dhfile;

		/* Load our keys and certificates*/
		if(!SSL_CTX_use_certificate_chain_file(ctx, certfile.c_str()))
		{
			log(DEFAULT, "m_ssl_openssl.so: Can't read certificate file %s", certfile.c_str());
		}

		if(!SSL_CTX_use_PrivateKey_file(ctx, keyfile.c_str(), SSL_FILETYPE_PEM))
		{
			log(DEFAULT, "m_ssl_openssl.so: Can't read key file %s", keyfile.c_str());
		}

		/* Load the CAs we trust*/
		if(!SSL_CTX_load_verify_locations(ctx, cafile.c_str(), 0))
		{
			log(DEFAULT, "m_ssl_openssl.so: Can't read CA list from ", cafile.c_str());
		}

		FILE* dhpfile = fopen(dhfile.c_str(), "r");
		DH* ret;

		if(dhpfile == NULL)
		{
			log(DEFAULT, "m_ssl_openssl.so Couldn't open DH file %s: %s", dhfile.c_str(), strerror(errno));
			throw ModuleException();
		}
		else
		{
			ret = PEM_read_DHparams(dhpfile, NULL, NULL, NULL);
		
			if(SSL_CTX_set_tmp_dh(ctx, ret) < 0)
			{
				log(DEFAULT, "m_ssl_openssl.so: Couldn't set DH parameters");
			}
		}
		
		fclose(dhpfile);

		DELETE(Conf);
	}

	virtual ~ModuleSSLOpenSSL()
	{
		SSL_CTX_free(ctx);
	}
	
	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			
			if(user->GetExt("ssl", dummy) && IS_LOCAL(user) && isin(user->port, listenports))
			{
				// User is using SSL, they're a local user, and they're using one of *our* SSL ports.
				// Potentially there could be multiple SSL modules loaded at once on different ports.
				log(DEBUG, "m_ssl_openssl.so: Adding user %s to cull list", user->nick);
				culllist.AddItem(user, "SSL module unloading");
			}
		}
	}
	
	virtual void OnUnloadModule(Module* mod, const std::string &name)
	{
		if(mod == this)
		{
			// We're being unloaded, kill all the users added to the cull list in OnCleanup
			int numusers = culllist.Apply();
			log(DEBUG, "m_ssl_openssl.so: Killed %d users for unload of OpenSSL SSL module", numusers);
			
			for(unsigned int i = 0; i < listenports.size(); i++)
				SrvConf->DelIOHook(listenports[i]);
		}
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR);
	}

	void Implements(char* List)
	{
		List[I_OnRawSocketAccept] = List[I_OnRawSocketClose] = List[I_OnRawSocketRead] = List[I_OnRawSocketWrite] = List[I_OnCleanup] = 1;
		List[I_OnSyncUserMetaData] = List[I_OnDecodeMetaData] = List[I_OnUnloadModule] = List[I_OnRehash] = List[I_OnWhois] = List[I_OnGlobalConnect] = 1;
	}

	virtual void OnRawSocketAccept(int fd, const std::string &ip, int localport)
	{
		issl_session* session = &sessions[fd];
	
		session->fd = fd;
		session->inbuf = new char[inbufsize];
		session->inbufoffset = 0;		
		session->sess = SSL_new(ctx);
		session->status = ISSL_NONE;
	
		if(session->sess == NULL)
		{
			log(DEBUG, "m_ssl.so: Couldn't create SSL object: %s", get_error());
			return;
		}
		
		if(SSL_set_fd(session->sess, fd) == 0)
		{
			log(DEBUG, "m_ssl.so: Couldn't set fd for SSL object: %s", get_error());
			return;
		}

 		Handshake(session);
	}

	virtual void OnRawSocketClose(int fd)
	{
		log(DEBUG, "m_ssl_openssl.so: OnRawSocketClose: %d", fd);
		CloseSession(&sessions[fd]);
	}
	
	virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult)
	{
		issl_session* session = &sessions[fd];
		
		if(!session->sess)
		{
			log(DEBUG, "m_ssl_openssl.so: OnRawSocketRead: No session to read from");
			readresult = 0;
			CloseSession(session);
			return 1;
		}
		
		log(DEBUG, "m_ssl_openssl.so: OnRawSocketRead(%d, buffer, %u, %d)", fd, count, readresult);
		
		if(session->status == ISSL_HANDSHAKING)
		{
			if(session->rstat == ISSL_READ || session->wstat == ISSL_READ)
			{
				// The handshake isn't finished and it wants to read, try to finish it.
				if(Handshake(session))
				{
					// Handshake successfully resumed.
					log(DEBUG, "m_ssl_openssl.so: OnRawSocketRead: successfully resumed handshake");
				}
				else
				{
					// Couldn't resume handshake.	
					log(DEBUG, "m_ssl_openssl.so: OnRawSocketRead: failed to resume handshake");
					return -1;
				}
			}
			else
			{
				log(DEBUG, "m_ssl_openssl.so: OnRawSocketRead: handshake wants to write data but we are currently reading");
				return -1;			
			}
		}
		
		// If we resumed the handshake then session->status will be ISSL_OPEN
				
		if(session->status == ISSL_OPEN)
		{
			if(session->wstat == ISSL_READ)
			{
				if(DoWrite(session) == 0)
					return 0;
			}
			
			if(session->rstat == ISSL_READ)
			{
				int ret = DoRead(session);
			
				if(ret > 0)
				{
					if(count <= session->inbufoffset)
					{
						memcpy(buffer, session->inbuf, count);
						// Move the stuff left in inbuf to the beginning of it
						memcpy(session->inbuf, session->inbuf + count, (session->inbufoffset - count));
						// Now we need to set session->inbufoffset to the amount of data still waiting to be handed to insp.
						session->inbufoffset -= count;
						// Insp uses readresult as the count of how much data there is in buffer, so:
						readresult = count;
					}
					else
					{
						// There's not as much in the inbuf as there is space in the buffer, so just copy the whole thing.
						memcpy(buffer, session->inbuf, session->inbufoffset);
						
						readresult = session->inbufoffset;
						// Zero the offset, as there's nothing there..
						session->inbufoffset = 0;
					}
				
					log(DEBUG, "m_ssl_openssl.so: OnRawSocketRead: Passing %d bytes up to insp:", count);
					Srv->Log(DEBUG, std::string(buffer, readresult));
				
					return 1;
				}
				else
				{
					return ret;
				}
			}
		}
		
		return -1;
	}
	
	virtual int OnRawSocketWrite(int fd, char* buffer, int count)
	{		
		issl_session* session = &sessions[fd];

		if(!session->sess)
		{
			log(DEBUG, "m_ssl_openssl.so: OnRawSocketWrite: No session to write to");
			CloseSession(session);
			return 1;
		}
		
		log(DEBUG, "m_ssl_openssl.so: OnRawSocketWrite: Adding %d bytes to the outgoing buffer", count);		
		session->outbuf.append(buffer, count);
		
		if(session->status == ISSL_HANDSHAKING)
		{
			// The handshake isn't finished, try to finish it.
			if(session->rstat == ISSL_WRITE || session->wstat == ISSL_WRITE)
			{
				if(Handshake(session))
				{
					// Handshake successfully resumed.
					log(DEBUG, "m_ssl_openssl.so: OnRawSocketWrite: successfully resumed handshake");
				}
				else
				{
					// Couldn't resume handshake.	
					log(DEBUG, "m_ssl_openssl.so: OnRawSocketWrite: failed to resume handshake"); 
				}
			}
			else
			{
				log(DEBUG, "m_ssl_openssl.so: OnRawSocketWrite: handshake wants to read data but we are currently writing");			
			}
		}
		
		if(session->status == ISSL_OPEN)
		{
			if(session->rstat == ISSL_WRITE)
			{
				DoRead(session);
			}
			
			if(session->wstat == ISSL_WRITE)
			{
				return DoWrite(session);
			}
		}
		
		return 1;
	}
	
	int DoWrite(issl_session* session)
	{
		log(DEBUG, "m_ssl_openssl.so: DoWrite: Trying to write %d bytes:", session->outbuf.size());
		Srv->Log(DEBUG, session->outbuf);
			
		int ret = SSL_write(session->sess, session->outbuf.data(), session->outbuf.size());
		
		if(ret == 0)
		{
			log(DEBUG, "m_ssl_openssl.so: DoWrite: Client closed the connection");
			CloseSession(session);
			return 0;
		}
		else if(ret < 0)
		{
			int err = SSL_get_error(session->sess, ret);
			
			if(err == SSL_ERROR_WANT_WRITE)
			{
				log(DEBUG, "m_ssl_openssl.so: DoWrite: Not all SSL data written, need to retry: %s", get_error());
				session->wstat = ISSL_WRITE;
				return -1;
			}
			else if(err == SSL_ERROR_WANT_READ)
			{
				log(DEBUG, "m_ssl_openssl.so: DoWrite: Not all SSL data written but the damn thing wants to read instead: %s", get_error());
				session->wstat = ISSL_READ;
				return -1;
			}
			else
			{
				log(DEBUG, "m_ssl_openssl.so: DoWrite: Error writing SSL data: %s", get_error());
				CloseSession(session);
				return 0;
			}
		}
		else
		{
			log(DEBUG, "m_ssl_openssl.so: DoWrite: Successfully wrote %d bytes", ret);
			session->outbuf = session->outbuf.substr(ret);
			return ret;
		}
	}
	
	int DoRead(issl_session* session)
	{
		// Is this right? Not sure if the unencrypted data is garaunteed to be the same length.
		// Read into the inbuffer, offset from the beginning by the amount of data we have that insp hasn't taken yet.
		log(DEBUG, "m_ssl_openssl.so: DoRead: SSL_read(sess, inbuf+%d, %d-%d)", session->inbufoffset, inbufsize, session->inbufoffset);
			
		int ret = SSL_read(session->sess, session->inbuf + session->inbufoffset, inbufsize - session->inbufoffset);

		if(ret == 0)
		{
			// Client closed connection.
			log(DEBUG, "m_ssl_openssl.so: DoRead: Client closed the connection");
			CloseSession(session);
			return 0;
		}
		else if(ret < 0)
		{
			int err = SSL_get_error(session->sess, ret);
				
			if(err == SSL_ERROR_WANT_READ)
			{
				log(DEBUG, "m_ssl_openssl.so: DoRead: Not all SSL data read, need to retry: %s", get_error());
				session->rstat = ISSL_READ;
				return -1;
			}
			else if(err == SSL_ERROR_WANT_WRITE)
			{
				log(DEBUG, "m_ssl_openssl.so: DoRead: Not all SSL data read but the damn thing wants to write instead: %s", get_error());
				session->rstat = ISSL_WRITE;
				return -1;
			}
			else
			{
				log(DEBUG, "m_ssl_openssl.so: DoRead: Error reading SSL data: %s", get_error());
				CloseSession(session);
				return 0;
			}
		}
		else
		{
			// Read successfully 'ret' bytes into inbuf + inbufoffset
			// There are 'ret' + 'inbufoffset' bytes of data in 'inbuf'
			// 'buffer' is 'count' long
			
			log(DEBUG, "m_ssl_openssl.so: DoRead: Read %d bytes, now have %d waiting to be passed up", ret, ret + session->inbufoffset);

			session->inbufoffset += ret;

			return ret;
		}
	}
	
	// :kenny.chatspike.net 320 Om Epy|AFK :is a Secure Connection
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		// Bugfix, only send this numeric for *our* SSL users
		if(dest->GetExt("ssl", dummy) || (IS_LOCAL(dest) &&  isin(dest->port, listenports)))
		{
			WriteServ(source->fd, "320 %s %s :is using a secure connection", source->nick, dest->nick);
		}
	}
	
	virtual void OnSyncUserMetaData(userrec* user, Module* proto, void* opaque, const std::string &extname)
	{
		// check if the linking module wants to know about OUR metadata
		if(extname == "ssl")
		{
			// check if this user has an swhois field to send
			if(user->GetExt(extname, dummy))
			{
				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, "ON");
			}
		}
	}
	
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_USER) && (extname == "ssl"))
		{
			userrec* dest = (userrec*)target;
			// if they dont already have an ssl flag, accept the remote server's
			if (!dest->GetExt(extname, dummy))
			{
				dest->Extend(extname, "ON");
			}
		}
	}
	
	bool Handshake(issl_session* session)
	{		
		int ret = SSL_accept(session->sess);
      
		if(ret < 0)
		{
			int err = SSL_get_error(session->sess, ret);
				
			if(err == SSL_ERROR_WANT_READ)
			{
				log(DEBUG, "m_ssl_openssl.so: Handshake: Not completed, need to read again: %s", get_error());
				session->rstat = ISSL_READ;
				session->status = ISSL_HANDSHAKING;
			}
			else if(err == SSL_ERROR_WANT_WRITE)
			{
				log(DEBUG, "m_ssl_openssl.so: Handshake: Not completed, need to write more data: %s", get_error());
				session->wstat = ISSL_WRITE;
				session->status = ISSL_HANDSHAKING;
				MakePollWrite(session);
			}
			else
			{
				log(DEBUG, "m_ssl_openssl.so: Handshake: Failed, bailing: %s", get_error());
				CloseSession(session);
			}

			return false;
		}
		else
		{
			// Handshake complete.
			log(DEBUG, "m_ssl_openssl.so: Handshake completed");
			
			// This will do for setting the ssl flag...it could be done earlier if it's needed. But this seems neater.
			userrec* u = Srv->FindDescriptor(session->fd);
			if (u)
			{
				if (!u->GetExt("ssl", dummy))
					u->Extend("ssl", "ON");
			}
			
			session->status = ISSL_OPEN;
			
			MakePollWrite(session);
			
			return true;
		}
	}
	
	virtual void OnGlobalConnect(userrec* user)
	{
		// This occurs AFTER OnUserConnect so we can be sure the
		// protocol module has propogated the NICK message.
		if ((user->GetExt("ssl", dummy)) && (IS_LOCAL(user)))
		{
			// Tell whatever protocol module we're using that we need to inform other servers of this metadata NOW.
			std::deque<std::string>* metadata = new std::deque<std::string>;
			metadata->push_back(user->nick);
			metadata->push_back("ssl");		// The metadata id
			metadata->push_back("ON");		// The value to send
			Event* event = new Event((char*)metadata,(Module*)this,"send_metadata");
			event->Send();				// Trigger the event. We don't care what module picks it up.
			DELETE(event);
			DELETE(metadata);
		}
	}
	
	void MakePollWrite(issl_session* session)
	{
		OnRawSocketWrite(session->fd, NULL, 0);
	}
	
	void CloseSession(issl_session* session)
	{
		if(session->sess)
		{
			SSL_shutdown(session->sess);
			SSL_free(session->sess);
		}
		
		if(session->inbuf)
		{
			delete[] session->inbuf;
		}
		
		session->outbuf.clear();
		session->inbuf = NULL;
		session->sess = NULL;
		session->status = ISSL_NONE;
	}
};

class ModuleSSLOpenSSLFactory : public ModuleFactory
{
 public:
	ModuleSSLOpenSSLFactory()
	{
	}
	
	~ModuleSSLOpenSSLFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSSLOpenSSL(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleSSLOpenSSLFactory;
}
