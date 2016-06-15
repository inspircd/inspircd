/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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


/* $LinkerFlags: -lmbedtls */

#include "inspircd.h"
#include "modules/ssl.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/dhm.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ciphersuites.h>
#include <mbedtls/version.h>
#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_crl.h>

#ifdef INSPIRCD_MBEDTLS_LIBRARY_DEBUG
#include <mbedtls/debug.h>
#endif

namespace mbedTLS
{
	class Exception : public ModuleException
	{
	 public:
		Exception(const std::string& reason)
			: ModuleException(reason) { }
	};

	std::string ErrorToString(int errcode)
	{
		char buf[256];
		mbedtls_strerror(errcode, buf, sizeof(buf));
		return buf;
	}

	void ThrowOnError(int errcode, const char* msg)
	{
		if (errcode != 0)
		{
			std::string reason = msg;
			reason.append(" :").append(ErrorToString(errcode));
			throw Exception(reason);
		}
	}

	template <typename T, void (*init)(T*), void (*deinit)(T*)>
	class RAIIObj
	{
		T obj;

	 public:
		RAIIObj()
		{
			init(&obj);
		}

		~RAIIObj()
		{
			deinit(&obj);
		}

		T* get() { return &obj; }
		const T* get() const { return &obj; }
	};

	typedef RAIIObj<mbedtls_entropy_context, mbedtls_entropy_init, mbedtls_entropy_free> Entropy;

	class CTRDRBG : private RAIIObj<mbedtls_ctr_drbg_context, mbedtls_ctr_drbg_init, mbedtls_ctr_drbg_free>
	{
	 public:
		bool Seed(Entropy& entropy)
		{
			return (mbedtls_ctr_drbg_seed(get(), mbedtls_entropy_func, entropy.get(), NULL, 0) == 0);
		}

		void SetupConf(mbedtls_ssl_config* conf)
		{
			mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, get());
		}
	};

	class DHParams : public RAIIObj<mbedtls_dhm_context, mbedtls_dhm_init, mbedtls_dhm_free>
	{
	 public:
		void set(const std::string& dhstr)
		{
			// Last parameter is buffer size, must include the terminating null
			int ret = mbedtls_dhm_parse_dhm(get(), reinterpret_cast<const unsigned char*>(dhstr.c_str()), dhstr.size()+1);
			ThrowOnError(ret, "Unable to import DH params");
		}
	};

	class X509Key : public RAIIObj<mbedtls_pk_context, mbedtls_pk_init, mbedtls_pk_free>
	{
	 public:
		/** Import */
		X509Key(const std::string& keystr)
		{
			int ret = mbedtls_pk_parse_key(get(), reinterpret_cast<const unsigned char*>(keystr.c_str()), keystr.size()+1, NULL, 0);
			ThrowOnError(ret, "Unable to import private key");
		}
	};

	class Ciphersuites
	{
		std::vector<int> list;

	 public:
		Ciphersuites(const std::string& str)
		{
			// mbedTLS uses the ciphersuite format "TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256" internally.
			// This is a bit verbose, so we make life a bit simpler for admins by not requiring them to supply the static parts.
			irc::sepstream ss(str, ':');
			for (std::string token; ss.GetToken(token); )
			{
				// Prepend "TLS-" if not there
				if (token.compare(0, 4, "TLS-", 4))
					token.insert(0, "TLS-");

				const int id = mbedtls_ssl_get_ciphersuite_id(token.c_str());
				if (!id)
					throw Exception("Unknown ciphersuite " + token);
				list.push_back(id);
			}
			list.push_back(0);
		}

		const int* get() const { return &list.front(); }
		bool empty() const { return (list.size() <= 1); }
	};

	class Curves
	{
		std::vector<mbedtls_ecp_group_id> list;

	 public:
		Curves(const std::string& str)
		{
			irc::sepstream ss(str, ':');
			for (std::string token; ss.GetToken(token); )
			{
				const mbedtls_ecp_curve_info* curve = mbedtls_ecp_curve_info_from_name(token.c_str());
				if (!curve)
					throw Exception("Unknown curve " + token);
				list.push_back(curve->grp_id);
			}
			list.push_back(MBEDTLS_ECP_DP_NONE);
		}

		const mbedtls_ecp_group_id* get() const { return &list.front(); }
		bool empty() const { return (list.size() <= 1); }
	};

	class X509CertList : public RAIIObj<mbedtls_x509_crt, mbedtls_x509_crt_init, mbedtls_x509_crt_free>
	{
	 public:
		/** Import or create empty */
		X509CertList(const std::string& certstr, bool allowempty = false)
		{
			if ((allowempty) && (certstr.empty()))
				return;
			int ret = mbedtls_x509_crt_parse(get(), reinterpret_cast<const unsigned char*>(certstr.c_str()), certstr.size()+1);
			ThrowOnError(ret, "Unable to load certificates");
		}

		bool empty() const { return (get()->raw.p != NULL); }
	};

	class X509CRL : public RAIIObj<mbedtls_x509_crl, mbedtls_x509_crl_init, mbedtls_x509_crl_free>
	{
	 public:
		X509CRL(const std::string& crlstr)
		{
			if (crlstr.empty())
				return;
			int ret = mbedtls_x509_crl_parse(get(), reinterpret_cast<const unsigned char*>(crlstr.c_str()), crlstr.size()+1);
			ThrowOnError(ret, "Unable to load CRL");
		}
	};

	class X509Credentials
	{
		/** Private key
		 */
		X509Key key;

		/** Certificate list, presented to the peer
		 */
		X509CertList certs;

	 public:
		X509Credentials(const std::string& certstr, const std::string& keystr)
			: key(keystr)
			, certs(certstr)
		{
			// Verify that one of the certs match the private key
			bool found = false;
			for (mbedtls_x509_crt* cert = certs.get(); cert; cert = cert->next)
			{
				if (mbedtls_pk_check_pair(&cert->pk, key.get()) == 0)
				{
					found = true;
					break;
				}
			}
			if (!found)
				throw Exception("Public/private key pair does not match");
		}

		mbedtls_pk_context* getkey() { return key.get(); }
		mbedtls_x509_crt* getcerts() { return certs.get(); }
	};

	class Context
	{
		mbedtls_ssl_config conf;

#ifdef INSPIRCD_MBEDTLS_LIBRARY_DEBUG
		static void DebugLogFunc(void* userptr, int level, const char* file, int line, const char* msg)
		{
			// Remove trailing \n
			size_t len = strlen(msg);
			if ((len > 0) && (msg[len-1] == '\n'))
				len--;
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "%s:%d %.*s", file, line, len, msg);
		}
#endif

	 public:
		Context(CTRDRBG& ctrdrbg, unsigned int endpoint)
		{
			mbedtls_ssl_config_init(&conf);
#ifdef INSPIRCD_MBEDTLS_LIBRARY_DEBUG
			mbedtls_debug_set_threshold(INT_MAX);
			mbedtls_ssl_conf_dbg(&conf, DebugLogFunc, NULL);
#endif
			mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);

			// TODO: check ret of mbedtls_ssl_config_defaults
			mbedtls_ssl_config_defaults(&conf, endpoint, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
			ctrdrbg.SetupConf(&conf);
		}

		~Context()
		{
			mbedtls_ssl_config_free(&conf);
		}

		void SetMinDHBits(unsigned int mindh)
		{
			mbedtls_ssl_conf_dhm_min_bitlen(&conf, mindh);
		}

		void SetDHParams(DHParams& dh)
		{
			mbedtls_ssl_conf_dh_param_ctx(&conf, dh.get());
		}

		void SetX509CertAndKey(X509Credentials& x509cred)
		{
			mbedtls_ssl_conf_own_cert(&conf, x509cred.getcerts(), x509cred.getkey());
		}

		void SetCiphersuites(const Ciphersuites& ciphersuites)
		{
			mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites.get());
		}

		void SetCurves(const Curves& curves)
		{
			mbedtls_ssl_conf_curves(&conf, curves.get());
		}

		void SetVersion(int minver, int maxver)
		{
			// SSL v3 support cannot be enabled
			if (minver)
				mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, minver);
			if (maxver)
				mbedtls_ssl_conf_max_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, maxver);
		}

		void SetCA(X509CertList& certs, X509CRL& crl)
		{
			mbedtls_ssl_conf_ca_chain(&conf, certs.get(), crl.get());
		}

		const mbedtls_ssl_config* GetConf() const { return &conf; }
	};

	class Hash
	{
		const mbedtls_md_info_t* md;

		/** Buffer where cert hashes are written temporarily
		 */
		mutable std::vector<unsigned char> buf;

	 public:
		Hash(std::string hashstr)
		{
			std::transform(hashstr.begin(), hashstr.end(), hashstr.begin(), ::toupper);
			md = mbedtls_md_info_from_string(hashstr.c_str());
			if (!md)
				throw Exception("Unknown hash: " + hashstr);

			buf.resize(mbedtls_md_get_size(md));
		}

		std::string hash(const unsigned char* input, size_t length) const
		{
			mbedtls_md(md, input, length, &buf.front());
			return BinToHex(&buf.front(), buf.size());
		}
	};

	class Profile : public refcountbase
	{
		/** Name of this profile
		 */
		const std::string name;

		X509Credentials x509cred;

		/** Ciphersuites to use
		 */
		Ciphersuites ciphersuites;

		/** Curves accepted for use in ECDHE and in the peer's end-entity certificate
		 */
		Curves curves;

		Context serverctx;
		Context clientctx;

		DHParams dhparams;

		X509CertList cacerts;

		X509CRL crl;

		/** Hashing algorithm to use when generating certificate fingerprints
		 */
		Hash hash;

		/** Rough max size of records to send
		 */
		const unsigned int outrecsize;

		Profile(const std::string& profilename, const std::string& certstr, const std::string& keystr,
				const std::string& dhstr, unsigned int mindh, const std::string& hashstr,
				const std::string& ciphersuitestr, const std::string& curvestr,
				const std::string& castr, const std::string& crlstr,
				unsigned int recsize,
				CTRDRBG& ctrdrbg,
				int minver, int maxver
				)
			: name(profilename)
			, x509cred(certstr, keystr)
			, ciphersuites(ciphersuitestr)
			, curves(curvestr)
			, serverctx(ctrdrbg, MBEDTLS_SSL_IS_SERVER)
			, clientctx(ctrdrbg, MBEDTLS_SSL_IS_CLIENT)
			, cacerts(castr, true)
			, crl(crlstr)
			, hash(hashstr)
			, outrecsize(recsize)
		{
			serverctx.SetX509CertAndKey(x509cred);
			clientctx.SetX509CertAndKey(x509cred);
			clientctx.SetMinDHBits(mindh);

			if (!ciphersuites.empty())
			{
				serverctx.SetCiphersuites(ciphersuites);
				clientctx.SetCiphersuites(ciphersuites);
			}

			if (!curves.empty())
			{
				serverctx.SetCurves(curves);
				clientctx.SetCurves(curves);
			}

			serverctx.SetVersion(minver, maxver);
			clientctx.SetVersion(minver, maxver);

			if (!dhstr.empty())
			{
				dhparams.set(dhstr);
				serverctx.SetDHParams(dhparams);
			}

			serverctx.SetCA(cacerts, crl);
		}

		static std::string ReadFile(const std::string& filename)
		{
			FileReader reader(filename);
			std::string ret = reader.GetString();
			if (ret.empty())
				throw Exception("Cannot read file " + filename);
			return ret;
		}

	 public:
		static reference<Profile> Create(const std::string& profilename, ConfigTag* tag, CTRDRBG& ctr_drbg)
		{
			const std::string certstr = ReadFile(tag->getString("certfile", "cert.pem"));
			const std::string keystr = ReadFile(tag->getString("keyfile", "key.pem"));
			const std::string dhstr = ReadFile(tag->getString("dhfile", "dhparams.pem"));

			const std::string ciphersuitestr = tag->getString("ciphersuites");
			const std::string curvestr = tag->getString("curves");
			unsigned int mindh = tag->getInt("mindhbits", 2048);
			std::string hashstr = tag->getString("hash", "sha256");

			std::string crlstr;
			std::string castr = tag->getString("cafile");
			if (!castr.empty())
			{
				castr = ReadFile(castr);
				crlstr = tag->getString("crlfile");
				if (!crlstr.empty())
					crlstr = ReadFile(crlstr);
			}

			int minver = tag->getInt("minver");
			int maxver = tag->getInt("maxver");
			unsigned int outrecsize = tag->getInt("outrecsize", 2048, 512, 16384);
			return new Profile(profilename, certstr, keystr, dhstr, mindh, hashstr, ciphersuitestr, curvestr, castr, crlstr, outrecsize, ctr_drbg, minver, maxver);
		}

		/** Set up the given session with the settings in this profile
		 */
		void SetupClientSession(mbedtls_ssl_context* sess)
		{
			mbedtls_ssl_setup(sess, clientctx.GetConf());
		}

		void SetupServerSession(mbedtls_ssl_context* sess)
		{
			mbedtls_ssl_setup(sess, serverctx.GetConf());
		}

		const std::string& GetName() const { return name; }
		X509Credentials& GetX509Credentials() { return x509cred; }
		unsigned int GetOutgoingRecordSize() const { return outrecsize; }
		const Hash& GetHash() const { return hash; }
	};
}

class mbedTLSIOHook : public SSLIOHook
{
	enum Status
	{
		ISSL_NONE,
		ISSL_HANDSHAKING,
		ISSL_HANDSHAKEN
	};

	mbedtls_ssl_context sess;
	Status status;
	reference<mbedTLS::Profile> profile;

	void CloseSession()
	{
		if (status == ISSL_NONE)
			return;

		mbedtls_ssl_close_notify(&sess);
		mbedtls_ssl_free(&sess);
		certificate = NULL;
		status = ISSL_NONE;
	}

	// Returns 1 if handshake succeeded, 0 if it is still in progress, -1 if it failed
	int Handshake(StreamSocket* sock)
	{
		int ret = mbedtls_ssl_handshake(&sess);
		if (ret == 0)
		{
			// Change the seesion state
			this->status = ISSL_HANDSHAKEN;

			VerifyCertificate();

			// Finish writing, if any left
			SocketEngine::ChangeEventMask(sock, FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

			return 1;
		}

		this->status = ISSL_HANDSHAKING;
		if (ret == MBEDTLS_ERR_SSL_WANT_READ)
		{
			SocketEngine::ChangeEventMask(sock, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
			return 0;
		}
		else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE)
		{
			SocketEngine::ChangeEventMask(sock, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
			return 0;
		}

		sock->SetError("Handshake Failed - " + mbedTLS::ErrorToString(ret));
		CloseSession();
		return -1;
	}

	// Returns 1 if application I/O should proceed, 0 if it must wait for the underlying protocol to progress, -1 on fatal error
	int PrepareIO(StreamSocket* sock)
	{
		if (status == ISSL_HANDSHAKEN)
			return 1;
		else if (status == ISSL_HANDSHAKING)
		{
			// The handshake isn't finished, try to finish it
			return Handshake(sock);
		}

		CloseSession();
		sock->SetError("No SSL session");
		return -1;
	}

	void VerifyCertificate()
	{
		this->certificate = new ssl_cert;
		const mbedtls_x509_crt* const cert = mbedtls_ssl_get_peer_cert(&sess);
		if (!cert)
		{
			certificate->error = "No client certificate sent";
			return;
		}

		// If there is a certificate we can always generate a fingerprint
		certificate->fingerprint = profile->GetHash().hash(cert->raw.p, cert->raw.len);

		// At this point mbedTLS verified the cert already, we just need to check the results
		const uint32_t flags = mbedtls_ssl_get_verify_result(&sess);
		if (flags == 0xFFFFFFFF)
		{
			certificate->error = "Internal error during verification";
			return;
		}

		if (flags == 0)
		{
			// Verification succeeded
			certificate->trusted = true;
		}
		else
		{
			// Verification failed
			certificate->trusted = false;
			if ((flags & MBEDTLS_X509_BADCERT_EXPIRED) || (flags & MBEDTLS_X509_BADCERT_FUTURE))
				certificate->error = "Not activated, or expired certificate";
		}

		certificate->unknownsigner = (flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED);
		certificate->revoked = (flags & MBEDTLS_X509_BADCERT_REVOKED);
		certificate->invalid = ((flags & MBEDTLS_X509_BADCERT_BAD_KEY) || (flags & MBEDTLS_X509_BADCERT_BAD_MD) || (flags & MBEDTLS_X509_BADCERT_BAD_PK));

		GetDNString(&cert->subject, certificate->dn);
		GetDNString(&cert->issuer, certificate->issuer);
	}

	static void GetDNString(const mbedtls_x509_name* x509name, std::string& out)
	{
		char buf[512];
		const int ret = mbedtls_x509_dn_gets(buf, sizeof(buf), x509name);
		if (ret <= 0)
			return;

		out.assign(buf, ret);
	}

	static int Pull(void* userptr, unsigned char* buffer, size_t size)
	{
		StreamSocket* const sock = reinterpret_cast<StreamSocket*>(userptr);
		if (sock->GetEventMask() & FD_READ_WILL_BLOCK)
			return MBEDTLS_ERR_SSL_WANT_READ;

		const int ret = SocketEngine::Recv(sock, reinterpret_cast<char*>(buffer), size, 0);
		if (ret < (int)size)
		{
			SocketEngine::ChangeEventMask(sock, FD_READ_WILL_BLOCK);
			if ((ret == -1) && (SocketEngine::IgnoreError()))
				return MBEDTLS_ERR_SSL_WANT_READ;
		}
		return ret;
	}

	static int Push(void* userptr, const unsigned char* buffer, size_t size)
	{
		StreamSocket* const sock = reinterpret_cast<StreamSocket*>(userptr);
		if (sock->GetEventMask() & FD_WRITE_WILL_BLOCK)
			return MBEDTLS_ERR_SSL_WANT_WRITE;

		const int ret = SocketEngine::Send(sock, buffer, size, 0);
		if (ret < (int)size)
		{
			SocketEngine::ChangeEventMask(sock, FD_WRITE_WILL_BLOCK);
			if ((ret == -1) && (SocketEngine::IgnoreError()))
				return MBEDTLS_ERR_SSL_WANT_WRITE;
		}
		return ret;
	}

 public:
	mbedTLSIOHook(IOHookProvider* hookprov, StreamSocket* sock, bool isserver, mbedTLS::Profile* sslprofile)
		: SSLIOHook(hookprov)
		, status(ISSL_NONE)
		, profile(sslprofile)
	{
		mbedtls_ssl_init(&sess);
		if (isserver)
			profile->SetupServerSession(&sess);
		else
			profile->SetupClientSession(&sess);

		mbedtls_ssl_set_bio(&sess, reinterpret_cast<void*>(sock), Push, Pull, NULL);

		sock->AddIOHook(this);
		Handshake(sock);
	}

	void OnStreamSocketClose(StreamSocket* sock) CXX11_OVERRIDE
	{
		CloseSession();
	}

	int OnStreamSocketRead(StreamSocket* sock, std::string& recvq) CXX11_OVERRIDE
	{
		// Finish handshake if needed
		int prepret = PrepareIO(sock);
		if (prepret <= 0)
			return prepret;

		// If we resumed the handshake then this->status will be ISSL_HANDSHAKEN.
		char* const readbuf = ServerInstance->GetReadBuffer();
		const size_t readbufsize = ServerInstance->Config->NetBufferSize;
		int ret = mbedtls_ssl_read(&sess, reinterpret_cast<unsigned char*>(readbuf), readbufsize);
		if (ret > 0)
		{
			recvq.append(readbuf, ret);

			// Schedule a read if there is still data in the mbedTLS buffer
			if (mbedtls_ssl_get_bytes_avail(&sess) > 0)
				SocketEngine::ChangeEventMask(sock, FD_ADD_TRIAL_READ);
			return 1;
		}
		else if (ret == MBEDTLS_ERR_SSL_WANT_READ)
		{
			SocketEngine::ChangeEventMask(sock, FD_WANT_POLL_READ);
			return 0;
		}
		else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE)
		{
			SocketEngine::ChangeEventMask(sock, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
			return 0;
		}
		else if (ret == 0)
		{
			sock->SetError("Connection closed");
			CloseSession();
			return -1;
		}
		else // error or MBEDTLS_ERR_SSL_CLIENT_RECONNECT which we treat as an error
		{
			sock->SetError(mbedTLS::ErrorToString(ret));
			CloseSession();
			return -1;
		}
	}

	int OnStreamSocketWrite(StreamSocket* sock) CXX11_OVERRIDE
	{
		// Finish handshake if needed
		int prepret = PrepareIO(sock);
		if (prepret <= 0)
			return prepret;

		// Session is ready for transferring application data
		StreamSocket::SendQueue& sendq = sock->GetSendQ();
		while (!sendq.empty())
		{
			FlattenSendQueue(sendq, profile->GetOutgoingRecordSize());
			const StreamSocket::SendQueue::Element& buffer = sendq.front();
			int ret = mbedtls_ssl_write(&sess, reinterpret_cast<const unsigned char*>(buffer.data()), buffer.length());
			if (ret == (int)buffer.length())
			{
				// Wrote entire record, continue sending
				sendq.pop_front();
			}
			else if (ret > 0)
			{
				sendq.erase_front(ret);
				SocketEngine::ChangeEventMask(sock, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else if (ret == 0)
			{
				sock->SetError("Connection closed");
				CloseSession();
				return -1;
			}
			else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE)
			{
				SocketEngine::ChangeEventMask(sock, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else if (ret == MBEDTLS_ERR_SSL_WANT_READ)
			{
				SocketEngine::ChangeEventMask(sock, FD_WANT_POLL_READ);
				return 0;
			}
			else
			{
				sock->SetError(mbedTLS::ErrorToString(ret));
				CloseSession();
				return -1;
			}
		}

		SocketEngine::ChangeEventMask(sock, FD_WANT_NO_WRITE);
		return 1;
	}

	void GetCiphersuite(std::string& out) const CXX11_OVERRIDE
	{
		if (!IsHandshakeDone())
			return;
		out.append(mbedtls_ssl_get_version(&sess)).push_back('-');

		// All mbedTLS ciphersuite names currently begin with "TLS-" which provides no useful information so skip it, but be prepared if it changes
		const char* const ciphersuitestr = mbedtls_ssl_get_ciphersuite(&sess);
		const char prefix[] = "TLS-";
		unsigned int skip = sizeof(prefix)-1;
		if (strncmp(ciphersuitestr, prefix, sizeof(prefix)-1))
			skip = 0;
		out.append(ciphersuitestr + skip);
	}

	bool IsHandshakeDone() const { return (status == ISSL_HANDSHAKEN); }
};

class mbedTLSIOHookProvider : public refcountbase, public IOHookProvider
{
	reference<mbedTLS::Profile> profile;

 public:
 	mbedTLSIOHookProvider(Module* mod, mbedTLS::Profile* prof)
		: IOHookProvider(mod, "ssl/" + prof->GetName(), IOHookProvider::IOH_SSL)
		, profile(prof)
	{
		ServerInstance->Modules->AddService(*this);
	}

	~mbedTLSIOHookProvider()
	{
		ServerInstance->Modules->DelService(*this);
	}

	void OnAccept(StreamSocket* sock, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		new mbedTLSIOHook(this, sock, true, profile);
	}

	void OnConnect(StreamSocket* sock) CXX11_OVERRIDE
	{
		new mbedTLSIOHook(this, sock, false, profile);
	}
};

class ModuleSSLmbedTLS : public Module
{
	typedef std::vector<reference<mbedTLSIOHookProvider> > ProfileList;

	mbedTLS::Entropy entropy;
	mbedTLS::CTRDRBG ctr_drbg;
	ProfileList profiles;

	void ReadProfiles()
	{
		// First, store all profiles in a new, temporary container. If no problems occur, swap the two
		// containers; this way if something goes wrong we can go back and continue using the current profiles,
		// avoiding unpleasant situations where no new SSL connections are possible.
		ProfileList newprofiles;

		ConfigTagList tags = ServerInstance->Config->ConfTags("sslprofile");
		if (tags.first == tags.second)
		{
			// No <sslprofile> tags found, create a profile named "mbedtls" from settings in the <mbedtls> block
			const std::string defname = "mbedtls";
			ConfigTag* tag = ServerInstance->Config->ConfValue(defname);
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "No <sslprofile> tags found; using settings from the <mbedtls> tag");

			try
			{
				reference<mbedTLS::Profile> profile(mbedTLS::Profile::Create(defname, tag, ctr_drbg));
				newprofiles.push_back(new mbedTLSIOHookProvider(this, profile));
			}
			catch (CoreException& ex)
			{
				throw ModuleException("Error while initializing the default SSL profile - " + ex.GetReason());
			}
		}

		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			if (tag->getString("provider") != "mbedtls")
				continue;

			std::string name = tag->getString("name");
			if (name.empty())
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Ignoring <sslprofile> tag without name at " + tag->getTagLocation());
				continue;
			}

			reference<mbedTLS::Profile> profile;
			try
			{
				profile = mbedTLS::Profile::Create(name, tag, ctr_drbg);
			}
			catch (CoreException& ex)
			{
				throw ModuleException("Error while initializing SSL profile \"" + name + "\" at " + tag->getTagLocation() + " - " + ex.GetReason());
			}

			newprofiles.push_back(new mbedTLSIOHookProvider(this, profile));
		}

		// New profiles are ok, begin using them
		// Old profiles are deleted when their refcount drops to zero
		profiles.swap(newprofiles);
	}

 public:
	void init() CXX11_OVERRIDE
	{
		char verbuf[16]; // Should be at least 9 bytes in size
		mbedtls_version_get_string(verbuf);
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "mbedTLS lib version %s module was compiled for " MBEDTLS_VERSION_STRING, verbuf);

		if (!ctr_drbg.Seed(entropy))
			throw ModuleException("CTR DRBG seed failed");
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

	void OnCleanup(int target_type, void* item) CXX11_OVERRIDE
	{
		if (target_type != TYPE_USER)
			return;

		LocalUser* user = IS_LOCAL(static_cast<User*>(item));
		if ((user) && (user->eh.GetIOHook()) && (user->eh.GetIOHook()->prov->creator == this))
		{
			// User is using SSL, they're a local user, and they're using our IOHook.
			// Potentially there could be multiple SSL modules loaded at once on different ports.
			ServerInstance->Users.QuitUser(user, "SSL module unloading");
		}
	}

	ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE
	{
		if ((user->eh.GetIOHook()) && (user->eh.GetIOHook()->prov->creator == this))
		{
			mbedTLSIOHook* iohook = static_cast<mbedTLSIOHook*>(user->eh.GetIOHook());
			if (!iohook->IsHandshakeDone())
				return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides SSL support via mbedTLS (PolarSSL)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSSLmbedTLS)
