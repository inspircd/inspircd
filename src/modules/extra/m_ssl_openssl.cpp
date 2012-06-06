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


#include "inspircd.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ssl.h"

#ifdef WINDOWS
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

/* $ModDesc: Provides SSL support for clients */

/* $LinkerFlags: if("USE_FREEBSD_BASE_SSL") -lssl -lcrypto */
/* $CompileFlags: if(!"USE_FREEBSD_BASE_SSL") pkgconfversion("openssl","0.9.7") pkgconfincludes("openssl","/openssl/ssl.h","") */
/* $LinkerFlags: if(!"USE_FREEBSD_BASE_SSL") rpath("pkg-config --libs openssl") pkgconflibs("openssl","/libssl.so","-lssl -lcrypto -ldl") */

/* $NoPedantic */


enum issl_status { ISSL_NONE, ISSL_HANDSHAKING, ISSL_OPEN };

static bool SelfSigned = false;
static bool use_sha;

static char* get_error()
{
	return ERR_error_string(ERR_get_error(), NULL);
}

static int error_callback(const char *str, size_t len, void *u);

static int OnVerify(int preverify_ok, X509_STORE_CTX *store_ctx)
{
	/* XXX: This will allow self signed certificates.
	 * In the future if we want an option to not allow this,
	 * we can just return preverify_ok here, and openssl
	 * will boot off self-signed and invalid peer certs.
	 */
	int ve = X509_STORE_CTX_get_error(store_ctx);

	SelfSigned = (ve == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT);

	return 1;
}

/** Represents an SSL user's extra data
 */
class OSSLHook : public SSLIOHook
{
 public:
	SSL* sess;
	issl_status status;
	reference<ssl_cert> cert;

	int fd;
	bool outbound;
	bool data_to_write;

	OSSLHook(Module* Creator, bool client, StreamSocket* user, SSL_CTX* ctx) : SSLIOHook(Creator)
	{
		outbound = false;
		data_to_write = false;
		fd = user->GetFd();
		sess = SSL_new(ctx);
		status = ISSL_NONE;
		outbound = client;
		if (!sess)
			return;
		if (SSL_set_fd(sess, fd) == 0)
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEBUG,"BUG: Can't set fd with SSL_set_fd: %d", fd);
		}
		user->SetIOHook(this);
	}
	
	int OnRead(StreamSocket* user, std::string& recvq)
	{
		if (!sess)
			return -1;

		if (status == ISSL_HANDSHAKING)
		{
			// The handshake isn't finished and it wants to read, try to finish it.
			if (!Handshake(user))
			{
				// Couldn't resume handshake.
				if (status == ISSL_NONE)
					return -1;
				return 0;
			}
		}

		// If we resumed the handshake then status will be ISSL_OPEN

		if (status == ISSL_OPEN)
		{
			char* buffer = ServerInstance->GetReadBuffer();
			size_t bufsiz = ServerInstance->Config->NetBufferSize;
			int ret = SSL_read(sess, buffer, bufsiz);

			if (ret > 0)
			{
				recvq.append(buffer, ret);
				return 1;
			}
			else if (ret == 0)
			{
				// Client closed connection.
				OnClose(user);
				return -1;
			}
			else if (ret < 0)
			{
				int err = SSL_get_error(sess, ret);

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
					OnClose(user);
					return -1;
				}
			}
		}

		return 0;
	}

	int OnWrite(StreamSocket* user, std::string& buffer)
	{
		if (!sess)
			return -1;

		data_to_write = true;

		if (status == ISSL_HANDSHAKING)
		{
			if (!Handshake(user))
			{
				// Couldn't resume handshake.
				if (status == ISSL_NONE)
					return -1;
				return 0;
			}
		}

		if (status == ISSL_OPEN)
		{
			int ret = SSL_write(sess, buffer.data(), buffer.size());
			if (ret == (int)buffer.length())
			{
				data_to_write = false;
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
				OnClose(user);
				return -1;
			}
			else if (ret < 0)
			{
				int err = SSL_get_error(sess, ret);

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
					OnClose(user);
					return -1;
				}
			}
		}
		return 0;
	}

	bool Handshake(StreamSocket* user)
	{
		int ret;

		if (outbound)
			ret = SSL_connect(sess);
		else
			ret = SSL_accept(sess);

		if (ret < 0)
		{
			int err = SSL_get_error(sess, ret);

			if (err == SSL_ERROR_WANT_READ)
			{
				ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				status = ISSL_HANDSHAKING;
				return true;
			}
			else if (err == SSL_ERROR_WANT_WRITE)
			{
				ServerInstance->SE->ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
				status = ISSL_HANDSHAKING;
				return true;
			}
			else
			{
				OnClose(user);
			}

			return false;
		}
		else if (ret > 0)
		{
			// Handshake complete.
			VerifyCertificate(user);

			status = ISSL_OPEN;

			ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

			return true;
		}
		else if (ret == 0)
		{
			OnClose(user);
			return true;
		}

		return true;
	}

	void OnClose(StreamSocket* user)
	{
		if (sess)
		{
			SSL_shutdown(sess);
			SSL_free(sess);
		}

		sess = NULL;
		status = ISSL_NONE;
	}

	void VerifyCertificate(StreamSocket* user)
	{
		ssl_cert* certinfo = new ssl_cert;
		cert = certinfo;
		unsigned int n;
		unsigned char md[EVP_MAX_MD_SIZE];
		const EVP_MD *digest = use_sha ? EVP_sha1() : EVP_md5();

		X509* rawcert = SSL_get_peer_certificate(sess);

		if (!rawcert)
		{
			certinfo->error = "Could not get peer certificate: "+std::string(get_error());
			return;
		}

		certinfo->invalid = (SSL_get_verify_result(sess) != X509_V_OK);

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

		certinfo->dn = X509_NAME_oneline(X509_get_subject_name(rawcert),0,0);
		certinfo->issuer = X509_NAME_oneline(X509_get_issuer_name(rawcert),0,0);

		if (!X509_digest(rawcert, digest, md, &n))
		{
			certinfo->error = "Out of memory generating fingerprint";
		}
		else
		{
			certinfo->fingerprint = irc::hex(md, n);
		}

		if ((ASN1_UTCTIME_cmp_time_t(X509_get_notAfter(rawcert), ServerInstance->Time()) == -1) || (ASN1_UTCTIME_cmp_time_t(X509_get_notBefore(rawcert), ServerInstance->Time()) == 0))
		{
			certinfo->error = "Not activated, or expired certificate";
		}

		X509_free(rawcert);
	}

	ssl_cert* GetCertificate()
	{
		return cert;
	}
};

class OSSLProvider : public IOHookProvider
{
 public:
	SSL_CTX* ctx;
	SSL_CTX* clictx;

	OSSLProvider(Module* Creator) : IOHookProvider(Creator, "ssl/openssl") {}

	~OSSLProvider()
	{
		SSL_CTX_free(ctx);
		SSL_CTX_free(clictx);
	}

	void OnClientConnection(StreamSocket* user, ConfigTag* tag)
	{
		new OSSLHook(creator, true, user, clictx);
	}
	void OnServerConnection(StreamSocket* user, ListenSocket* from)
	{
		new OSSLHook(creator, false, user, ctx);
	}
};

class ModuleSSLOpenSSL : public Module
{
	std::string sslports;

	OSSLProvider iohook;
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

	void init()
	{
		// Needs the flag as it ignores a plain /rehash
		OnModuleRehash(NULL,"ssl");
		Implementation eventlist[] = { I_On005Numeric, I_OnModuleRehash, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		ServerInstance->Modules->AddService(iohook);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		sslports.clear();

		ConfigTag* Conf = ServerInstance->Config->GetTag("openssl");
		
		if (Conf->getBool("showports", true))
		{
			for (size_t i = 0; i < ServerInstance->ports.size(); i++)
			{
				ListenSocket* port = ServerInstance->ports[i];
				if (port->bind_tag->getString("ssl") != "openssl")
					continue;

				std::string portid = port->bind_desc;
				ServerInstance->Logs->Log("m_ssl_openssl", DEFAULT, "m_ssl_openssl.so: Enabling SSL for port %s", portid.c_str());
				if (port->bind_tag->getString("type", "clients") == "clients" && port->bind_addr != "127.0.0.1")
					sslports.append(portid).append(";");
			}

			if (!sslports.empty())
				sslports.erase(sslports.end() - 1);
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

		ConfigTag* conf = ServerInstance->Config->GetTag("openssl");

		cafile	 = conf->getString("cafile", "conf/ca.pem");
		certfile = conf->getString("certfile", "conf/cert.pem");
		keyfile	 = conf->getString("keyfile", "conf/key.pem");
		dhfile	 = conf->getString("dhfile", "conf/dhparams.pem");
		std::string hash = conf->getString("hash", "md5");
		if (hash != "sha1" && hash != "md5")
			throw ModuleException("Unknown hash type " + hash);
		use_sha = (hash == "sha1");


		/* Load our keys and certificates
		 * NOTE: OpenSSL's error logging API sucks, don't blame us for this clusterfuck.
		 */
		if ((!SSL_CTX_use_certificate_chain_file(iohook.ctx, certfile.c_str())) ||
			(!SSL_CTX_use_certificate_chain_file(iohook.clictx, certfile.c_str())))
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so: Can't read certificate file %s. %s", certfile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		if (((!SSL_CTX_use_PrivateKey_file(iohook.ctx, keyfile.c_str(), SSL_FILETYPE_PEM))) ||
			(!SSL_CTX_use_PrivateKey_file(iohook.clictx, keyfile.c_str(), SSL_FILETYPE_PEM)))
		{
			ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "m_ssl_openssl.so: Can't read key file %s. %s", keyfile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		/* Load the CAs we trust*/
		if (((!SSL_CTX_load_verify_locations(iohook.ctx, cafile.c_str(), 0))) ||
			(!SSL_CTX_load_verify_locations(iohook.clictx, cafile.c_str(), 0)))
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
			if ((SSL_CTX_set_tmp_dh(iohook.ctx, ret) < 0) || (SSL_CTX_set_tmp_dh(iohook.clictx, ret) < 0))
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

	void OnUserConnect(LocalUser* user)
	{
		OSSLHook* hook = static_cast<OSSLHook*>(user->eh->GetIOHook());
		if (hook && hook->creator == this)
		{
			if (!hook->cert->fingerprint.empty())
				user->WriteServ("NOTICE %s :*** You are connected using SSL fingerprint %s",
					user->nick.c_str(), hook->cert->fingerprint.c_str());
		}
	}

	Version GetVersion()
	{
		return Version("Provides SSL support for clients", VF_VENDOR);
	}
};

static int error_callback(const char *str, size_t len, void *u)
{
	ServerInstance->Logs->Log("m_ssl_openssl",DEFAULT, "SSL error: " + std::string(str, len - 1));

	return 0;
}

MODULE_INIT(ModuleSSLOpenSSL)
