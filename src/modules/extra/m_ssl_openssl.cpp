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
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ssl.h"

#ifdef WINDOWS
#pragma comment(lib, "libeay32MTd")
#pragma comment(lib, "ssleay32MTd")
#undef MAX_DESCRIPTORS
#define MAX_DESCRIPTORS 10000
#endif

/* $ModDesc: Provides SSL support for clients */

/* $LinkerFlags: if("USE_FREEBSD_BASE_SSL") -lssl -lcrypto */
/* $CompileFlags: if(!"USE_FREEBSD_BASE_SSL") pkgconfversion("openssl","0.9.7") pkgconfincludes("openssl","/openssl/ssl.h","") */
/* $LinkerFlags: if(!"USE_FREEBSD_BASE_SSL") rpath("pkg-config --libs openssl") pkgconflibs("openssl","/libssl.so","-lssl -lcrypto -ldl") */

/* $ModDep: transport.h */
/* $NoPedantic */
/* $CopyInstall: conf/key.pem $(CONPATH) */
/* $CopyInstall: conf/cert.pem $(CONPATH) */


enum issl_status { ISSL_NONE, ISSL_HANDSHAKING, ISSL_OPEN };

static bool SelfSigned = false;

char* get_error()
{
	return ERR_error_string(ERR_get_error(), NULL);
}

static int error_callback(const char *str, size_t len, void *u);

/** Represents an SSL user's extra data
 */
class issl_session : public classbase
{
public:
	SSL* sess;
	issl_status status;

	int fd;
	bool outbound;
	bool data_to_write;

	issl_session()
	{
		outbound = false;
		data_to_write = false;
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
	int inbufsize;
	issl_session* sessions;

	SSL_CTX* ctx;
	SSL_CTX* clictx;

	char cipher[MAXBUF];

	std::string keyfile;
	std::string certfile;
	std::string cafile;
	// std::string crlfile;
	std::string dhfile;
	std::string sslports;

 public:

	ModuleSSLOpenSSL()
	{
		ServerInstance->Modules->PublishInterface("BufferedSocketHook", this);

		sessions = new issl_session[ServerInstance->SE->GetMaxFds()];

		// Not rehashable...because I cba to reduce all the sizes of existing buffers.
		inbufsize = ServerInstance->Config->NetBufferSize;

		/* Global SSL library initialization*/
		SSL_library_init();
		SSL_load_error_strings();

		/* Build our SSL contexts:
		 * NOTE: OpenSSL makes us have two contexts, one for servers and one for clients. ICK.
		 */
		ctx = SSL_CTX_new( SSLv23_server_method() );
		clictx = SSL_CTX_new( SSLv23_client_method() );

		SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
		SSL_CTX_set_mode(clictx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, OnVerify);
		SSL_CTX_set_verify(clictx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, OnVerify);

		// Needs the flag as it ignores a plain /rehash
		OnModuleRehash(NULL,"ssl");
		Implementation eventlist[] = {
			I_On005Numeric, I_OnRehash, I_OnModuleRehash, I_OnPostConnect,
			I_OnHookIO };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnHookIO(StreamSocket* user, ListenSocketBase* lsb)
	{
		if (!user->GetIOHook() && lsb->hook == "openssl")
		{
			/* Hook the user with our module */
			user->AddIOHook(this);
		}
	}

	void OnRehash(User* user)
	{
		ConfigReader Conf;

		sslports.clear();

		for (size_t i = 0; i < ServerInstance->ports.size(); i++)
		{
			ListenSocketBase* port = ServerInstance->ports[i];
			if (port->hook != "openssl")
				continue;

			std::string portid = port->GetBindDesc();
			ServerInstance->Logs->Log("m_ssl_openssl", DEFAULT, "m_ssl_openssl.so: Enabling SSL for port %s", portid.c_str());
			if (port->type == "clients" && port->GetIP() != "127.0.0.1")
				sslports.append(portid).append(";");
		}

		if (!sslports.empty())
			sslports.erase(sslports.end() - 1);
	}

	void OnModuleRehash(User* user, const std::string &param)
	{
		if (param != "ssl")
			return;

		OnRehash(user);

		ConfigReader Conf;

		std::string confdir(ServerInstance->ConfigFileName);
		// +1 so we the path ends with a /
		confdir = confdir.substr(0, confdir.find_last_of('/') + 1);

		cafile	 = Conf.ReadValue("openssl", "cafile", 0);
		certfile = Conf.ReadValue("openssl", "certfile", 0);
		keyfile	 = Conf.ReadValue("openssl", "keyfile", 0);
		dhfile	 = Conf.ReadValue("openssl", "dhfile", 0);

		// Set all the default values needed.
		if (cafile.empty())
			cafile = "ca.pem";

		if (certfile.empty())
			certfile = "cert.pem";

		if (keyfile.empty())
			keyfile = "key.pem";

		if (dhfile.empty())
			dhfile = "dhparams.pem";

		// Prepend relative paths with the path to the config directory.
		if ((cafile[0] != '/') && (!ServerInstance->Config->StartsWithWindowsDriveLetter(cafile)))
			cafile = confdir + cafile;

		if ((certfile[0] != '/') && (!ServerInstance->Config->StartsWithWindowsDriveLetter(certfile)))
			certfile = confdir + certfile;

		if ((keyfile[0] != '/') && (!ServerInstance->Config->StartsWithWindowsDriveLetter(keyfile)))
			keyfile = confdir + keyfile;

		if ((dhfile[0] != '/') && (!ServerInstance->Config->StartsWithWindowsDriveLetter(dhfile)))
			dhfile = confdir + dhfile;

		/* Load our keys and certificates
		 * NOTE: OpenSSL's error logging API sucks, don't blame us for this clusterfuck.
		 */
		if ((!SSL_CTX_use_certificate_chain_file(ctx, certfile.c_str())) || (!SSL_CTX_use_certificate_chain_file(clictx, certfile.c_str())))
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so: Can't read certificate file %s. %s", certfile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		if (((!SSL_CTX_use_PrivateKey_file(ctx, keyfile.c_str(), SSL_FILETYPE_PEM))) || (!SSL_CTX_use_PrivateKey_file(clictx, keyfile.c_str(), SSL_FILETYPE_PEM)))
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so: Can't read key file %s. %s", keyfile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		/* Load the CAs we trust*/
		if (((!SSL_CTX_load_verify_locations(ctx, cafile.c_str(), 0))) || (!SSL_CTX_load_verify_locations(clictx, cafile.c_str(), 0)))
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so: Can't read CA list from %s. %s", cafile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		FILE* dhpfile = fopen(dhfile.c_str(), "r");
		DH* ret;

		if (dhpfile == NULL)
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so Couldn't open DH file %s: %s", dhfile.c_str(), strerror(errno));
			throw ModuleException("Couldn't open DH file " + dhfile + ": " + strerror(errno));
		}
		else
		{
			ret = PEM_read_DHparams(dhpfile, NULL, NULL, NULL);
			if ((SSL_CTX_set_tmp_dh(ctx, ret) < 0) || (SSL_CTX_set_tmp_dh(clictx, ret) < 0))
			{
				ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so: Couldn't set DH parameters %s. SSL errors follow:", dhfile.c_str());
				ERR_print_errors_cb(error_callback, this);
			}
		}

		fclose(dhpfile);
	}

	void On005Numeric(std::string &output)
	{
		if (!sslports.empty())
			output.append(" SSL=" + sslports);
	}

	~ModuleSSLOpenSSL()
	{
		SSL_CTX_free(ctx);
		SSL_CTX_free(clictx);
		ServerInstance->Modules->UnpublishInterface("BufferedSocketHook", this);
		delete[] sessions;
	}

	void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			User* user = (User*)item;

			if (user->GetIOHook() == this)
			{
				// User is using SSL, they're a local user, and they're using one of *our* SSL ports.
				// Potentially there could be multiple SSL modules loaded at once on different ports.
				ServerInstance->Users->QuitUser(user, "SSL module unloading");
				user->DelIOHook();
			}
		}
	}

	Version GetVersion()
	{
		return Version("Provides SSL support for clients", VF_VENDOR, API_VERSION);
	}


	void OnRequest(Request& request)
	{
		Module* sslinfo = ServerInstance->Modules->Find("m_sslinfo.so");
		if (sslinfo)
			sslinfo->OnRequest(request);
	}


	void OnStreamSocketAccept(StreamSocket* user, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
	{
		int fd = user->GetFd();

		issl_session* session = &sessions[fd];

		session->fd = fd;
		session->sess = SSL_new(ctx);
		session->status = ISSL_NONE;
		session->outbound = false;

		if (session->sess == NULL)
			return;

		if (SSL_set_fd(session->sess, fd) == 0)
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEBUG,"BUG: Can't set fd with SSL_set_fd: %d", fd);
			return;
		}

 		Handshake(user, session);
	}

	void OnStreamSocketConnect(StreamSocket* user)
	{
		int fd = user->GetFd();
		/* Are there any possibilities of an out of range fd? Hope not, but lets be paranoid */
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds() -1))
			return;

		issl_session* session = &sessions[fd];

		session->fd = fd;
		session->sess = SSL_new(clictx);
		session->status = ISSL_NONE;
		session->outbound = true;

		if (session->sess == NULL)
			return;

		if (SSL_set_fd(session->sess, fd) == 0)
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEBUG,"BUG: Can't set fd with SSL_set_fd: %d", fd);
			return;
		}

		Handshake(user, session);
	}

	void OnStreamSocketClose(StreamSocket* user)
	{
		int fd = user->GetFd();
		/* Are there any possibilities of an out of range fd? Hope not, but lets be paranoid */
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds() - 1))
			return;

		CloseSession(&sessions[fd]);
	}

	int OnStreamSocketRead(StreamSocket* user, std::string& recvq)
	{
		int fd = user->GetFd();
		/* Are there any possibilities of an out of range fd? Hope not, but lets be paranoid */
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds() - 1))
			return -1;

		issl_session* session = &sessions[fd];

		if (!session->sess)
		{
			CloseSession(session);
			return -1;
		}

		if (session->status == ISSL_HANDSHAKING)
		{
			// The handshake isn't finished and it wants to read, try to finish it.
			if (!Handshake(user, session))
			{
				// Couldn't resume handshake.
				if (session->status == ISSL_NONE)
					return -1;
				return 0;
			}
		}

		// If we resumed the handshake then session->status will be ISSL_OPEN

		if (session->status == ISSL_OPEN)
		{
			char* buffer = ServerInstance->GetReadBuffer();
			size_t bufsiz = ServerInstance->Config->NetBufferSize;
			int ret = SSL_read(session->sess, buffer, bufsiz);
			
			if (ret > 0)
			{
				recvq.append(buffer, ret);
				return 1;
			}
			else if (ret == 0)
			{
				// Client closed connection.
				CloseSession(session);
				return -1;
			}
			else if (ret < 0)
			{
				int err = SSL_get_error(session->sess, ret);

				if (err == SSL_ERROR_WANT_READ)
				{
					ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ);
					return 0;
				}
				else if (err == SSL_ERROR_WANT_WRITE)
				{
					ServerInstance->SE->ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
					return 0;
				}
				else
				{
					CloseSession(session);
					return -1;
				}
			}
		}

		return 0;
	}

	int OnStreamSocketWrite(StreamSocket* user, std::string& buffer)
	{
		int fd = user->GetFd();

		issl_session* session = &sessions[fd];

		if (!session->sess)
		{
			CloseSession(session);
			return -1;
		}

		session->data_to_write = true;

		if (session->status == ISSL_HANDSHAKING)
		{
			if (!Handshake(user, session))
			{
				// Couldn't resume handshake.
				if (session->status == ISSL_NONE)
					return -1;
				return 0;
			}
		}

		if (session->status == ISSL_OPEN)
		{
			int ret = SSL_write(session->sess, buffer.data(), buffer.size());
			if (ret == (int)buffer.length())
			{
				session->data_to_write = false;
				ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				return 1;
			}
			else if (ret > 0)
			{
				buffer = buffer.substr(ret);
				ServerInstance->SE->ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else if (ret == 0)
			{
				CloseSession(session);
				return -1;
			}
			else if (ret < 0)
			{
				int err = SSL_get_error(session->sess, ret);

				if (err == SSL_ERROR_WANT_WRITE)
				{
					ServerInstance->SE->ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
					return 0;
				}
				else if (err == SSL_ERROR_WANT_READ)
				{
					ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
					return 0;
				}
				else
				{
					CloseSession(session);
					return -1;
				}
			}
		}
		return 0;
	}

	bool Handshake(EventHandler* user, issl_session* session)
	{
		int ret;

		if (session->outbound)
			ret = SSL_connect(session->sess);
		else
			ret = SSL_accept(session->sess);

		if (ret < 0)
		{
			int err = SSL_get_error(session->sess, ret);

			if (err == SSL_ERROR_WANT_READ)
			{
				ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				session->status = ISSL_HANDSHAKING;
				return true;
			}
			else if (err == SSL_ERROR_WANT_WRITE)
			{
				ServerInstance->SE->ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
				session->status = ISSL_HANDSHAKING;
				return true;
			}
			else
			{
				CloseSession(session);
			}

			return false;
		}
		else if (ret > 0)
		{
			// Handshake complete.
			VerifyCertificate(session, user);

			session->status = ISSL_OPEN;

			ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

			return true;
		}
		else if (ret == 0)
		{
			CloseSession(session);
			return true;
		}

		return true;
	}

	void CloseSession(issl_session* session)
	{
		if (session->sess)
		{
			SSL_shutdown(session->sess);
			SSL_free(session->sess);
		}

		session->sess = NULL;
		session->status = ISSL_NONE;
		errno = EIO;
	}

	void VerifyCertificate(issl_session* session, Extensible* user)
	{
		if (!session->sess || !user)
			return;

		Module* sslinfo = ServerInstance->Modules->Find("m_sslinfo.so");
		if (!sslinfo)
			return;

		X509* cert;
		ssl_cert* certinfo = new ssl_cert;
		unsigned int n;
		unsigned char md[EVP_MAX_MD_SIZE];
		const EVP_MD *digest = EVP_md5();

		cert = SSL_get_peer_certificate((SSL*)session->sess);

		if (!cert)
		{
			certinfo->error = "Could not get peer certificate: "+std::string(get_error());
			SSLCertSubmission(user, this, sslinfo, certinfo);
			return;
		}

		certinfo->invalid = (SSL_get_verify_result(session->sess) != X509_V_OK);

		if (SelfSigned)
		{
			certinfo->unknownsigner = false;
			certinfo->trusted = true;
		}
		else
		{
			certinfo->unknownsigner = true;
			certinfo->trusted = false;
		}

		certinfo->dn = X509_NAME_oneline(X509_get_subject_name(cert),0,0);
		certinfo->issuer = X509_NAME_oneline(X509_get_issuer_name(cert),0,0);

		if (!X509_digest(cert, digest, md, &n))
		{
			certinfo->error = "Out of memory generating fingerprint";
		}
		else
		{
			certinfo->fingerprint = irc::hex(md, n);
		}

		if ((ASN1_UTCTIME_cmp_time_t(X509_get_notAfter(cert), ServerInstance->Time()) == -1) || (ASN1_UTCTIME_cmp_time_t(X509_get_notBefore(cert), ServerInstance->Time()) == 0))
		{
			certinfo->error = "Not activated, or expired certificate";
		}

		X509_free(cert);
		SSLCertSubmission(user, this, sslinfo, certinfo);
	}

	void Prioritize()
	{
		Module* server = ServerInstance->Modules->Find("m_spanningtree.so");
		ServerInstance->Modules->SetPriority(this, I_OnPostConnect, PRIORITY_AFTER, &server);
	}

};

static int error_callback(const char *str, size_t len, void *u)
{
	ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "SSL error: " + std::string(str, len - 1));

	//
	// XXX: Remove this line, it causes valgrind warnings...
	//
	// MD_update(&m, buf, j);
	//
	//
	// ... ONLY JOKING! :-)
	//

	return 0;
}

MODULE_INIT(ModuleSSLOpenSSL)
