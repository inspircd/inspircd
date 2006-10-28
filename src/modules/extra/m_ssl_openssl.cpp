#include <string>
#include <vector>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "inspircd_config.h"
#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "socket.h"
#include "hashcomp.h"
#include "inspircd.h"

#include "ssl_cert.h"

/* $ModDesc: Provides SSL support for clients */
/* $CompileFlags: -I/usr/include -I/usr/local/include */
/* $LinkerFlags: -L/usr/local/lib -Wl,--rpath -Wl,/usr/local/lib -L/usr/lib -Wl,--rpath -Wl,/usr/lib -lssl */



enum issl_status { ISSL_NONE, ISSL_HANDSHAKING, ISSL_OPEN };
enum issl_io_status { ISSL_WRITE, ISSL_READ };

static bool SelfSigned = false;

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

/** Represents an SSL user's extra data
 */
class issl_session : public classbase
{
public:
	SSL* sess;
	issl_status status;
	issl_io_status rstat;
	issl_io_status wstat;

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

static int OnVerify(int preverify_ok, X509_STORE_CTX *ctx)
{
	/* XXX: This will allow self signed certificates.
	 * In the future if we want an option to not allow this,
	 * we can just return preverify_ok here, and openssl
	 * will boot off self-signed and invalid peer certs.
	 */
	int ve = X509_STORE_CTX_get_error(ctx);

	SelfSigned = (ve == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT);

	return 1;
}
	
class ModuleSSLOpenSSL : public Module
{
	
	ConfigReader* Conf;
	
	CullList* culllist;
	
	std::vector<int> listenports;
	
	int inbufsize;
	issl_session sessions[MAX_DESCRIPTORS];
	
	SSL_CTX* ctx;
	
	char* dummy;
	
	std::string keyfile;
	std::string certfile;
	std::string cafile;
	// std::string crlfile;
	std::string dhfile;
	
 public:
	
	ModuleSSLOpenSSL(InspIRCd* Me)
		: Module::Module(Me)
	{
		culllist = new CullList(ServerInstance);
		
		// Not rehashable...because I cba to reduce all the sizes of existing buffers.
		inbufsize = ServerInstance->Config->NetBufferSize;
		
		/* Global SSL library initialization*/
		SSL_library_init();
		SSL_load_error_strings();
		
		/* Build our SSL context*/
		ctx = SSL_CTX_new( SSLv23_server_method() );

		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, OnVerify);

		// Needs the flag as it ignores a plain /rehash
		OnRehash("ssl");
	}
	
	virtual void OnRehash(const std::string &param)
	{
		if(param != "ssl")
			return;
	
		Conf = new ConfigReader(ServerInstance);
			
		for(unsigned int i = 0; i < listenports.size(); i++)
		{
			ServerInstance->Config->DelIOHook(listenports[i]);
		}
		
		listenports.clear();
		
		for(int i = 0; i < Conf->Enumerate("bind"); i++)
		{
			// For each <bind> tag
			if(((Conf->ReadValue("bind", "type", i) == "") || (Conf->ReadValue("bind", "type", i) == "clients")) && (Conf->ReadValue("bind", "ssl", i) == "openssl"))
			{
				// Get the port we're meant to be listening on with SSL
				unsigned int port = Conf->ReadInteger("bind", "port", i, true);
				if (ServerInstance->Config->AddIOHook(port, this))
				{
					// We keep a record of which ports we're listening on with SSL
					listenports.push_back(port);
				
					ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: Enabling SSL for port %d", port);
				}
				else
				{
					ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: FAILED to enable SSL on port %d, maybe you have another ssl or similar module loaded?", port);
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
			ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: Can't read certificate file %s", certfile.c_str());
		}

		if(!SSL_CTX_use_PrivateKey_file(ctx, keyfile.c_str(), SSL_FILETYPE_PEM))
		{
			ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: Can't read key file %s", keyfile.c_str());
		}

		/* Load the CAs we trust*/
		if(!SSL_CTX_load_verify_locations(ctx, cafile.c_str(), 0))
		{
			ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: Can't read CA list from ", cafile.c_str());
		}

		FILE* dhpfile = fopen(dhfile.c_str(), "r");
		DH* ret;

		if(dhpfile == NULL)
		{
			ServerInstance->Log(DEFAULT, "m_ssl_openssl.so Couldn't open DH file %s: %s", dhfile.c_str(), strerror(errno));
			throw ModuleException();
		}
		else
		{
			ret = PEM_read_DHparams(dhpfile, NULL, NULL, NULL);
		
			if(SSL_CTX_set_tmp_dh(ctx, ret) < 0)
			{
				ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: Couldn't set DH parameters");
			}
		}
		
		fclose(dhpfile);

		DELETE(Conf);
	}

	virtual ~ModuleSSLOpenSSL()
	{
		SSL_CTX_free(ctx);
		delete culllist;
	}
	
	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			
			if(user->GetExt("ssl", dummy) && IS_LOCAL(user) && isin(user->GetPort(), listenports))
			{
				// User is using SSL, they're a local user, and they're using one of *our* SSL ports.
				// Potentially there could be multiple SSL modules loaded at once on different ports.
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: Adding user %s to cull list", user->nick);
				culllist->AddItem(user, "SSL module unloading");
			}
			if (user->GetExt("ssl_cert", dummy) && isin(user->GetPort(), listenports))
			{
				ssl_cert* tofree;
				user->GetExt("ssl_cert", tofree);
				delete tofree;
				user->Shrink("ssl_cert");
			}
		}
	}
	
	virtual void OnUnloadModule(Module* mod, const std::string &name)
	{
		if(mod == this)
		{
			// We're being unloaded, kill all the users added to the cull list in OnCleanup
			int numusers = culllist->Apply();
			ServerInstance->Log(DEBUG, "m_ssl_openssl.so: Killed %d users for unload of OpenSSL SSL module", numusers);
			
			for(unsigned int i = 0; i < listenports.size(); i++)
				ServerInstance->Config->DelIOHook(listenports[i]);
		}
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnRawSocketAccept] = List[I_OnRawSocketClose] = List[I_OnRawSocketRead] = List[I_OnRawSocketWrite] = List[I_OnCleanup] = 1;
		List[I_OnSyncUserMetaData] = List[I_OnDecodeMetaData] = List[I_OnUnloadModule] = List[I_OnRehash] = List[I_OnWhois] = List[I_OnPostConnect] = 1;
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
			ServerInstance->Log(DEBUG, "m_ssl.so: Couldn't create SSL object: %s", get_error());
			return;
		}
		
		if(SSL_set_fd(session->sess, fd) == 0)
		{
			ServerInstance->Log(DEBUG, "m_ssl.so: Couldn't set fd for SSL object: %s", get_error());
			return;
		}

 		Handshake(session);
	}

	virtual void OnRawSocketClose(int fd)
	{
		ServerInstance->Log(DEBUG, "m_ssl_openssl.so: OnRawSocketClose: %d", fd);
		CloseSession(&sessions[fd]);

		EventHandler* user = ServerInstance->SE->GetRef(fd);

		if ((user) && (user->GetExt("ssl_cert", dummy)))
		{
			ssl_cert* tofree;
			user->GetExt("ssl_cert", tofree);
			delete tofree;
			user->Shrink("ssl_cert");
		}
	}

	virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult)
	{
		issl_session* session = &sessions[fd];
		
		if(!session->sess)
		{
			ServerInstance->Log(DEBUG, "m_ssl_openssl.so: OnRawSocketRead: No session to read from");
			readresult = 0;
			CloseSession(session);
			return 1;
		}
		
		if(session->status == ISSL_HANDSHAKING)
		{
			if(session->rstat == ISSL_READ || session->wstat == ISSL_READ)
			{
				// The handshake isn't finished and it wants to read, try to finish it.
				if(Handshake(session))
				{
					// Handshake successfully resumed.
					ServerInstance->Log(DEBUG, "m_ssl_openssl.so: OnRawSocketRead: successfully resumed handshake");
				}
				else
				{
					// Couldn't resume handshake.	
					ServerInstance->Log(DEBUG, "m_ssl_openssl.so: OnRawSocketRead: failed to resume handshake");
					return -1;
				}
			}
			else
			{
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: OnRawSocketRead: handshake wants to write data but we are currently reading");
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
	
	virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
	{		
		issl_session* session = &sessions[fd];

		if(!session->sess)
		{
			ServerInstance->Log(DEBUG, "m_ssl_openssl.so: OnRawSocketWrite: No session to write to");
			CloseSession(session);
			return 1;
		}

		session->outbuf.append(buffer, count);
		
		if(session->status == ISSL_HANDSHAKING)
		{
			// The handshake isn't finished, try to finish it.
			if(session->rstat == ISSL_WRITE || session->wstat == ISSL_WRITE)
			{
				if(Handshake(session))
				{
					// Handshake successfully resumed.
					ServerInstance->Log(DEBUG, "m_ssl_openssl.so: OnRawSocketWrite: successfully resumed handshake");
				}
				else
				{
					// Couldn't resume handshake.	
					ServerInstance->Log(DEBUG, "m_ssl_openssl.so: OnRawSocketWrite: failed to resume handshake"); 
				}
			}
			else
			{
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: OnRawSocketWrite: handshake wants to read data but we are currently writing");			
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
		int ret = SSL_write(session->sess, session->outbuf.data(), session->outbuf.size());
		
		if(ret == 0)
		{
			ServerInstance->Log(DEBUG, "m_ssl_openssl.so: DoWrite: Client closed the connection");
			CloseSession(session);
			return 0;
		}
		else if(ret < 0)
		{
			int err = SSL_get_error(session->sess, ret);
			
			if(err == SSL_ERROR_WANT_WRITE)
			{
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: DoWrite: Not all SSL data written, need to retry: %s", get_error());
				session->wstat = ISSL_WRITE;
				return -1;
			}
			else if(err == SSL_ERROR_WANT_READ)
			{
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: DoWrite: Not all SSL data written but the damn thing wants to read instead: %s", get_error());
				session->wstat = ISSL_READ;
				return -1;
			}
			else
			{
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: DoWrite: Error writing SSL data: %s", get_error());
				CloseSession(session);
				return 0;
			}
		}
		else
		{
			session->outbuf = session->outbuf.substr(ret);
			return ret;
		}
	}
	
	int DoRead(issl_session* session)
	{
		// Is this right? Not sure if the unencrypted data is garaunteed to be the same length.
		// Read into the inbuffer, offset from the beginning by the amount of data we have that insp hasn't taken yet.
			
		int ret = SSL_read(session->sess, session->inbuf + session->inbufoffset, inbufsize - session->inbufoffset);

		if(ret == 0)
		{
			// Client closed connection.
			ServerInstance->Log(DEBUG, "m_ssl_openssl.so: DoRead: Client closed the connection");
			CloseSession(session);
			return 0;
		}
		else if(ret < 0)
		{
			int err = SSL_get_error(session->sess, ret);
				
			if(err == SSL_ERROR_WANT_READ)
			{
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: DoRead: Not all SSL data read, need to retry: %s", get_error());
				session->rstat = ISSL_READ;
				return -1;
			}
			else if(err == SSL_ERROR_WANT_WRITE)
			{
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: DoRead: Not all SSL data read but the damn thing wants to write instead: %s", get_error());
				session->rstat = ISSL_WRITE;
				return -1;
			}
			else
			{
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: DoRead: Error reading SSL data: %s", get_error());
				CloseSession(session);
				return 0;
			}
		}
		else
		{
			// Read successfully 'ret' bytes into inbuf + inbufoffset
			// There are 'ret' + 'inbufoffset' bytes of data in 'inbuf'
			// 'buffer' is 'count' long

			session->inbufoffset += ret;

			return ret;
		}
	}
	
	// :kenny.chatspike.net 320 Om Epy|AFK :is a Secure Connection
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		// Bugfix, only send this numeric for *our* SSL users
		if(dest->GetExt("ssl", dummy) || (IS_LOCAL(dest) &&  isin(dest->GetPort(), listenports)))
		{
			ServerInstance->SendWhoisLine(source, dest, 320, "%s %s :is using a secure connection", source->nick, dest->nick);
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
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: Handshake: Not completed, need to read again: %s", get_error());
				session->rstat = ISSL_READ;
				session->status = ISSL_HANDSHAKING;
			}
			else if(err == SSL_ERROR_WANT_WRITE)
			{
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: Handshake: Not completed, need to write more data: %s", get_error());
				session->wstat = ISSL_WRITE;
				session->status = ISSL_HANDSHAKING;
				MakePollWrite(session);
			}
			else
			{
				ServerInstance->Log(DEBUG, "m_ssl_openssl.so: Handshake: Failed, bailing: %s", get_error());
				CloseSession(session);
			}

			return false;
		}
		else
		{
			// Handshake complete.
			ServerInstance->Log(DEBUG, "m_ssl_openssl.so: Handshake completed");
			
			// This will do for setting the ssl flag...it could be done earlier if it's needed. But this seems neater.
			userrec* u = ServerInstance->FindDescriptor(session->fd);
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
	
	virtual void OnPostConnect(userrec* user)
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
			event->Send(ServerInstance);		// Trigger the event. We don't care what module picks it up.
			DELETE(event);
			DELETE(metadata);

			VerifyCertificate(&sessions[user->GetFd()], user);
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

	void VerifyCertificate(issl_session* session, userrec* user)
	{
		X509* cert;
		ssl_cert* certinfo = new ssl_cert;
		unsigned int n;
		unsigned char md[EVP_MAX_MD_SIZE];
		const EVP_MD *digest = EVP_md5();

		user->Extend("ssl_cert",certinfo);

		cert = SSL_get_peer_certificate((SSL*)session->sess);

		if (!cert)
		{
			certinfo->data.insert(std::make_pair("error","Could not get peer certificate: "+std::string(get_error())));
			return;
		}

		certinfo->data.insert(std::make_pair("invalid", SSL_get_verify_result(session->sess) != X509_V_OK ? ConvToStr(1) : ConvToStr(0)));

		if (SelfSigned)
		{
			certinfo->data.insert(std::make_pair("unknownsigner",ConvToStr(0)));
			certinfo->data.insert(std::make_pair("trusted",ConvToStr(1)));
		}
		else
		{
			certinfo->data.insert(std::make_pair("unknownsigner",ConvToStr(1)));
			certinfo->data.insert(std::make_pair("trusted",ConvToStr(0)));
		}

		certinfo->data.insert(std::make_pair("dn",std::string(X509_NAME_oneline(X509_get_subject_name(cert),0,0))));
		certinfo->data.insert(std::make_pair("issuer",std::string(X509_NAME_oneline(X509_get_issuer_name(cert),0,0))));

		if (!X509_digest(cert, digest, md, &n))
		{
			certinfo->data.insert(std::make_pair("error","Out of memory generating fingerprint"));
		}
		else
		{
			certinfo->data.insert(std::make_pair("fingerprint",irc::hex(md, n)));
		}

		user->WriteServ("NOTICE %s :*** Your SSL Certificate fingerprint is: %s", user->nick, irc::hex(md, n).c_str());

		if ((ASN1_UTCTIME_cmp_time_t(X509_get_notAfter(cert), time(NULL)) == -1) || (ASN1_UTCTIME_cmp_time_t(X509_get_notBefore(cert), time(NULL)) == 0))
		{
			certinfo->data.insert(std::make_pair("error","Not activated, or expired certificate"));
		}

		X509_free(cert);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSSLOpenSSL(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleSSLOpenSSLFactory;
}
