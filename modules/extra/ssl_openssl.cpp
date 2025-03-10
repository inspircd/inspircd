/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017, 2023 Wade Cline <wadecline@hotmail.com>
 *   Copyright (C) 2014, 2016-2017, 2019-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Julien Vehent <julien@linuxwall.info>
 *   Copyright (C) 2012-2017 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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

/// $CompilerFlags: find_compiler_flags("openssl" "")
/// $LinkerFlags: find_linker_flags("openssl" "-lssl -lcrypto")

/// $PackageInfo: require_system("alpine") openssl-dev pkgconf
/// $PackageInfo: require_system("arch") openssl pkgconf
/// $PackageInfo: require_system("darwin") openssl pkg-config
/// $PackageInfo: require_system("debian~") libssl-dev openssl pkg-config
/// $PackageInfo: require_system("rhel~") openssl-devel pkgconfig


#include "inspircd.h"
#include "iohook.h"
#include "modules/ssl.h"
#include "stringutils.h"
#include "timeutils.h"
#include "utility/string.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dh.h>

#ifdef _WIN32
# define timegm _mkgmtime
# pragma comment(lib, "libcrypto.lib")
# pragma comment(lib, "libssl.lib")
#endif

#if OPENSSL_VERSION_NUMBER < 0x30000000L
# error OpenSSL 3.0.0 or newer is required by the ssl_openssl module.
#endif

static bool SelfSigned = false;
static int exdataindex;
static Module* thismod;

char* get_error()
{
	return ERR_error_string(ERR_get_error(), nullptr);
}

static int OnVerify(int preverify_ok, X509_STORE_CTX* ctx);
static void StaticSSLInfoCallback(const SSL* ssl, int where, int rc);

namespace OpenSSL
{
	class Exception final
		: public ModuleException
	{
	public:
		Exception(const std::string& msg)
			: ModuleException(thismod, msg)
		{
		}
	};

	class Context final
	{
		SSL_CTX* const ctx;
		long ctx_options;

	public:
		Context(SSL_CTX* context)
			: ctx(context)
		{
			// Sane default options for OpenSSL see https://www.openssl.org/docs/ssl/SSL_CTX_set_options.html
			// and when choosing a cipher, use the server's preferences instead of the client preferences.
			long opts = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION | SSL_OP_CIPHER_SERVER_PREFERENCE | SSL_OP_SINGLE_DH_USE;
			// Only turn options on if they exist
#ifdef SSL_OP_SINGLE_ECDH_USE
			opts |= SSL_OP_SINGLE_ECDH_USE;
#endif
#ifdef SSL_OP_NO_TICKET
			opts |= SSL_OP_NO_TICKET;
#endif

			ctx_options = SSL_CTX_set_options(ctx, opts);

			long mode = SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER;
#ifdef SSL_MODE_RELEASE_BUFFERS
			mode |= SSL_MODE_RELEASE_BUFFERS;
#endif
			SSL_CTX_set_mode(ctx, mode);
			SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
			SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
			SSL_CTX_set_info_callback(ctx, StaticSSLInfoCallback);
		}

		~Context()
		{
			SSL_CTX_free(ctx);
		}

#ifndef OPENSSL_NO_ECDH
		void SetECDH(const std::string& curvename)
		{
			int nid = OBJ_sn2nid(curvename.c_str());
			if (nid == NID_undef)
				throw Exception("Unknown curve: " + curvename);

			ERR_clear_error();
			if (!SSL_CTX_set1_groups(ctx, &nid, 1))
				throw Exception("Couldn't set ECDH curve");
		}
#endif

		bool SetCiphers(const std::string& ciphers)
		{
			// TLSv1 to TLSv1.2 ciphers.
			ERR_clear_error();
			return SSL_CTX_set_cipher_list(ctx, ciphers.c_str());
		}

		bool SetCiphersuites(const std::string& ciphers)
		{
			// TLSv1.3+ ciphers.
			ERR_clear_error();
			return SSL_CTX_set_ciphersuites(ctx, ciphers.c_str());
		}

		bool SetCerts(const std::string& filename)
		{
			ERR_clear_error();
			return SSL_CTX_use_certificate_chain_file(ctx, filename.c_str());
		}

		bool SetPrivateKey(const std::string& filename)
		{
			ERR_clear_error();
			return SSL_CTX_use_PrivateKey_file(ctx, filename.c_str(), SSL_FILETYPE_PEM);
		}

		bool SetCA(const std::string& filename)
		{
			ERR_clear_error();
			return SSL_CTX_load_verify_locations(ctx, filename.c_str(), nullptr);
		}

		void SetCRL(const std::string& crlfile, const std::string& crlpath, const std::string& crlmode)
		{
			if (crlfile.empty() && crlpath.empty())
				return;

			/* Set CRL mode */
			unsigned long crlflags = X509_V_FLAG_CRL_CHECK;
			if (insp::equalsci(crlmode, "chain"))
			{
				crlflags |= X509_V_FLAG_CRL_CHECK_ALL;
			}
			else if (!insp::equalsci(crlmode, "leaf"))
			{
				throw ModuleException(thismod, "Unknown mode '" + crlmode + "'; expected either 'chain' (default) or 'leaf'");
			}

			/* Load CRL files */
			X509_STORE* store = SSL_CTX_get_cert_store(ctx);
			if (!store)
			{
				throw ModuleException(thismod, "Unable to get X509_STORE from TLS context; this should never happen");
			}
			ERR_clear_error();
			if (!X509_STORE_load_locations(store,
				crlfile.empty() ? nullptr : crlfile.c_str(),
				crlpath.empty() ? nullptr : crlpath.c_str()))
			{
				unsigned long err = ERR_get_error();
				throw ModuleException(thismod, "Unable to load CRL file '" + crlfile + "' or CRL path '" + crlpath + "': '" + (err ? ERR_error_string(err, nullptr) : "unknown") + "'");
			}

			/* Set CRL mode */
			if (X509_STORE_set_flags(store, crlflags) != 1)
			{
				throw ModuleException(thismod, "Unable to set X509 CRL flags");
			}
		}

		long GetDefaultContextOptions() const
		{
			return ctx_options;
		}

		long SetRawContextOptions(long setoptions, long clearoptions)
		{
			// Clear everything
			SSL_CTX_clear_options(ctx, SSL_CTX_get_options(ctx));

			// Set the default options and what is in the conf
			SSL_CTX_set_options(ctx, ctx_options | setoptions);
			return SSL_CTX_clear_options(ctx, clearoptions);
		}

		void SetVerifyCert()
		{
			SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, OnVerify);
		}

		SSL* CreateServerSession()
		{
			SSL* sess = SSL_new(ctx);
			SSL_set_accept_state(sess); // Act as server
			return sess;
		}

		SSL* CreateClientSession()
		{
			SSL* sess = SSL_new(ctx);
			SSL_set_connect_state(sess); // Act as client
			return sess;
		}
	};

	class Profile final
	{
		/** Name of this profile
		 */
		const std::string name;

		/** OpenSSL makes us have two contexts, one for servers and one for clients
		 */
		Context ctx;
		Context clientctx;

		/** Digest to use when generating fingerprints
		 */
		std::vector<const EVP_MD*> digests;

		/** Last error, set by error_callback()
		 */
		std::string lasterr;

		/** True if renegotiations are allowed, false if not
		 */
		const bool allowrenego;

		/** Rough max size of records to send
		 */
		const unsigned int outrecsize;

		static int error_callback(const char* str, size_t len, void* u)
		{
			Profile* profile = reinterpret_cast<Profile*>(u);
			profile->lasterr = std::string(str, len - 1);
			return 0;
		}

		/** Set raw OpenSSL context (SSL_CTX) options from a config tag
		 * @param ctxname Name of the context, client or server
		 * @param tag Config tag defining this profile
		 * @param context Context object to manipulate
		 */
		void SetContextOptions(const std::string& ctxname, const std::shared_ptr<ConfigTag>& tag, Context& context)
		{
			long setoptions = tag->getNum<long>(ctxname + "setoptions", 0);
			long clearoptions = tag->getNum<long>(ctxname + "clearoptions", 0);

#ifdef SSL_OP_NO_COMPRESSION
			// Disable compression by default
			if (!tag->getBool("compression", false))
				setoptions |= SSL_OP_NO_COMPRESSION;
#endif

#ifdef SSL_OP_NO_TLSv1_1
			// Disable TLSv1.1 by default.
			if (!tag->getBool("tlsv11", false))
				setoptions |= SSL_OP_NO_TLSv1_1;
#endif

#ifdef SSL_OP_NO_TLSv1_2
			// Enable TLSv1.2 by default.
			if (!tag->getBool("tlsv12", true))
				setoptions |= SSL_OP_NO_TLSv1_2;
#endif

#ifdef SSL_OP_NO_TLSv1_3
			// Enable TLSv1.3 by default.
			if (!tag->getBool("tlsv13", true))
				setoptions |= SSL_OP_NO_TLSv1_3;
#endif

			if (!setoptions && !clearoptions)
				return; // Nothing to do

			ServerInstance->Logs.Debug(MODNAME, "Setting {} {} context options, default: {} set: {} clear: {}", name, ctxname, ctx.GetDefaultContextOptions(), setoptions, clearoptions);
			long final = context.SetRawContextOptions(setoptions, clearoptions);
			ServerInstance->Logs.Normal(MODNAME, "{} {} context options: {}", name, ctxname, final);
		}

	public:
		Profile(const std::string& profilename, const std::shared_ptr<ConfigTag>& tag)
			: name(profilename)
			, ctx(SSL_CTX_new(TLS_server_method()))
			, clientctx(SSL_CTX_new(TLS_client_method()))
			, allowrenego(tag->getBool("renegotiation")) // Disallow by default
			, outrecsize(tag->getNum<unsigned int>("outrecsize", 2048, 512, 16384))
		{
			irc::spacesepstream hashstream(tag->getString("hash", "sha256", 1));
			for (std::string hash; hashstream.GetToken(hash); )
			{
				const auto* digest = EVP_get_digestbyname(hash.c_str());
				if (!digest)
					throw Exception("Unknown hash type " + hash);

				digests.push_back(digest);
			}

			const std::string ciphers = tag->getString("ciphers");
			if (!ciphers.empty())
			{
				if ((!ctx.SetCiphers(ciphers)) || (!clientctx.SetCiphers(ciphers)))
				{
					ERR_print_errors_cb(error_callback, this);
					throw Exception("Can't set cipher list to \"" + ciphers + "\" " + lasterr);
				}
			}

			const std::string ciphersuites = tag->getString("ciphersuites");
			if (!ciphersuites.empty())
			{
				if ((!ctx.SetCiphersuites(ciphersuites)) || (!clientctx.SetCiphersuites(ciphersuites)))
				{
					ERR_print_errors_cb(error_callback, this);
					throw Exception("Can't set ciphersuite list to \"" + ciphersuites + "\" " + lasterr);
				}
			}

#ifndef OPENSSL_NO_ECDH
			const std::string curvename = tag->getString("ecdhcurve", "prime256v1");
			if (!curvename.empty())
				ctx.SetECDH(curvename);
#endif

			SetContextOptions("server", tag, ctx);
			SetContextOptions("client", tag, clientctx);

			/* Load our keys and certificates
			 * NOTE: OpenSSL's error logging API sucks, don't blame us for this clusterfuck.
			 */
			std::string filename = ServerInstance->Config->Paths.PrependConfig(tag->getString("certfile", "cert.pem", 1));
			if ((!ctx.SetCerts(filename)) || (!clientctx.SetCerts(filename)))
			{
				ERR_print_errors_cb(error_callback, this);
				throw Exception("Can't read certificate file: " + lasterr);
			}

			filename = ServerInstance->Config->Paths.PrependConfig(tag->getString("keyfile", "key.pem", 1));
			if ((!ctx.SetPrivateKey(filename)) || (!clientctx.SetPrivateKey(filename)))
			{
				ERR_print_errors_cb(error_callback, this);
				throw Exception("Can't read key file: " + lasterr);
			}

			// Load the CAs we trust
			filename = ServerInstance->Config->Paths.PrependConfig(tag->getString("cafile", "ca.pem", 1));
			if ((!ctx.SetCA(filename)) || (!clientctx.SetCA(filename)))
			{
				ERR_print_errors_cb(error_callback, this);
				ServerInstance->Logs.Normal(MODNAME, "Can't read CA list from {}. This is only a problem if you want to verify client certificates, otherwise it's safe to ignore this message. Error: {}", filename, lasterr);
			}

			// Load the CRLs.
			const std::string crlfile = tag->getString("crlfile");
			const std::string crlpath = tag->getString("crlpath");
			const std::string crlmode = tag->getString("crlmode", "chain", 1);
			ctx.SetCRL(crlfile, crlpath, crlmode);

			clientctx.SetVerifyCert();
			if (tag->getBool("requestclientcert", true))
				ctx.SetVerifyCert();
		}

		const std::string& GetName() const { return name; }
		SSL* CreateServerSession() { return ctx.CreateServerSession(); }
		SSL* CreateClientSession() { return clientctx.CreateClientSession(); }
		const std::vector<const EVP_MD*> GetDigests() { return digests; }
		bool AllowRenegotiation() const { return allowrenego; }
		unsigned int GetOutgoingRecordSize() const { return outrecsize; }
	};

	namespace BIOMethod
	{
		static int create(BIO* bio)
		{
			BIO_set_init(bio, 1);
			return 1;
		}

		static int destroy(BIO* bio)
		{
			// XXX: Dummy function to avoid a memory leak in OpenSSL.
			// The memory leak happens in BIO_free() (bio_lib.c) when the destroy func of the BIO is NULL.
			// This is fixed in OpenSSL but some distros still ship the unpatched version hence we provide this workaround.
			return 1;
		}

		static long ctrl(BIO* bio, int cmd, long num, void* ptr)
		{
			if (cmd == BIO_CTRL_FLUSH)
				return 1;
			return 0;
		}

		// These signatures are required by the BIO_meth_set_write|read interface
		// even though they lead to shortening issues.
		static int read(BIO* bio, char* buf, int len);
		static int write(BIO* bio, const char* buf, int len);

		static BIO_METHOD* alloc()
		{
			BIO_METHOD* meth = BIO_meth_new(100 | BIO_TYPE_SOURCE_SINK, "inspircd");
			BIO_meth_set_write(meth, OpenSSL::BIOMethod::write);
			BIO_meth_set_read(meth, OpenSSL::BIOMethod::read);
			BIO_meth_set_ctrl(meth, OpenSSL::BIOMethod::ctrl);
			BIO_meth_set_create(meth, OpenSSL::BIOMethod::create);
			BIO_meth_set_destroy(meth, OpenSSL::BIOMethod::destroy);
			return meth;
		}
	}
}

static BIO_METHOD* biomethods;

static int OnVerify(int preverify_ok, X509_STORE_CTX* ctx)
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

class OpenSSLIOHook final
	: public SSLIOHook
{
private:
	SSL* sess;
	bool data_to_write = false;

	// Returns 1 if handshake succeeded, 0 if it is still in progress, -1 if it failed
	int Handshake(StreamSocket* user)
	{
		ERR_clear_error();
		int ret = SSL_do_handshake(sess);
		if (ret < 0)
		{
			int err = SSL_get_error(sess, ret);

			if (err == SSL_ERROR_WANT_READ)
			{
				SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				this->status = STATUS_HANDSHAKING;
				return 0;
			}
			else if (err == SSL_ERROR_WANT_WRITE)
			{
				SocketEngine::ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
				this->status = STATUS_HANDSHAKING;
				return 0;
			}
			else
			{
				CloseSession();
				return -1;
			}
		}
		else if (ret > 0)
		{
			// Handshake complete.
			VerifyCertificate();

			status = STATUS_OPEN;

			SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

			return 1;
		}
		else if (ret == 0)
		{
			CloseSession();
		}
		return -1;
	}

	void CloseSession()
	{
		if (sess)
		{
			SSL_shutdown(sess);
			SSL_free(sess);
		}
		sess = nullptr;
		certificate = nullptr;
		status = STATUS_NONE;
	}

	void VerifyCertificate()
	{
		X509* cert;
		auto* certinfo = new ssl_cert();
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

		GetDNString(X509_get_subject_name(cert), certinfo->dn);
		GetDNString(X509_get_issuer_name(cert), certinfo->issuer);

		for (const auto* digest : GetProfile().GetDigests())
		{
			if (!X509_digest(cert, digest, md, &n))
			{
				certinfo->error = "Out of memory generating fingerprint";
			}
			else
			{
				certinfo->fingerprints.push_back(Hex::Encode(md, n));
			}
		}

		certinfo->activation = GetTime(X509_getm_notBefore(cert));
		certinfo->expiration = GetTime(X509_getm_notAfter(cert));

		int activated = ASN1_TIME_cmp_time_t(X509_getm_notBefore(cert), ServerInstance->Time());
		if (activated != -1 && activated != 0)
		{
			certinfo->error = FMT::format("Certificate not active for {} (on {})",
				Duration::ToHuman(certinfo->activation - ServerInstance->Time(), true),
				Time::ToString(certinfo->activation));
		}

		int expired = ASN1_TIME_cmp_time_t(X509_getm_notAfter(cert), ServerInstance->Time());
		if (expired != 0 && expired != 1)
		{
			certinfo->error = FMT::format("Certificate expired {} ago (on {})",
				Duration::ToHuman(ServerInstance->Time() - certinfo->expiration, true),
				Time::ToString(certinfo->expiration));
		}

		X509_free(cert);
	}

	static void GetDNString(X509_NAME* x509name, std::string& out)
	{
		char buf[512];
		X509_NAME_oneline(x509name, buf, sizeof(buf));

		out.assign(buf);
		for (size_t pos = 0; ((pos = out.find_first_of("\r\n", pos)) != std::string::npos); )
			out[pos] = ' ';
	}

	static time_t GetTime(ASN1_TIME* x509time)
	{
		if (!x509time)
			return 0;

		struct tm ts;
		if (!ASN1_TIME_to_tm(x509time, &ts))
			return 0;

		return timegm(&ts);
	}

	void SSLInfoCallback(int where, int rc)
	{
		if ((where & SSL_CB_HANDSHAKE_START) && (status == STATUS_OPEN))
		{
			if (GetProfile().AllowRenegotiation())
				return;

			// The other side is trying to renegotiate, kill the connection and change status
			// to STATUS_NONE so CheckRenego() closes the session
			status = STATUS_NONE;
			BIO* bio = SSL_get_rbio(sess);
			EventHandler* eh = static_cast<StreamSocket*>(BIO_get_data(bio));
			SocketEngine::Shutdown(eh, 2);
		}
	}

	bool CheckRenego(StreamSocket* sock)
	{
		if (status != STATUS_NONE)
			return true;

		ServerInstance->Logs.Debug(MODNAME, "Session {} killed, attempted to renegotiate", FMT_PTR(sess));
		CloseSession();
		sock->SetError("Renegotiation is not allowed");
		return false;
	}

	// Returns 1 if application I/O should proceed, 0 if it must wait for the underlying protocol to progress, -1 on fatal error
	int PrepareIO(StreamSocket* sock)
	{
		if (status == STATUS_OPEN)
			return 1;
		else if (status == STATUS_HANDSHAKING)
		{
			// The handshake isn't finished, try to finish it
			return Handshake(sock);
		}

		CloseSession();
		return -1;
	}

	// Calls our private SSLInfoCallback()
	friend void StaticSSLInfoCallback(const SSL* ssl, int where, int rc);

public:
	OpenSSLIOHook(const std::shared_ptr<IOHookProvider>& hookprov, StreamSocket* sock, SSL* session)
		: SSLIOHook(hookprov)
		, sess(session)
	{
		// Create BIO instance and store a pointer to the socket in it which will be used by the read and write functions
		BIO* bio = BIO_new(biomethods);
		BIO_set_data(bio, sock);
		SSL_set_bio(sess, bio, bio);

		SSL_set_ex_data(sess, exdataindex, this);
		sock->AddIOHook(this);
		Handshake(sock);
	}

	void OnStreamSocketClose(StreamSocket* user) override
	{
		CloseSession();
	}

	ssize_t OnStreamSocketRead(StreamSocket* user, std::string& recvq) override
	{
		// Finish handshake if needed
		int prepret = PrepareIO(user);
		if (prepret <= 0)
			return prepret;

		// If we resumed the handshake then this->status will be STATUS_OPEN
		{
			ERR_clear_error();
			char* buffer = ServerInstance->GetReadBuffer();
			int bufsiz = static_cast<int>(std::min<size_t>(ServerInstance->Config->NetBufferSize, INT_MAX));
			int ret = SSL_read(sess, buffer, bufsiz);

			if (!CheckRenego(user))
				return -1;

			if (ret > 0)
			{
				recvq.append(buffer, ret);
				int mask = 0;
				// Schedule a read if there is still data in the OpenSSL buffer
				if (SSL_pending(sess) > 0)
					mask |= FD_ADD_TRIAL_READ;
				if (data_to_write)
					mask |= FD_WANT_POLL_READ | FD_WANT_SINGLE_WRITE;
				if (mask != 0)
					SocketEngine::ChangeEventMask(user, mask);
				return 1;
			}
			else if (ret == 0)
			{
				// Client closed connection.
				CloseSession();
				user->SetError("Connection closed");
				return -1;
			}
			else // if (ret < 0)
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
	}

	ssize_t OnStreamSocketWrite(StreamSocket* user, StreamSocket::SendQueue& sendq) override
	{
		// Finish handshake if needed
		int prepret = PrepareIO(user);
		if (prepret <= 0)
			return prepret;

		data_to_write = true;

		// Session is ready for transferring application data
		while (!sendq.empty())
		{
			ERR_clear_error();
			FlattenSendQueue(sendq, GetProfile().GetOutgoingRecordSize());
			const StreamSocket::SendQueue::Element& buffer = sendq.front();
			int ret = SSL_write(sess, buffer.data(), static_cast<int>(buffer.size()));

			if (!CheckRenego(user))
				return -1;

			if (ret == (int)buffer.length())
			{
				// Wrote entire record, continue sending
				sendq.pop_front();
			}
			else if (ret > 0)
			{
				sendq.erase_front(ret);
				SocketEngine::ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else if (ret == 0)
			{
				CloseSession();
				return -1;
			}
			else // if (ret < 0)
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

		data_to_write = false;
		SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
		return 1;
	}

	void GetCiphersuite(std::string& out) const override
	{
		if (!IsHookReady())
			return;
		out.append(SSL_get_version(sess)).push_back('-');
		out.append(SSL_get_cipher(sess));
	}

	bool GetServerName(std::string& out) const override
	{
		const char* name = SSL_get_servername(sess, TLSEXT_NAMETYPE_host_name);
		if (!name)
			return false;

		out.append(name);
		return true;
	}

	OpenSSL::Profile& GetProfile();
};

static void StaticSSLInfoCallback(const SSL* ssl, int where, int rc)
{
	OpenSSLIOHook* hook = static_cast<OpenSSLIOHook*>(SSL_get_ex_data(ssl, exdataindex));
	hook->SSLInfoCallback(where, rc);
}

static int OpenSSL::BIOMethod::write(BIO* bio, const char* buffer, int size)
{
	BIO_clear_retry_flags(bio);

	StreamSocket* sock = static_cast<StreamSocket*>(BIO_get_data(bio));
	if (sock->GetEventMask() & FD_WRITE_WILL_BLOCK)
	{
		// Writes blocked earlier, don't retry syscall
		BIO_set_retry_write(bio);
		return -1;
	}

	ssize_t ret = SocketEngine::Send(sock, buffer, size, 0);
	if ((ret < size) && ((ret > 0) || (SocketEngine::IgnoreError())))
	{
		// Blocked, set retry flag for OpenSSL
		SocketEngine::ChangeEventMask(sock, FD_WRITE_WILL_BLOCK);
		BIO_set_retry_write(bio);
	}

	return static_cast<int>(ret);
}

static int OpenSSL::BIOMethod::read(BIO* bio, char* buffer, int size)
{
	BIO_clear_retry_flags(bio);

	StreamSocket* sock = static_cast<StreamSocket*>(BIO_get_data(bio));
	if (sock->GetEventMask() & FD_READ_WILL_BLOCK)
	{
		// Reads blocked earlier, don't retry syscall
		BIO_set_retry_read(bio);
		return -1;
	}

	ssize_t ret = SocketEngine::Recv(sock, buffer, size, 0);
	if ((ret < size) && ((ret > 0) || (SocketEngine::IgnoreError())))
	{
		// Blocked, set retry flag for OpenSSL
		SocketEngine::ChangeEventMask(sock, FD_READ_WILL_BLOCK);
		BIO_set_retry_read(bio);
	}

	return static_cast<int>(ret);
}

class OpenSSLIOHookProvider final
	: public SSLIOHookProvider
{
	OpenSSL::Profile profile;

public:
	OpenSSLIOHookProvider(Module* mod, const std::string& profilename, const std::shared_ptr<ConfigTag>& tag)
		: SSLIOHookProvider(mod, profilename)
		, profile(profilename, tag)
	{
		ServerInstance->Modules.AddService(*this);
	}

	~OpenSSLIOHookProvider() override
	{
		ServerInstance->Modules.DelService(*this);
	}

	void OnAccept(StreamSocket* sock, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server) override
	{
		new OpenSSLIOHook(shared_from_this(), sock, profile.CreateServerSession());
	}

	void OnConnect(StreamSocket* sock) override
	{
		new OpenSSLIOHook(shared_from_this(), sock, profile.CreateClientSession());
	}

	OpenSSL::Profile& GetProfile() { return profile; }
};

OpenSSL::Profile& OpenSSLIOHook::GetProfile()
{
	return std::static_pointer_cast<OpenSSLIOHookProvider>(prov)->GetProfile();
}

class ModuleSSLOpenSSL final
	: public Module
{
	typedef std::vector<std::shared_ptr<OpenSSLIOHookProvider>> ProfileList;

	ProfileList profiles;

	void ReadProfiles()
	{
		ProfileList newprofiles;
		auto tags = ServerInstance->Config->ConfTags("sslprofile");
		if (tags.empty())
			throw ModuleException(this, "You have not specified any <sslprofile> tags that are usable by this module!");

		for (const auto& [_, tag] : tags)
		{
			if (!insp::equalsci(tag->getString("provider", "openssl", 1), "openssl"))
			{
				ServerInstance->Logs.Debug(MODNAME, "Ignoring non-OpenSSL <sslprofile> tag at {}", tag->source.str());
				continue;
			}

			const std::string name = tag->getString("name");
			if (name.empty())
			{
				ServerInstance->Logs.Warning(MODNAME, "Ignoring <sslprofile> tag without name at {}", tag->source.str());
				continue;
			}

			std::shared_ptr<OpenSSLIOHookProvider> prov;
			try
			{
				prov = std::make_shared<OpenSSLIOHookProvider>(this, name, tag);
			}
			catch (const CoreException& ex)
			{
				throw ModuleException(this, "Error while initializing TLS profile \"" + name + "\" at " + tag->source.str() + " - " + ex.GetReason());
			}

			newprofiles.push_back(prov);
		}

		for (const auto& profile : profiles)
			ServerInstance->Modules.DelService(*profile);

		profiles.swap(newprofiles);
	}

public:
	ModuleSSLOpenSSL()
		: Module(VF_VENDOR, "Allows TLS encrypted connections using the OpenSSL library.")
	{
		// Initialize OpenSSL
		OPENSSL_init_ssl(0, nullptr);
		biomethods = OpenSSL::BIOMethod::alloc();

		thismod = this;
	}

	~ModuleSSLOpenSSL() override
	{
		BIO_meth_free(biomethods);
	}

	void init() override
	{
		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against OpenSSL version {} and is running against version {}",
			OPENSSL_VERSION_STR, OpenSSL_version(OPENSSL_VERSION_STRING));

		// Register application specific data
		char exdatastr[] = "inspircd";
		exdataindex = SSL_get_ex_new_index(0, exdatastr, nullptr, nullptr, nullptr);
		if (exdataindex < 0)
			throw ModuleException(this, "Failed to register application specific data");
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("openssl");
		if (status.initial || tag->getBool("onrehash", true))
		{
			// Try to help people who have outdated configs.
			for (const auto& field : {"cafile", "certfile", "ciphers", "clientclearoptions", "clientsetoptions", "compression", "crlfile", "crlmode", "crlpath", "dhfile", "ecdhcurve", "hash", "keyfile", "renegotiation", "requestclientcert", "serverclearoptions", "serversetoptions", "tlsv1", "tlsv11", "tlsv12", "tlsv13"})
			{
				if (!tag->getString(field).empty())
					throw ModuleException(this, "TLS settings have moved from <openssl> to <sslprofile>. See " INSPIRCD_DOCS "modules/ssl_openssl/#sslprofile for more information.");
			}
			ReadProfiles();
		}
	}

	void OnModuleRehash(User* user, const std::string& param) override
	{
		if (!irc::equals(param, "tls") && !irc::equals(param, "ssl"))
			return;

		try
		{
			ReadProfiles();
			ServerInstance->SNO.WriteToSnoMask('r', "OpenSSL TLS profiles have been reloaded.");
		}
		catch (const ModuleException& ex)
		{
			ServerInstance->SNO.WriteToSnoMask('r', "Failed to reload the OpenSSL TLS profiles. " + ex.GetReason());
		}
	}

	void OnCleanup(ExtensionType type, Extensible* item) override
	{
		if (type == ExtensionType::USER)
		{
			LocalUser* user = IS_LOCAL((User*)item);

			if ((user) && (user->eh.GetModHook(this)))
			{
				// User is using TLS, they're a local user, and they're using one of *our* TLS ports.
				// Potentially there could be multiple TLS modules loaded at once on different ports.
				ServerInstance->Users.QuitUser(user, "OpenSSL module unloading");
			}
		}
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		const OpenSSLIOHook* const iohook = static_cast<OpenSSLIOHook*>(user->eh.GetModHook(this));
		if ((iohook) && (!iohook->IsHookReady()))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleSSLOpenSSL)
