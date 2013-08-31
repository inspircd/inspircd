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
#include "iohook.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "modules/ssl.h"

#ifdef _WIN32
# pragma comment(lib, "libcrypto.lib")
# pragma comment(lib, "libssl.lib")
# pragma comment(lib, "user32.lib")
# pragma comment(lib, "advapi32.lib")
# pragma comment(lib, "libgcc.lib")
# pragma comment(lib, "libmingwex.lib")
# pragma comment(lib, "gdi32.lib")
# undef MAX_DESCRIPTORS
# define MAX_DESCRIPTORS 10000
#endif

/* $CompileFlags: pkgconfversion("openssl","0.9.7") pkgconfincludes("openssl","/openssl/ssl.h","") -Wno-pedantic */
/* $LinkerFlags: rpath("pkg-config --libs openssl") pkgconflibs("openssl","/libssl.so","-lssl -lcrypto") */

enum issl_status { ISSL_NONE, ISSL_HANDSHAKING, ISSL_OPEN };

static bool SelfSigned = false;

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

class OpenSSLIOHook : public SSLIOHook
{
 private:
	bool Handshake(StreamSocket* user, issl_session* session)
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

	void VerifyCertificate(issl_session* session, StreamSocket* user)
	{
		if (!session->sess || !user)
			return;

		X509* cert;
		ssl_cert* certinfo = new ssl_cert;
		session->cert = certinfo;
		unsigned int n;
		unsigned char md[EVP_MAX_MD_SIZE];

		cert = SSL_get_peer_certificate((SSL*)session->sess);

		if (!cert)
		{
			certinfo->error = "Could not get peer certificate: "+std::string(get_error());
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
			certinfo->fingerprint = BinToHex(md, n);
		}

		if ((ASN1_UTCTIME_cmp_time_t(X509_get_notAfter(cert), ServerInstance->Time()) == -1) || (ASN1_UTCTIME_cmp_time_t(X509_get_notBefore(cert), ServerInstance->Time()) == 0))
		{
			certinfo->error = "Not activated, or expired certificate";
		}

		X509_free(cert);
	}

 public:
	issl_session* sessions;
	SSL_CTX* ctx;
	SSL_CTX* clictx;
	const EVP_MD *digest;

	OpenSSLIOHook(Module* mod)
		: SSLIOHook(mod, "ssl/openssl")
	{
		sessions = new issl_session[ServerInstance->SE->GetMaxFds()];
	}

	~OpenSSLIOHook()
	{
		delete[] sessions;
	}

	void OnStreamSocketAccept(StreamSocket* user, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		int fd = user->GetFd();

		issl_session* session = &sessions[fd];

		session->sess = SSL_new(ctx);
		session->status = ISSL_NONE;
		session->outbound = false;
		session->cert = NULL;

		if (session->sess == NULL)
			return;

		if (SSL_set_fd(session->sess, fd) == 0)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "BUG: Can't set fd with SSL_set_fd: %d", fd);
			return;
		}

 		Handshake(user, session);
	}

	void OnStreamSocketConnect(StreamSocket* user) CXX11_OVERRIDE
	{
		int fd = user->GetFd();
		/* Are there any possibilities of an out of range fd? Hope not, but lets be paranoid */
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds() -1))
			return;

		issl_session* session = &sessions[fd];

		session->sess = SSL_new(clictx);
		session->status = ISSL_NONE;
		session->outbound = true;

		if (session->sess == NULL)
			return;

		if (SSL_set_fd(session->sess, fd) == 0)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "BUG: Can't set fd with SSL_set_fd: %d", fd);
			return;
		}

		Handshake(user, session);
	}

	void OnStreamSocketClose(StreamSocket* user) CXX11_OVERRIDE
	{
		int fd = user->GetFd();
		/* Are there any possibilities of an out of range fd? Hope not, but lets be paranoid */
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds() - 1))
			return;

		CloseSession(&sessions[fd]);
	}

	int OnStreamSocketRead(StreamSocket* user, std::string& recvq) CXX11_OVERRIDE
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
				if (session->data_to_write)
					ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_SINGLE_WRITE);
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

	int OnStreamSocketWrite(StreamSocket* user, std::string& buffer) CXX11_OVERRIDE
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

	ssl_cert* GetCertificate(StreamSocket* sock) CXX11_OVERRIDE
	{
		int fd = sock->GetFd();
		issl_session* session = &sessions[fd];
		return session->cert;
	}

	void TellCiphersAndFingerprint(LocalUser* user)
	{
		issl_session& s = sessions[user->eh.GetFd()];
		if (s.sess)
		{
			std::string text = "*** You are connected using SSL cipher '" + std::string(SSL_get_cipher(s.sess)) + "'";
			const std::string& fingerprint = s.cert->fingerprint;
			if (!fingerprint.empty())
				text += " and your SSL fingerprint is " + fingerprint;

			user->WriteNotice(text);
		}
	}
};

class ModuleSSLOpenSSL : public Module
{
	std::string sslports;
	OpenSSLIOHook iohook;

 public:
	ModuleSSLOpenSSL() : iohook(this)
	{
		/* Global SSL library initialization*/
		SSL_library_init();
		SSL_load_error_strings();

		/* Build our SSL contexts:
		 * NOTE: OpenSSL makes us have two contexts, one for servers and one for clients. ICK.
		 */
		iohook.ctx = SSL_CTX_new( SSLv23_server_method() );
		iohook.clictx = SSL_CTX_new( SSLv23_client_method() );

		SSL_CTX_set_mode(iohook.ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
		SSL_CTX_set_mode(iohook.clictx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

		SSL_CTX_set_verify(iohook.ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, OnVerify);
		SSL_CTX_set_verify(iohook.clictx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, OnVerify);
	}

	~ModuleSSLOpenSSL()
	{
		SSL_CTX_free(iohook.ctx);
		SSL_CTX_free(iohook.clictx);
	}

	void init() CXX11_OVERRIDE
	{
		// Needs the flag as it ignores a plain /rehash
		OnModuleRehash(NULL,"ssl");
		ServerInstance->Modules->AddService(iohook);
	}

	void OnHookIO(StreamSocket* user, ListenSocket* lsb) CXX11_OVERRIDE
	{
		if (!user->GetIOHook() && lsb->bind_tag->getString("ssl") == "openssl")
		{
			/* Hook the user with our module */
			user->AddIOHook(&iohook);
		}
	}

	void OnRehash(User* user) CXX11_OVERRIDE
	{
		sslports.clear();

		ConfigTag* Conf = ServerInstance->Config->ConfValue("openssl");

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
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Enabling SSL for port %s", portid.c_str());

				if (port->bind_tag->getString("type", "clients") == "clients" && port->bind_addr != "127.0.0.1")
				{
					/*
					 * Found an SSL port for clients that is not bound to 127.0.0.1 and handled by us, display
					 * the IP:port in ISUPPORT.
					 *
					 * We used to advertise all ports separated by a ';' char that matched the above criteria,
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

	void OnModuleRehash(User* user, const std::string &param) CXX11_OVERRIDE
	{
		if (param != "ssl")
			return;

		std::string keyfile;
		std::string certfile;
		std::string cafile;
		std::string dhfile;
		OnRehash(user);

		ConfigTag* conf = ServerInstance->Config->ConfValue("openssl");

		cafile	 = ServerInstance->Config->Paths.PrependConfig(conf->getString("cafile", "ca.pem"));
		certfile = ServerInstance->Config->Paths.PrependConfig(conf->getString("certfile", "cert.pem"));
		keyfile	 = ServerInstance->Config->Paths.PrependConfig(conf->getString("keyfile", "key.pem"));
		dhfile	 = ServerInstance->Config->Paths.PrependConfig(conf->getString("dhfile", "dhparams.pem"));
		std::string hash = conf->getString("hash", "md5");

		iohook.digest = EVP_get_digestbyname(hash.c_str());
		if (iohook.digest == NULL)
			throw ModuleException("Unknown hash type " + hash);

		std::string ciphers = conf->getString("ciphers", "");

		SSL_CTX* ctx = iohook.ctx;
		SSL_CTX* clictx = iohook.clictx;

		if (!ciphers.empty())
		{
			if ((!SSL_CTX_set_cipher_list(ctx, ciphers.c_str())) || (!SSL_CTX_set_cipher_list(clictx, ciphers.c_str())))
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Can't set cipher list to %s.", ciphers.c_str());
				ERR_print_errors_cb(error_callback, this);
			}
		}

		/* Load our keys and certificates
		 * NOTE: OpenSSL's error logging API sucks, don't blame us for this clusterfuck.
		 */
		if ((!SSL_CTX_use_certificate_chain_file(ctx, certfile.c_str())) || (!SSL_CTX_use_certificate_chain_file(clictx, certfile.c_str())))
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Can't read certificate file %s. %s", certfile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		if (((!SSL_CTX_use_PrivateKey_file(ctx, keyfile.c_str(), SSL_FILETYPE_PEM))) || (!SSL_CTX_use_PrivateKey_file(clictx, keyfile.c_str(), SSL_FILETYPE_PEM)))
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Can't read key file %s. %s", keyfile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		/* Load the CAs we trust*/
		if (((!SSL_CTX_load_verify_locations(ctx, cafile.c_str(), 0))) || (!SSL_CTX_load_verify_locations(clictx, cafile.c_str(), 0)))
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Can't read CA list from %s. This is only a problem if you want to verify client certificates, otherwise it's safe to ignore this message. Error: %s", cafile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		FILE* dhpfile = fopen(dhfile.c_str(), "r");
		DH* ret;

		if (dhpfile == NULL)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Couldn't open DH file %s: %s", dhfile.c_str(), strerror(errno));
			throw ModuleException("Couldn't open DH file " + dhfile + ": " + strerror(errno));
		}
		else
		{
			ret = PEM_read_DHparams(dhpfile, NULL, NULL, NULL);
			if ((SSL_CTX_set_tmp_dh(ctx, ret) < 0) || (SSL_CTX_set_tmp_dh(clictx, ret) < 0))
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Couldn't set DH parameters %s. SSL errors follow:", dhfile.c_str());
				ERR_print_errors_cb(error_callback, this);
			}
		}

		fclose(dhpfile);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		if (!sslports.empty())
			tokens["SSL"] = sslports;
	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		if (user->eh.GetIOHook() == &iohook)
			iohook.TellCiphersAndFingerprint(user);
	}

	void OnCleanup(int target_type, void* item) CXX11_OVERRIDE
	{
		if (target_type == TYPE_USER)
		{
			LocalUser* user = IS_LOCAL((User*)item);

			if (user && user->eh.GetIOHook() == &iohook)
			{
				// User is using SSL, they're a local user, and they're using one of *our* SSL ports.
				// Potentially there could be multiple SSL modules loaded at once on different ports.
				ServerInstance->Users->QuitUser(user, "SSL module unloading");
			}
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides SSL support for clients", VF_VENDOR);
	}
};

static int error_callback(const char *str, size_t len, void *u)
{
	ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "SSL error: " + std::string(str, len - 1));

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
