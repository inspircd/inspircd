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
#include "iohook.h"
#include "modules/ssl.h"

// Ignore OpenSSL deprecation warnings on OS X Lion and newer.
#if defined __APPLE__
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef _WIN32
# pragma comment(lib, "libcrypto.lib")
# pragma comment(lib, "libssl.lib")
# pragma comment(lib, "user32.lib")
# pragma comment(lib, "advapi32.lib")
# pragma comment(lib, "libgcc.lib")
# pragma comment(lib, "libmingwex.lib")
# pragma comment(lib, "gdi32.lib")
#endif

/* $CompileFlags: pkgconfversion("openssl","0.9.7") pkgconfincludes("openssl","/openssl/ssl.h","") */
/* $LinkerFlags: rpath("pkg-config --libs openssl") pkgconflibs("openssl","/libssl.so","-lssl -lcrypto") */

enum issl_status { ISSL_NONE, ISSL_HANDSHAKING, ISSL_OPEN };

static bool SelfSigned = false;

char* get_error()
{
	return ERR_error_string(ERR_get_error(), NULL);
}

static int OnVerify(int preverify_ok, X509_STORE_CTX* ctx);

namespace OpenSSL
{
	class Exception : public ModuleException
	{
	 public:
		Exception(const std::string& reason)
			: ModuleException(reason) { }
	};

	class DHParams
	{
		DH* dh;

	 public:
		DHParams(const std::string& filename)
		{
			FILE* dhpfile = fopen(filename.c_str(), "r");
			if (dhpfile == NULL)
				throw Exception("Couldn't open DH file " + filename + ": " + strerror(errno));

			dh = PEM_read_DHparams(dhpfile, NULL, NULL, NULL);
			fclose(dhpfile);
			if (!dh)
				throw Exception("Couldn't read DH params from file " + filename);
		}

		~DHParams()
		{
			DH_free(dh);
		}

		DH* get()
		{
			return dh;
		}
	};

	class Context
	{
		SSL_CTX* const ctx;

	 public:
		Context(SSL_CTX* context)
			: ctx(context)
		{
			SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
			SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, OnVerify);

			const unsigned char session_id[] = "inspircd";
			SSL_CTX_set_session_id_context(ctx, session_id, sizeof(session_id) - 1);
		}

		~Context()
		{
			SSL_CTX_free(ctx);
		}

		bool SetDH(DHParams& dh)
		{
			return (SSL_CTX_set_tmp_dh(ctx, dh.get()) >= 0);
		}

		bool SetCiphers(const std::string& ciphers)
		{
			return SSL_CTX_set_cipher_list(ctx, ciphers.c_str());
		}

		bool SetCerts(const std::string& filename)
		{
			return SSL_CTX_use_certificate_chain_file(ctx, filename.c_str());
		}

		bool SetPrivateKey(const std::string& filename)
		{
			return SSL_CTX_use_PrivateKey_file(ctx, filename.c_str(), SSL_FILETYPE_PEM);
		}

		bool SetCA(const std::string& filename)
		{
			return SSL_CTX_load_verify_locations(ctx, filename.c_str(), 0);
		}

		SSL* CreateSession()
		{
			return SSL_new(ctx);
		}
	};

	class Profile : public refcountbase
	{
		/** Name of this profile
		 */
		const std::string name;

		/** DH parameters in use
		 */
		DHParams dh;

		/** OpenSSL makes us have two contexts, one for servers and one for clients
		 */
		Context ctx;
		Context clictx;

		/** Digest to use when generating fingerprints
		 */
		const EVP_MD* digest;

		/** Last error, set by error_callback()
		 */
		std::string lasterr;

		static int error_callback(const char* str, size_t len, void* u)
		{
			Profile* profile = reinterpret_cast<Profile*>(u);
			profile->lasterr = std::string(str, len - 1);
			return 0;
		}

	 public:
		Profile(const std::string& profilename, ConfigTag* tag)
			: name(profilename)
			, dh(ServerInstance->Config->Paths.PrependConfig(tag->getString("dhfile", "dh.pem")))
			, ctx(SSL_CTX_new(SSLv23_server_method()))
			, clictx(SSL_CTX_new(SSLv23_client_method()))
		{
			if ((!ctx.SetDH(dh)) || (!clictx.SetDH(dh)))
				throw Exception("Couldn't set DH parameters");

			std::string hash = tag->getString("hash", "md5");
			digest = EVP_get_digestbyname(hash.c_str());
			if (digest == NULL)
				throw Exception("Unknown hash type " + hash);

			std::string ciphers = tag->getString("ciphers");
			if (!ciphers.empty())
			{
				if ((!ctx.SetCiphers(ciphers)) || (!clictx.SetCiphers(ciphers)))
				{
					ERR_print_errors_cb(error_callback, this);
					throw Exception("Can't set cipher list to \"" + ciphers + "\" " + lasterr);
				}
			}

			/* Load our keys and certificates
			 * NOTE: OpenSSL's error logging API sucks, don't blame us for this clusterfuck.
			 */
			std::string filename = ServerInstance->Config->Paths.PrependConfig(tag->getString("certfile", "cert.pem"));
			if ((!ctx.SetCerts(filename)) || (!clictx.SetCerts(filename)))
			{
				ERR_print_errors_cb(error_callback, this);
				throw Exception("Can't read certificate file: " + lasterr);
			}

			filename = ServerInstance->Config->Paths.PrependConfig(tag->getString("keyfile", "key.pem"));
			if ((!ctx.SetPrivateKey(filename)) || (!clictx.SetPrivateKey(filename)))
			{
				ERR_print_errors_cb(error_callback, this);
				throw Exception("Can't read key file: " + lasterr);
			}

			// Load the CAs we trust
			filename = ServerInstance->Config->Paths.PrependConfig(tag->getString("cafile", "ca.pem"));
			if ((!ctx.SetCA(filename)) || (!clictx.SetCA(filename)))
			{
				ERR_print_errors_cb(error_callback, this);
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Can't read CA list from %s. This is only a problem if you want to verify client certificates, otherwise it's safe to ignore this message. Error: %s", filename.c_str(), lasterr.c_str());
			}
		}

		const std::string& GetName() const { return name; }
		SSL* CreateServerSession() { return ctx.CreateSession(); }
		SSL* CreateClientSession() { return clictx.CreateSession(); }
		const EVP_MD* GetDigest() { return digest; }
	};
}

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
	SSL* sess;
	issl_status status;
	const bool outbound;
	bool data_to_write;
	reference<OpenSSL::Profile> profile;

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
				SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				this->status = ISSL_HANDSHAKING;
				return true;
			}
			else if (err == SSL_ERROR_WANT_WRITE)
			{
				SocketEngine::ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
				this->status = ISSL_HANDSHAKING;
				return true;
			}
			else
			{
				CloseSession();
			}

			return false;
		}
		else if (ret > 0)
		{
			// Handshake complete.
			VerifyCertificate();

			status = ISSL_OPEN;

			SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

			return true;
		}
		else if (ret == 0)
		{
			CloseSession();
			return true;
		}

		return true;
	}

	void CloseSession()
	{
		if (sess)
		{
			SSL_shutdown(sess);
			SSL_free(sess);
		}
		sess = NULL;
		certificate = NULL;
		status = ISSL_NONE;
		errno = EIO;
	}

	void VerifyCertificate()
	{
		X509* cert;
		ssl_cert* certinfo = new ssl_cert;
		this->certificate = certinfo;
		unsigned int n;
		unsigned char md[EVP_MAX_MD_SIZE];

		cert = SSL_get_peer_certificate(sess);

		if (!cert)
		{
			certinfo->error = "Could not get peer certificate: "+std::string(get_error());
			return;
		}

		certinfo->invalid = (SSL_get_verify_result(sess) != X509_V_OK);

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
		X509_NAME_oneline(X509_get_issuer_name(cert), buf, sizeof(buf));
		certinfo->issuer = buf;

		if (!X509_digest(cert, profile->GetDigest(), md, &n))
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
	OpenSSLIOHook(IOHookProvider* hookprov, StreamSocket* sock, bool is_outbound, SSL* session, const reference<OpenSSL::Profile>& sslprofile)
		: SSLIOHook(hookprov)
		, sess(session)
		, status(ISSL_NONE)
		, outbound(is_outbound)
		, data_to_write(false)
		, profile(sslprofile)
	{
		if (sess == NULL)
			return;
		if (SSL_set_fd(sess, sock->GetFd()) == 0)
			throw ModuleException("Can't set fd with SSL_set_fd: " + ConvToStr(sock->GetFd()));

		sock->AddIOHook(this);
		Handshake(sock);
	}

	void OnStreamSocketClose(StreamSocket* user) CXX11_OVERRIDE
	{
		CloseSession();
	}

	int OnStreamSocketRead(StreamSocket* user, std::string& recvq) CXX11_OVERRIDE
	{
		if (!sess)
		{
			CloseSession();
			return -1;
		}

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

		// If we resumed the handshake then this->status will be ISSL_OPEN

		if (status == ISSL_OPEN)
		{
			char* buffer = ServerInstance->GetReadBuffer();
			size_t bufsiz = ServerInstance->Config->NetBufferSize;
			int ret = SSL_read(sess, buffer, bufsiz);

			if (ret > 0)
			{
				recvq.append(buffer, ret);
				if (data_to_write)
					SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_SINGLE_WRITE);
				return 1;
			}
			else if (ret == 0)
			{
				// Client closed connection.
				CloseSession();
				user->SetError("Connection closed");
				return -1;
			}
			else if (ret < 0)
			{
				int err = SSL_get_error(sess, ret);

				if (err == SSL_ERROR_WANT_READ)
				{
					SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ);
					return 0;
				}
				else if (err == SSL_ERROR_WANT_WRITE)
				{
					SocketEngine::ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
					return 0;
				}
				else
				{
					CloseSession();
					return -1;
				}
			}
		}

		return 0;
	}

	int OnStreamSocketWrite(StreamSocket* user, std::string& buffer) CXX11_OVERRIDE
	{
		if (!sess)
		{
			CloseSession();
			return -1;
		}

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
				SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				return 1;
			}
			else if (ret > 0)
			{
				buffer = buffer.substr(ret);
				SocketEngine::ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else if (ret == 0)
			{
				CloseSession();
				return -1;
			}
			else if (ret < 0)
			{
				int err = SSL_get_error(sess, ret);

				if (err == SSL_ERROR_WANT_WRITE)
				{
					SocketEngine::ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
					return 0;
				}
				else if (err == SSL_ERROR_WANT_READ)
				{
					SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ);
					return 0;
				}
				else
				{
					CloseSession();
					return -1;
				}
			}
		}
		return 0;
	}

	void TellCiphersAndFingerprint(LocalUser* user)
	{
		if (sess)
		{
			std::string text = "*** You are connected using SSL cipher '" + std::string(SSL_get_cipher(sess)) + "'";
			const std::string& fingerprint = certificate->fingerprint;
			if (!fingerprint.empty())
				text += " and your SSL fingerprint is " + fingerprint;

			user->WriteNotice(text);
		}
	}
};

class OpenSSLIOHookProvider : public refcountbase, public IOHookProvider
{
	reference<OpenSSL::Profile> profile;

 public:
	OpenSSLIOHookProvider(Module* mod, reference<OpenSSL::Profile>& prof)
		: IOHookProvider(mod, "ssl/" + prof->GetName(), IOHookProvider::IOH_SSL)
		, profile(prof)
	{
		ServerInstance->Modules->AddService(*this);
	}

	~OpenSSLIOHookProvider()
	{
		ServerInstance->Modules->DelService(*this);
	}

	void OnAccept(StreamSocket* sock, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		new OpenSSLIOHook(this, sock, false, profile->CreateServerSession(), profile);
	}

	void OnConnect(StreamSocket* sock) CXX11_OVERRIDE
	{
		new OpenSSLIOHook(this, sock, true, profile->CreateClientSession(), profile);
	}
};

class ModuleSSLOpenSSL : public Module
{
	typedef std::vector<reference<OpenSSLIOHookProvider> > ProfileList;

	ProfileList profiles;

	void ReadProfiles()
	{
		ProfileList newprofiles;
		ConfigTagList tags = ServerInstance->Config->ConfTags("sslprofile");
		if (tags.first == tags.second)
		{
			// Create a default profile named "openssl"
			const std::string defname = "openssl";
			ConfigTag* tag = ServerInstance->Config->ConfValue(defname);
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "No <sslprofile> tags found, using settings from the <openssl> tag");

			try
			{
				reference<OpenSSL::Profile> profile(new OpenSSL::Profile(defname, tag));
				newprofiles.push_back(new OpenSSLIOHookProvider(this, profile));
			}
			catch (OpenSSL::Exception& ex)
			{
				throw ModuleException("Error while initializing the default SSL profile - " + ex.GetReason());
			}
		}

		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			if (tag->getString("provider") != "openssl")
				continue;

			std::string name = tag->getString("name");
			if (name.empty())
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Ignoring <sslprofile> tag without name at " + tag->getTagLocation());
				continue;
			}

			reference<OpenSSL::Profile> profile;
			try
			{
				profile = new OpenSSL::Profile(name, tag);
			}
			catch (CoreException& ex)
			{
				throw ModuleException("Error while initializing SSL profile \"" + name + "\" at " + tag->getTagLocation() + " - " + ex.GetReason());
			}

			newprofiles.push_back(new OpenSSLIOHookProvider(this, profile));
		}

		profiles.swap(newprofiles);
	}

 public:
	ModuleSSLOpenSSL()
	{
		// Initialize OpenSSL
		SSL_library_init();
		SSL_load_error_strings();
	}

	void init() CXX11_OVERRIDE
	{
		ReadProfiles();
	}

	void OnModuleRehash(User* user, const std::string &param) CXX11_OVERRIDE
	{
		if (param != "ssl")
			return;

		try
		{
			ReadProfiles();
		}
		catch (ModuleException& ex)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, ex.GetReason() + " Not applying settings.");
		}
	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		IOHook* hook = user->eh.GetIOHook();
		if (hook && hook->prov->creator == this)
			static_cast<OpenSSLIOHook*>(hook)->TellCiphersAndFingerprint(user);
	}

	void OnCleanup(int target_type, void* item) CXX11_OVERRIDE
	{
		if (target_type == TYPE_USER)
		{
			LocalUser* user = IS_LOCAL((User*)item);

			if (user && user->eh.GetIOHook() && user->eh.GetIOHook()->prov->creator == this)
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

MODULE_INIT(ModuleSSLOpenSSL)
