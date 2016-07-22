/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

 /* HACK: This prevents OpenSSL on OS X 10.7 and later from spewing deprecation
  * warnings for every single function call. As far as I (SaberUK) know, Apple
  * have no plans to remove OpenSSL so this warning just causes needless spam.
  */
#ifdef __APPLE__
# define __AVAILABILITYMACROS__
# define DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER
#endif
 
#include "inspircd.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ssl.h"

#ifdef _WIN32
# pragma comment(lib, "ssleay32.lib")
# pragma comment(lib, "libeay32.lib")
# undef MAX_DESCRIPTORS
# define MAX_DESCRIPTORS 10000
#endif

/* $ModDesc: Provides SSL support for clients */

/* $LinkerFlags: if("USE_FREEBSD_BASE_SSL") -lssl -lcrypto */
/* $CompileFlags: if(!"USE_FREEBSD_BASE_SSL") pkgconfversion("openssl","0.9.7") pkgconfincludes("openssl","/openssl/ssl.h","") */
/* $LinkerFlags: if(!"USE_FREEBSD_BASE_SSL") rpath("pkg-config --libs openssl") pkgconflibs("openssl","/libssl.so","-lssl -lcrypto -ldl") */

/* $NoPedantic */


class ModuleSSLOpenSSL;

enum issl_status { ISSL_NONE, ISSL_HANDSHAKING, ISSL_OPEN };

static bool SelfSigned = false;

#ifdef INSPIRCD_OPENSSL_ENABLE_RENEGO_DETECTION
static ModuleSSLOpenSSL* opensslmod = NULL;
#endif

char* get_error()
{
	return ERR_error_string(ERR_get_error(), NULL);
}

static int error_callback(const char *str, size_t len, void *u);

/** Represents an SSL user's extra data
 */
class issl_session
{
public:
	SSL* sess;
	issl_status status;
	reference<ssl_cert> cert;

	bool outbound;
	bool data_to_write;

	issl_session()
		: sess(NULL)
		, status(ISSL_NONE)
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
	issl_session* sessions;

	SSL_CTX* ctx;
	SSL_CTX* clictx;

	long ctx_options;
	long clictx_options;

	std::string sslports;
	bool use_sha;

	ServiceProvider iohook;

	static void SetContextOptions(SSL_CTX* ctx, long defoptions, const std::string& ctxname, ConfigTag* tag)
	{
		long setoptions = tag->getInt(ctxname + "setoptions");
		// User-friendly config options for setting context options
#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
		if (tag->getBool("cipherserverpref"))
			setoptions |= SSL_OP_CIPHER_SERVER_PREFERENCE;
#endif
#ifdef SSL_OP_NO_COMPRESSION
		if (!tag->getBool("compression", true))
			setoptions |= SSL_OP_NO_COMPRESSION;
#endif
		if (!tag->getBool("sslv3", true))
			setoptions |= SSL_OP_NO_SSLv3;
		if (!tag->getBool("tlsv1", true))
			setoptions |= SSL_OP_NO_TLSv1;

		long clearoptions = tag->getInt(ctxname + "clearoptions");
		ServerInstance->Logs->Log("m_ssl_openssl", DEBUG, "Setting OpenSSL %s context options, default: %ld set: %ld clear: %ld", ctxname.c_str(), defoptions, setoptions, clearoptions);

		// Clear everything
		SSL_CTX_clear_options(ctx, SSL_CTX_get_options(ctx));

		// Set the default options and what is in the conf
		SSL_CTX_set_options(ctx, defoptions | setoptions);
		long final = SSL_CTX_clear_options(ctx, clearoptions);
		ServerInstance->Logs->Log("m_ssl_openssl", DEFAULT, "OpenSSL %s context options: %ld", ctxname.c_str(), final);
	}

#ifdef INSPIRCD_OPENSSL_ENABLE_ECDH
	void SetupECDH(ConfigTag* tag)
	{
		std::string curvename = tag->getString("ecdhcurve", "prime256v1");
		if (curvename.empty())
			return;

		int nid = OBJ_sn2nid(curvename.c_str());
		if (nid == 0)
		{
			ServerInstance->Logs->Log("m_ssl_openssl", DEFAULT, "m_ssl_openssl.so: Unknown curve: \"%s\"", curvename.c_str());
			return;
		}

		EC_KEY* eckey = EC_KEY_new_by_curve_name(nid);
		if (!eckey)
		{
			ServerInstance->Logs->Log("m_ssl_openssl", DEFAULT, "m_ssl_openssl.so: Unable to create EC key object");
			return;
		}

		ERR_clear_error();
		if (SSL_CTX_set_tmp_ecdh(ctx, eckey) < 0)
		{
			ServerInstance->Logs->Log("m_ssl_openssl", DEFAULT, "m_ssl_openssl.so: Couldn't set ECDH parameters");
			ERR_print_errors_cb(error_callback, this);
		}

		EC_KEY_free(eckey);
	}
#endif

#ifdef INSPIRCD_OPENSSL_ENABLE_RENEGO_DETECTION
	static void SSLInfoCallback(const SSL* ssl, int where, int rc)
	{
		int fd = SSL_get_fd(const_cast<SSL*>(ssl));
		issl_session& session = opensslmod->sessions[fd];

		if ((where & SSL_CB_HANDSHAKE_START) && (session.status == ISSL_OPEN))
		{
			// The other side is trying to renegotiate, kill the connection and change status
			// to ISSL_NONE so CheckRenego() closes the session
			session.status = ISSL_NONE;
			ServerInstance->SE->Shutdown(fd, 2);
		}
	}

	bool CheckRenego(StreamSocket* sock, issl_session* session)
	{
		if (session->status != ISSL_NONE)
			return true;

		ServerInstance->Logs->Log("m_ssl_openssl", DEBUG, "Session %p killed, attempted to renegotiate", (void*)session->sess);
		CloseSession(session);
		sock->SetError("Renegotiation is not allowed");
		return false;
	}
#endif

 public:

	ModuleSSLOpenSSL() : iohook(this, "ssl/openssl", SERVICE_IOHOOK)
	{
#ifdef INSPIRCD_OPENSSL_ENABLE_RENEGO_DETECTION
		opensslmod = this;
#endif
		sessions = new issl_session[ServerInstance->SE->GetMaxFds()];

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

		SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
		SSL_CTX_set_session_cache_mode(clictx, SSL_SESS_CACHE_OFF);

		long opts = SSL_OP_NO_SSLv2 | SSL_OP_SINGLE_DH_USE;
		// Only turn options on if they exist
#ifdef SSL_OP_SINGLE_ECDH_USE
		opts |= SSL_OP_SINGLE_ECDH_USE;
#endif
#ifdef SSL_OP_NO_TICKET
		opts |= SSL_OP_NO_TICKET;
#endif

		ctx_options = SSL_CTX_set_options(ctx, opts);
		clictx_options = SSL_CTX_set_options(clictx, opts);
	}

	void init()
	{
		// Needs the flag as it ignores a plain /rehash
		OnModuleRehash(NULL,"ssl");
		Implementation eventlist[] = { I_On005Numeric, I_OnRehash, I_OnModuleRehash, I_OnHookIO, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		ServerInstance->Modules->AddService(iohook);
	}

	void OnHookIO(StreamSocket* user, ListenSocket* lsb)
	{
		if (!user->GetIOHook() && lsb->bind_tag->getString("ssl") == "openssl")
		{
			/* Hook the user with our module */
			user->AddIOHook(this);
		}
	}

	void OnRehash(User* user)
	{
		sslports.clear();

		ConfigTag* Conf = ServerInstance->Config->ConfValue("openssl");

#ifdef INSPIRCD_OPENSSL_ENABLE_RENEGO_DETECTION
		// Set the callback if we are not allowing renegotiations, unset it if we do
		if (Conf->getBool("renegotiation", true))
		{
			SSL_CTX_set_info_callback(ctx, NULL);
			SSL_CTX_set_info_callback(clictx, NULL);
		}
		else
		{
			SSL_CTX_set_info_callback(ctx, SSLInfoCallback);
			SSL_CTX_set_info_callback(clictx, SSLInfoCallback);
		}
#endif

		if (Conf->getBool("showports", true))
		{
			sslports = Conf->getString("advertisedports");
			if (!sslports.empty())
				return;

			for (size_t i = 0; i < ServerInstance->ports.size(); i++)
			{
				ListenSocket* port = ServerInstance->ports[i];
				if (port->bind_tag->getString("ssl") != "openssl")
					continue;

				const std::string& portid = port->bind_desc;
				ServerInstance->Logs->Log("m_ssl_openssl", DEFAULT, "m_ssl_openssl.so: Enabling SSL for port %s", portid.c_str());

				if (port->bind_tag->getString("type", "clients") == "clients" && port->bind_addr != "127.0.0.1")
				{
					/*
					 * Found an SSL port for clients that is not bound to 127.0.0.1 and handled by us, display
					 * the IP:port in ISUPPORT.
					 *
					 * We used to advertise all ports seperated by a ';' char that matched the above criteria,
					 * but this resulted in too long ISUPPORT lines if there were lots of ports to be displayed.
					 * To solve this by default we now only display the first IP:port found and let the user
					 * configure the exact value for the 005 token, if necessary.
					 */
					sslports = portid;
					break;
				}
			}
		}
	}

	void OnModuleRehash(User* user, const std::string &param)
	{
		if (param != "ssl")
			return;

		std::string keyfile;
		std::string certfile;
		std::string cafile;
		std::string dhfile;
		OnRehash(user);

		ConfigTag* conf = ServerInstance->Config->ConfValue("openssl");

		cafile	 = conf->getString("cafile", CONFIG_PATH "/ca.pem");
		certfile = conf->getString("certfile", CONFIG_PATH "/cert.pem");
		keyfile	 = conf->getString("keyfile", CONFIG_PATH "/key.pem");
		dhfile	 = conf->getString("dhfile", CONFIG_PATH "/dhparams.pem");
		std::string hash = conf->getString("hash", "md5");
		if (hash != "sha1" && hash != "md5")
			throw ModuleException("Unknown hash type " + hash);
		use_sha = (hash == "sha1");

		if (conf->getBool("customcontextoptions"))
		{
			SetContextOptions(ctx, ctx_options, "server", conf);
			SetContextOptions(clictx, clictx_options, "client", conf);
		}

		std::string ciphers = conf->getString("ciphers", "");

		if (!ciphers.empty())
		{
			ERR_clear_error();
			if ((!SSL_CTX_set_cipher_list(ctx, ciphers.c_str())) || (!SSL_CTX_set_cipher_list(clictx, ciphers.c_str())))
			{
				ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so: Can't set cipher list to %s.", ciphers.c_str());
				ERR_print_errors_cb(error_callback, this);
			}
		}

		/* Load our keys and certificates
		 * NOTE: OpenSSL's error logging API sucks, don't blame us for this clusterfuck.
		 */
		ERR_clear_error();
		if ((!SSL_CTX_use_certificate_chain_file(ctx, certfile.c_str())) || (!SSL_CTX_use_certificate_chain_file(clictx, certfile.c_str())))
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so: Can't read certificate file %s. %s", certfile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		ERR_clear_error();
		if (((!SSL_CTX_use_PrivateKey_file(ctx, keyfile.c_str(), SSL_FILETYPE_PEM))) || (!SSL_CTX_use_PrivateKey_file(clictx, keyfile.c_str(), SSL_FILETYPE_PEM)))
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so: Can't read key file %s. %s", keyfile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		/* Load the CAs we trust*/
		ERR_clear_error();
		if (((!SSL_CTX_load_verify_locations(ctx, cafile.c_str(), 0))) || (!SSL_CTX_load_verify_locations(clictx, cafile.c_str(), 0)))
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so: Can't read CA list from %s. This is only a problem if you want to verify client certificates, otherwise it's safe to ignore this message. Error: %s", cafile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

#ifdef _WIN32
		BIO* dhpfile = BIO_new_file(dhfile.c_str(), "r");
#else
		FILE* dhpfile = fopen(dhfile.c_str(), "r");
#endif
		DH* ret;

		if (dhpfile == NULL)
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so Couldn't open DH file %s: %s", dhfile.c_str(), strerror(errno));
			throw ModuleException("Couldn't open DH file " + dhfile + ": " + strerror(errno));
		}
		else
		{
#ifdef _WIN32
			ret = PEM_read_bio_DHparams(dhpfile, NULL, NULL, NULL);
			BIO_free(dhpfile);
#else
			ret = PEM_read_DHparams(dhpfile, NULL, NULL, NULL);
#endif

			ERR_clear_error();
			if (ret)
			{
				if ((SSL_CTX_set_tmp_dh(ctx, ret) < 0) || (SSL_CTX_set_tmp_dh(clictx, ret) < 0))
				{
					ServerInstance->Logs->Log("m_ssl_openssl", DEFAULT, "m_ssl_openssl.so: Couldn't set DH parameters %s. SSL errors follow:", dhfile.c_str());
					ERR_print_errors_cb(error_callback, this);
				}
				DH_free(ret);
			}
			else
			{
				ServerInstance->Logs->Log("m_ssl_openssl", DEFAULT, "m_ssl_openssl.so: Couldn't set DH parameters %s.", dhfile.c_str());
			}
		}

#ifndef _WIN32
		fclose(dhpfile);
#endif

#ifdef INSPIRCD_OPENSSL_ENABLE_ECDH
		SetupECDH(conf);
#endif
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
		delete[] sessions;
	}

	void OnUserConnect(LocalUser* user)
	{
		if (user->eh.GetIOHook() == this)
		{
			if (sessions[user->eh.GetFd()].sess)
			{
				if (!sessions[user->eh.GetFd()].cert->fingerprint.empty())
					user->WriteServ("NOTICE %s :*** You are connected using SSL cipher \"%s\""
						" and your SSL fingerprint is %s", user->nick.c_str(), SSL_get_cipher(sessions[user->eh.GetFd()].sess), sessions[user->eh.GetFd()].cert->fingerprint.c_str());
				else
					user->WriteServ("NOTICE %s :*** You are connected using SSL cipher \"%s\"", user->nick.c_str(), SSL_get_cipher(sessions[user->eh.GetFd()].sess));
			}
		}
	}

	void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			LocalUser* user = IS_LOCAL((User*)item);

			if (user && user->eh.GetIOHook() == this)
			{
				// User is using SSL, they're a local user, and they're using one of *our* SSL ports.
				// Potentially there could be multiple SSL modules loaded at once on different ports.
				ServerInstance->Users->QuitUser(user, "SSL module unloading");
			}
		}
	}

	Version GetVersion()
	{
		return Version("Provides SSL support for clients", VF_VENDOR);
	}

	void OnRequest(Request& request)
	{
		if (strcmp("GET_SSL_CERT", request.id) == 0)
		{
			SocketCertificateRequest& req = static_cast<SocketCertificateRequest&>(request);
			int fd = req.sock->GetFd();
			issl_session* session = &sessions[fd];

			req.cert = session->cert;
		}
		else if (!strcmp("GET_RAW_SSL_SESSION", request.id))
		{
			SSLRawSessionRequest& req = static_cast<SSLRawSessionRequest&>(request);
			if ((req.fd >= 0) && (req.fd < ServerInstance->SE->GetMaxFds()))
				req.data = reinterpret_cast<void*>(sessions[req.fd].sess);
		}
	}

	void OnStreamSocketAccept(StreamSocket* user, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
	{
		int fd = user->GetFd();

		issl_session* session = &sessions[fd];

		session->sess = SSL_new(ctx);
		session->status = ISSL_NONE;
		session->outbound = false;
		session->data_to_write = false;

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

		session->sess = SSL_new(clictx);
		session->status = ISSL_NONE;
		session->outbound = true;
		session->data_to_write = false;

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
			ERR_clear_error();
			char* buffer = ServerInstance->GetReadBuffer();
			size_t bufsiz = ServerInstance->Config->NetBufferSize;
			int ret = SSL_read(session->sess, buffer, bufsiz);

#ifdef INSPIRCD_OPENSSL_ENABLE_RENEGO_DETECTION
			if (!CheckRenego(user, session))
				return -1;
#endif

			if (ret > 0)
			{
				recvq.append(buffer, ret);

				int mask = 0;
				// Schedule a read if there is still data in the OpenSSL buffer
				if (SSL_pending(session->sess) > 0)
					mask |= FD_ADD_TRIAL_READ;
				if (session->data_to_write)
					mask |= FD_WANT_POLL_READ | FD_WANT_SINGLE_WRITE;
				if (mask != 0)
					ServerInstance->SE->ChangeEventMask(user, mask);
				return 1;
			}
			else if (ret == 0)
			{
				// Client closed connection.
				CloseSession(session);
				user->SetError("Connection closed");
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
			ERR_clear_error();
			int ret = SSL_write(session->sess, buffer.data(), buffer.size());

#ifdef INSPIRCD_OPENSSL_ENABLE_RENEGO_DETECTION
			if (!CheckRenego(user, session))
				return -1;
#endif

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
					ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ);
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

	bool Handshake(StreamSocket* user, issl_session* session)
	{
		int ret;

		ERR_clear_error();
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
		}
		return false;
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
		session->cert = NULL;
	}

	void VerifyCertificate(issl_session* session, StreamSocket* user)
	{
		if (!session->sess || !user)
			return;

		X509* cert;
		ssl_cert* certinfo = new ssl_cert;
		session->cert = certinfo;
		unsigned int n;
		unsigned char md[EVP_MAX_MD_SIZE];
		const EVP_MD *digest = use_sha ? EVP_sha1() : EVP_md5();

		cert = SSL_get_peer_certificate((SSL*)session->sess);

		if (!cert)
		{
			certinfo->error = "Could not get peer certificate: "+std::string(get_error());
			return;
		}

		certinfo->invalid = (SSL_get_verify_result(session->sess) != X509_V_OK);

		if (!SelfSigned)
		{
			certinfo->unknownsigner = false;
			certinfo->trusted = true;
		}
		else
		{
			certinfo->unknownsigner = true;
			certinfo->trusted = false;
		}

		char buf[512];
		X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
		certinfo->dn = buf;
		// Make sure there are no chars in the string that we consider invalid
		if (certinfo->dn.find_first_of("\r\n") != std::string::npos)
			certinfo->dn.clear();

		X509_NAME_oneline(X509_get_issuer_name(cert), buf, sizeof(buf));
		certinfo->issuer = buf;
		if (certinfo->issuer.find_first_of("\r\n") != std::string::npos)
			certinfo->issuer.clear();

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
