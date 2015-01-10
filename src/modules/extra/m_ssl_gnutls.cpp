/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include "modules/ssl.h"
#include <memory>

// Fix warnings about the use of commas at end of enumerator lists on C++03.
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-extensions"
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-pedantic"
#endif

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#if ((GNUTLS_VERSION_MAJOR > 2) || (GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR > 9) || (GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR == 9 && GNUTLS_VERSION_PATCH >= 8))
#define GNUTLS_HAS_MAC_GET_ID
#include <gnutls/crypto.h>
#endif

#if (GNUTLS_VERSION_MAJOR > 2 || GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR > 12)
# define GNUTLS_HAS_RND
#else
# include <gcrypt.h>
#endif

#ifdef _WIN32
# pragma comment(lib, "libgnutls-28.lib")
#endif

/* $CompileFlags: pkgconfincludes("gnutls","/gnutls/gnutls.h","") eval("print `libgcrypt-config --cflags | tr -d \r` if `pkg-config --modversion gnutls 2>/dev/null | tr -d \r` lt '2.12'") */
/* $LinkerFlags: rpath("pkg-config --libs gnutls") pkgconflibs("gnutls","/libgnutls.so","-lgnutls") eval("print `libgcrypt-config --libs | tr -d \r` if `pkg-config --modversion gnutls 2>/dev/null | tr -d \r` lt '2.12'") */

#ifndef GNUTLS_VERSION_MAJOR
#define GNUTLS_VERSION_MAJOR LIBGNUTLS_VERSION_MAJOR
#define GNUTLS_VERSION_MINOR LIBGNUTLS_VERSION_MINOR
#define GNUTLS_VERSION_PATCH LIBGNUTLS_VERSION_PATCH
#endif

// These don't exist in older GnuTLS versions
#if ((GNUTLS_VERSION_MAJOR > 2) || (GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR > 1) || (GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR == 1 && GNUTLS_VERSION_PATCH >= 7))
#define GNUTLS_NEW_PRIO_API
#endif

#if(GNUTLS_VERSION_MAJOR < 2)
typedef gnutls_certificate_credentials_t gnutls_certificate_credentials;
typedef gnutls_dh_params_t gnutls_dh_params;
#endif

enum issl_status { ISSL_NONE, ISSL_HANDSHAKING_READ, ISSL_HANDSHAKING_WRITE, ISSL_HANDSHAKEN, ISSL_CLOSING, ISSL_CLOSED };

#if (GNUTLS_VERSION_MAJOR > 2 || (GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR >= 12))
#define GNUTLS_NEW_CERT_CALLBACK_API
typedef gnutls_retr2_st cert_cb_last_param_type;
#else
typedef gnutls_retr_st cert_cb_last_param_type;
#endif

class RandGen : public HandlerBase2<void, char*, size_t>
{
 public:
	void Call(char* buffer, size_t len)
	{
#ifdef GNUTLS_HAS_RND
		gnutls_rnd(GNUTLS_RND_RANDOM, buffer, len);
#else
		gcry_randomize(buffer, len, GCRY_STRONG_RANDOM);
#endif
	}
};

namespace GnuTLS
{
	class Init
	{
	 public:
		Init() { gnutls_global_init(); }
		~Init() { gnutls_global_deinit(); }
	};

	class Exception : public ModuleException
	{
	 public:
		Exception(const std::string& reason)
			: ModuleException(reason) { }
	};

	void ThrowOnError(int errcode, const char* msg)
	{
		if (errcode < 0)
		{
			std::string reason = msg;
			reason.append(" :").append(gnutls_strerror(errcode));
			throw Exception(reason);
		}
	}

	/** Used to create a gnutls_datum_t* from a std::string
	 */
	class Datum
	{
		gnutls_datum_t datum;

	 public:
		Datum(const std::string& dat)
		{
			datum.data = (unsigned char*)dat.data();
			datum.size = static_cast<unsigned int>(dat.length());
		}

		const gnutls_datum_t* get() const { return &datum; }
	};

	class Hash
	{
		gnutls_digest_algorithm_t hash;

	 public:
		// Nothing to deallocate, constructor may throw freely
		Hash(const std::string& hashname)
		{
			// As older versions of gnutls can't do this, let's disable it where needed.
#ifdef GNUTLS_HAS_MAC_GET_ID
			// As gnutls_digest_algorithm_t and gnutls_mac_algorithm_t are mapped 1:1, we can do this
			// There is no gnutls_dig_get_id() at the moment, but it may come later
			hash = (gnutls_digest_algorithm_t)gnutls_mac_get_id(hashname.c_str());
			if (hash == GNUTLS_DIG_UNKNOWN)
				throw Exception("Unknown hash type " + hashname);

			// Check if the user is giving us something that is a valid MAC but not digest
			gnutls_hash_hd_t is_digest;
			if (gnutls_hash_init(&is_digest, hash) < 0)
				throw Exception("Unknown hash type " + hashname);
			gnutls_hash_deinit(is_digest, NULL);
#else
			if (hashname == "md5")
				hash = GNUTLS_DIG_MD5;
			else if (hashname == "sha1")
				hash = GNUTLS_DIG_SHA1;
#ifdef INSPIRCD_GNUTLS_ENABLE_SHA256_FINGERPRINT
			else if (hashname == "sha256")
				hash = GNUTLS_DIG_SHA256;
#endif
			else
				throw Exception("Unknown hash type " + hashname);
#endif
		}

		gnutls_digest_algorithm_t get() const { return hash; }
	};

	class DHParams
	{
		gnutls_dh_params_t dh_params;

		DHParams()
		{
			ThrowOnError(gnutls_dh_params_init(&dh_params), "gnutls_dh_params_init() failed");
		}

	 public:
		/** Import */
		static std::auto_ptr<DHParams> Import(const std::string& dhstr)
		{
			std::auto_ptr<DHParams> dh(new DHParams);
			int ret = gnutls_dh_params_import_pkcs3(dh->dh_params, Datum(dhstr).get(), GNUTLS_X509_FMT_PEM);
			ThrowOnError(ret, "Unable to import DH params");
			return dh;
		}

		/** Generate */
		static std::auto_ptr<DHParams> Generate(unsigned int bits)
		{
			std::auto_ptr<DHParams> dh(new DHParams);
			ThrowOnError(gnutls_dh_params_generate2(dh->dh_params, bits), "Unable to generate DH params");
			return dh;
		}

		~DHParams()
		{
			gnutls_dh_params_deinit(dh_params);
		}

		const gnutls_dh_params_t& get() const { return dh_params; }
	};

	class X509Key
	{
		/** Ensure that the key is deinited in case the constructor of X509Key throws
		 */
		class RAIIKey
		{
		 public:
			gnutls_x509_privkey_t key;

			RAIIKey()
			{
				ThrowOnError(gnutls_x509_privkey_init(&key), "gnutls_x509_privkey_init() failed");
			}

			~RAIIKey()
			{
				gnutls_x509_privkey_deinit(key);
			}
		} key;

	 public:
		/** Import */
		X509Key(const std::string& keystr)
		{
			int ret = gnutls_x509_privkey_import(key.key, Datum(keystr).get(), GNUTLS_X509_FMT_PEM);
			ThrowOnError(ret, "Unable to import private key");
		}

		gnutls_x509_privkey_t& get() { return key.key; }
	};

	class X509CertList
	{
		std::vector<gnutls_x509_crt_t> certs;

	 public:
		/** Import */
		X509CertList(const std::string& certstr)
		{
			unsigned int certcount = 3;
			certs.resize(certcount);
			Datum datum(certstr);

			int ret = gnutls_x509_crt_list_import(raw(), &certcount, datum.get(), GNUTLS_X509_FMT_PEM, GNUTLS_X509_CRT_LIST_IMPORT_FAIL_IF_EXCEED);
			if (ret == GNUTLS_E_SHORT_MEMORY_BUFFER)
			{
				// the buffer wasn't big enough to hold all certs but gnutls changed certcount to the number of available certs,
				// try again with a bigger buffer
				certs.resize(certcount);
				ret = gnutls_x509_crt_list_import(raw(), &certcount, datum.get(), GNUTLS_X509_FMT_PEM, GNUTLS_X509_CRT_LIST_IMPORT_FAIL_IF_EXCEED);
			}

			ThrowOnError(ret, "Unable to load certificates");

			// Resize the vector to the actual number of certs because we rely on its size being correct
			// when deallocating the certs
			certs.resize(certcount);
		}

		~X509CertList()
		{
			for (std::vector<gnutls_x509_crt_t>::iterator i = certs.begin(); i != certs.end(); ++i)
				gnutls_x509_crt_deinit(*i);
		}

		gnutls_x509_crt_t* raw() { return &certs[0]; }
		unsigned int size() const { return certs.size(); }
	};

	class X509CRL : public refcountbase
	{
		class RAIICRL
		{
		 public:
			gnutls_x509_crl_t crl;

			RAIICRL()
			{
				ThrowOnError(gnutls_x509_crl_init(&crl), "gnutls_x509_crl_init() failed");
			}

			~RAIICRL()
			{
				gnutls_x509_crl_deinit(crl);
			}
		} crl;

	 public:
		/** Import */
		X509CRL(const std::string& crlstr)
		{
			int ret = gnutls_x509_crl_import(get(), Datum(crlstr).get(), GNUTLS_X509_FMT_PEM);
			ThrowOnError(ret, "Unable to load certificate revocation list");
		}

		gnutls_x509_crl_t& get() { return crl.crl; }
	};

#ifdef GNUTLS_NEW_PRIO_API
	class Priority
	{
		gnutls_priority_t priority;

	 public:
		Priority(const std::string& priorities)
		{
			// Try to set the priorities for ciphers, kex methods etc. to the user supplied string
			// If the user did not supply anything then the string is already set to "NORMAL"
			const char* priocstr = priorities.c_str();
			const char* prioerror;

			int ret = gnutls_priority_init(&priority, priocstr, &prioerror);
			if (ret < 0)
			{
				// gnutls did not understand the user supplied string
				throw Exception("Unable to initialize priorities to \"" + priorities + "\": " + gnutls_strerror(ret) + " Syntax error at position " + ConvToStr((unsigned int) (prioerror - priocstr)));
			}
		}

		~Priority()
		{
			gnutls_priority_deinit(priority);
		}

		void SetupSession(gnutls_session_t sess)
		{
			gnutls_priority_set(sess, priority);
		}
	};
#else
	/** Dummy class, used when gnutls_priority_set() is not available
	 */
	class Priority
	{
	 public:
		Priority(const std::string& priorities)
		{
			if (priorities != "NORMAL")
				throw Exception("You've set a non-default priority string, but GnuTLS lacks support for it");
		}

		static void SetupSession(gnutls_session_t sess)
		{
			// Always set the default priorities
			gnutls_set_default_priority(sess);
		}
	};
#endif

	class CertCredentials
	{
		/** DH parameters associated with these credentials
		 */
		std::auto_ptr<DHParams> dh;

	 protected:
		gnutls_certificate_credentials_t cred;

	 public:
		CertCredentials()
		{
			ThrowOnError(gnutls_certificate_allocate_credentials(&cred), "Cannot allocate certificate credentials");
		}

		~CertCredentials()
		{
			gnutls_certificate_free_credentials(cred);
		}

		/** Associates these credentials with the session
		 */
		void SetupSession(gnutls_session_t sess)
		{
			gnutls_credentials_set(sess, GNUTLS_CRD_CERTIFICATE, cred);
		}

		/** Set the given DH parameters to be used with these credentials
		 */
		void SetDH(std::auto_ptr<DHParams>& DH)
		{
			dh = DH;
			gnutls_certificate_set_dh_params(cred, dh->get());
		}
	};

	class X509Credentials : public CertCredentials
	{
		/** Private key
		 */
		X509Key key;

		/** Certificate list, presented to the peer
		 */
		X509CertList certs;

		/** Trusted CA, may be NULL
		 */
		std::auto_ptr<X509CertList> trustedca;

		/** Certificate revocation list, may be NULL
		 */
		std::auto_ptr<X509CRL> crl;

		static int cert_callback(gnutls_session_t session, const gnutls_datum_t* req_ca_rdn, int nreqs, const gnutls_pk_algorithm_t* sign_algos, int sign_algos_length, cert_cb_last_param_type* st);

	 public:
		X509Credentials(const std::string& certstr, const std::string& keystr)
			: key(keystr)
			, certs(certstr)
		{
			// Throwing is ok here, the destructor of Credentials is called in that case
			int ret = gnutls_certificate_set_x509_key(cred, certs.raw(), certs.size(), key.get());
			ThrowOnError(ret, "Unable to set cert/key pair");

#ifdef GNUTLS_NEW_CERT_CALLBACK_API
			gnutls_certificate_set_retrieve_function(cred, cert_callback);
#else
			gnutls_certificate_client_set_retrieve_function(cred, cert_callback);
#endif
		}

		/** Sets the trusted CA and the certificate revocation list
		 * to use when verifying certificates
		 */
		void SetCA(std::auto_ptr<X509CertList>& certlist, std::auto_ptr<X509CRL>& CRL)
		{
			// Do nothing if certlist is NULL
			if (certlist.get())
			{
				int ret = gnutls_certificate_set_x509_trust(cred, certlist->raw(), certlist->size());
				ThrowOnError(ret, "gnutls_certificate_set_x509_trust() failed");

				if (CRL.get())
				{
					ret = gnutls_certificate_set_x509_crl(cred, &CRL->get(), 1);
					ThrowOnError(ret, "gnutls_certificate_set_x509_crl() failed");
				}

				trustedca = certlist;
				crl = CRL;
			}
		}
	};

	class Profile : public refcountbase
	{
		/** Name of this profile
		 */
		const std::string name;

		/** X509 certificate(s) and key
		 */
		X509Credentials x509cred;

		/** The minimum length in bits for the DH prime to be accepted as a client
		 */
		unsigned int min_dh_bits;

		/** Hashing algorithm to use when generating certificate fingerprints
		 */
		Hash hash;

		/** Priorities for ciphers, compression methods, etc.
		 */
		Priority priority;

		Profile(const std::string& profilename, const std::string& certstr, const std::string& keystr,
				std::auto_ptr<DHParams>& DH, unsigned int mindh, const std::string& hashstr,
				const std::string& priostr, std::auto_ptr<X509CertList>& CA, std::auto_ptr<X509CRL>& CRL)
			: name(profilename)
			, x509cred(certstr, keystr)
			, min_dh_bits(mindh)
			, hash(hashstr)
			, priority(priostr)
		{
			x509cred.SetDH(DH);
			x509cred.SetCA(CA, CRL);
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
		static reference<Profile> Create(const std::string& profilename, ConfigTag* tag)
		{
			std::string certstr = ReadFile(tag->getString("certfile", "cert.pem"));
			std::string keystr = ReadFile(tag->getString("keyfile", "key.pem"));

			std::auto_ptr<DHParams> dh;
			int gendh = tag->getInt("gendh");
			if (gendh)
			{
				gendh = (gendh < 1024 ? 1024 : gendh);
				dh = DHParams::Generate(gendh);
			}
			else
				dh = DHParams::Import(ReadFile(tag->getString("dhfile", "dhparams.pem")));

			// Use default priority string if this tag does not specify one
			std::string priostr = tag->getString("priority", "NORMAL");
			unsigned int mindh = tag->getInt("mindhbits", 1024);
			std::string hashstr = tag->getString("hash", "md5");

			// Load trusted CA and revocation list, if set
			std::auto_ptr<X509CertList> ca;
			std::auto_ptr<X509CRL> crl;
			std::string filename = tag->getString("cafile");
			if (!filename.empty())
			{
				ca.reset(new X509CertList(ReadFile(filename)));

				filename = tag->getString("crlfile");
				if (!filename.empty())
					crl.reset(new X509CRL(ReadFile(filename)));
			}

			return new Profile(profilename, certstr, keystr, dh, mindh, hashstr, priostr, ca, crl);
		}

		/** Set up the given session with the settings in this profile
		 */
		void SetupSession(gnutls_session_t sess)
		{
			priority.SetupSession(sess);
			x509cred.SetupSession(sess);
			gnutls_dh_set_prime_bits(sess, min_dh_bits);
		}

		const std::string& GetName() const { return name; }
		X509Credentials& GetX509Credentials() { return x509cred; }
		gnutls_digest_algorithm_t GetHash() const { return hash.get(); }
	};
}

class GnuTLSIOHook : public SSLIOHook
{
 private:
	gnutls_session_t sess;
	issl_status status;
	reference<GnuTLS::Profile> profile;

	void InitSession(StreamSocket* user, bool me_server)
	{
		gnutls_init(&sess, me_server ? GNUTLS_SERVER : GNUTLS_CLIENT);

		profile->SetupSession(sess);
		gnutls_transport_set_ptr(sess, reinterpret_cast<gnutls_transport_ptr_t>(user));
		gnutls_transport_set_push_function(sess, gnutls_push_wrapper);
		gnutls_transport_set_pull_function(sess, gnutls_pull_wrapper);

		if (me_server)
			gnutls_certificate_server_set_request(sess, GNUTLS_CERT_REQUEST); // Request client certificate if any.
	}

	void CloseSession()
	{
		if (this->sess)
		{
			gnutls_bye(this->sess, GNUTLS_SHUT_WR);
			gnutls_deinit(this->sess);
		}
		sess = NULL;
		certificate = NULL;
		status = ISSL_NONE;
	}

	bool Handshake(StreamSocket* user)
	{
		int ret = gnutls_handshake(this->sess);

		if (ret < 0)
		{
			if(ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
			{
				// Handshake needs resuming later, read() or write() would have blocked.

				if (gnutls_record_get_direction(this->sess) == 0)
				{
					// gnutls_handshake() wants to read() again.
					this->status = ISSL_HANDSHAKING_READ;
					SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				}
				else
				{
					// gnutls_handshake() wants to write() again.
					this->status = ISSL_HANDSHAKING_WRITE;
					SocketEngine::ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
				}
			}
			else
			{
				user->SetError("Handshake Failed - " + std::string(gnutls_strerror(ret)));
				CloseSession();
				this->status = ISSL_CLOSING;
			}

			return false;
		}
		else
		{
			// Change the seesion state
			this->status = ISSL_HANDSHAKEN;

			VerifyCertificate();

			// Finish writing, if any left
			SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

			return true;
		}
	}

	void VerifyCertificate()
	{
		unsigned int certstatus;
		const gnutls_datum_t* cert_list;
		int ret;
		unsigned int cert_list_size;
		gnutls_x509_crt_t cert;
		char str[512];
		unsigned char digest[512];
		size_t digest_size = sizeof(digest);
		size_t name_size = sizeof(str);
		ssl_cert* certinfo = new ssl_cert;
		this->certificate = certinfo;

		/* This verification function uses the trusted CAs in the credentials
		 * structure. So you must have installed one or more CA certificates.
		 */
		ret = gnutls_certificate_verify_peers2(this->sess, &certstatus);

		if (ret < 0)
		{
			certinfo->error = std::string(gnutls_strerror(ret));
			return;
		}

		certinfo->invalid = (certstatus & GNUTLS_CERT_INVALID);
		certinfo->unknownsigner = (certstatus & GNUTLS_CERT_SIGNER_NOT_FOUND);
		certinfo->revoked = (certstatus & GNUTLS_CERT_REVOKED);
		certinfo->trusted = !(certstatus & GNUTLS_CERT_SIGNER_NOT_CA);

		/* Up to here the process is the same for X.509 certificates and
		 * OpenPGP keys. From now on X.509 certificates are assumed. This can
		 * be easily extended to work with openpgp keys as well.
		 */
		if (gnutls_certificate_type_get(this->sess) != GNUTLS_CRT_X509)
		{
			certinfo->error = "No X509 keys sent";
			return;
		}

		ret = gnutls_x509_crt_init(&cert);
		if (ret < 0)
		{
			certinfo->error = gnutls_strerror(ret);
			return;
		}

		cert_list_size = 0;
		cert_list = gnutls_certificate_get_peers(this->sess, &cert_list_size);
		if (cert_list == NULL)
		{
			certinfo->error = "No certificate was found";
			goto info_done_dealloc;
		}

		/* This is not a real world example, since we only check the first
		 * certificate in the given chain.
		 */

		ret = gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER);
		if (ret < 0)
		{
			certinfo->error = gnutls_strerror(ret);
			goto info_done_dealloc;
		}

		if (gnutls_x509_crt_get_dn(cert, str, &name_size) == 0)
		{
			std::string& dn = certinfo->dn;
			dn = str;
			// Make sure there are no chars in the string that we consider invalid
			if (dn.find_first_of("\r\n") != std::string::npos)
				dn.clear();
		}

		name_size = sizeof(str);
		if (gnutls_x509_crt_get_issuer_dn(cert, str, &name_size) == 0)
		{
			std::string& issuer = certinfo->issuer;
			issuer = str;
			if (issuer.find_first_of("\r\n") != std::string::npos)
				issuer.clear();
		}

		if ((ret = gnutls_x509_crt_get_fingerprint(cert, profile->GetHash(), digest, &digest_size)) < 0)
		{
			certinfo->error = gnutls_strerror(ret);
		}
		else
		{
			certinfo->fingerprint = BinToHex(digest, digest_size);
		}

		/* Beware here we do not check for errors.
		 */
		if ((gnutls_x509_crt_get_expiration_time(cert) < ServerInstance->Time()) || (gnutls_x509_crt_get_activation_time(cert) > ServerInstance->Time()))
		{
			certinfo->error = "Not activated, or expired certificate";
		}

info_done_dealloc:
		gnutls_x509_crt_deinit(cert);
	}

	static const char* UnknownIfNULL(const char* str)
	{
		return str ? str : "UNKNOWN";
	}

	static ssize_t gnutls_pull_wrapper(gnutls_transport_ptr_t session_wrap, void* buffer, size_t size)
	{
		StreamSocket* sock = reinterpret_cast<StreamSocket*>(session_wrap);
#ifdef _WIN32
		GnuTLSIOHook* session = static_cast<GnuTLSIOHook*>(sock->GetIOHook());
#endif

		if (sock->GetEventMask() & FD_READ_WILL_BLOCK)
		{
#ifdef _WIN32
			gnutls_transport_set_errno(session->sess, EAGAIN);
#else
			errno = EAGAIN;
#endif
			return -1;
		}

		int rv = SocketEngine::Recv(sock, reinterpret_cast<char *>(buffer), size, 0);

#ifdef _WIN32
		if (rv < 0)
		{
			/* Windows doesn't use errno, but gnutls does, so check SocketEngine::IgnoreError()
			 * and then set errno appropriately.
			 * The gnutls library may also have a different errno variable than us, see
			 * gnutls_transport_set_errno(3).
			 */
			gnutls_transport_set_errno(session->sess, SocketEngine::IgnoreError() ? EAGAIN : errno);
		}
#endif

		if (rv < (int)size)
			SocketEngine::ChangeEventMask(sock, FD_READ_WILL_BLOCK);
		return rv;
	}

	static ssize_t gnutls_push_wrapper(gnutls_transport_ptr_t session_wrap, const void* buffer, size_t size)
	{
		StreamSocket* sock = reinterpret_cast<StreamSocket*>(session_wrap);
#ifdef _WIN32
		GnuTLSIOHook* session = static_cast<GnuTLSIOHook*>(sock->GetIOHook());
#endif

		if (sock->GetEventMask() & FD_WRITE_WILL_BLOCK)
		{
#ifdef _WIN32
			gnutls_transport_set_errno(session->sess, EAGAIN);
#else
			errno = EAGAIN;
#endif
			return -1;
		}

		int rv = SocketEngine::Send(sock, reinterpret_cast<const char *>(buffer), size, 0);

#ifdef _WIN32
		if (rv < 0)
		{
			/* Windows doesn't use errno, but gnutls does, so check SocketEngine::IgnoreError()
			 * and then set errno appropriately.
			 * The gnutls library may also have a different errno variable than us, see
			 * gnutls_transport_set_errno(3).
			 */
			gnutls_transport_set_errno(session->sess, SocketEngine::IgnoreError() ? EAGAIN : errno);
		}
#endif

		if (rv < (int)size)
			SocketEngine::ChangeEventMask(sock, FD_WRITE_WILL_BLOCK);
		return rv;
	}

 public:
	GnuTLSIOHook(IOHookProvider* hookprov, StreamSocket* sock, bool outbound, const reference<GnuTLS::Profile>& sslprofile)
		: SSLIOHook(hookprov)
		, sess(NULL)
		, status(ISSL_NONE)
		, profile(sslprofile)
	{
		InitSession(sock, outbound);
		sock->AddIOHook(this);
		Handshake(sock);
	}

	void OnStreamSocketClose(StreamSocket* user) CXX11_OVERRIDE
	{
		CloseSession();
	}

	int OnStreamSocketRead(StreamSocket* user, std::string& recvq) CXX11_OVERRIDE
	{
		if (!this->sess)
		{
			CloseSession();
			user->SetError("No SSL session");
			return -1;
		}

		if (this->status == ISSL_HANDSHAKING_READ || this->status == ISSL_HANDSHAKING_WRITE)
		{
			// The handshake isn't finished, try to finish it.

			if (!Handshake(user))
			{
				if (this->status != ISSL_CLOSING)
					return 0;
				return -1;
			}
		}

		// If we resumed the handshake then this->status will be ISSL_HANDSHAKEN.

		if (this->status == ISSL_HANDSHAKEN)
		{
			char* buffer = ServerInstance->GetReadBuffer();
			size_t bufsiz = ServerInstance->Config->NetBufferSize;
			int ret = gnutls_record_recv(this->sess, buffer, bufsiz);
			if (ret > 0)
			{
				recvq.append(buffer, ret);
				return 1;
			}
			else if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
			{
				return 0;
			}
			else if (ret == 0)
			{
				user->SetError("Connection closed");
				CloseSession();
				return -1;
			}
			else
			{
				user->SetError(gnutls_strerror(ret));
				CloseSession();
				return -1;
			}
		}
		else if (this->status == ISSL_CLOSING)
			return -1;

		return 0;
	}

	int OnStreamSocketWrite(StreamSocket* user, std::string& sendq) CXX11_OVERRIDE
	{
		if (!this->sess)
		{
			CloseSession();
			user->SetError("No SSL session");
			return -1;
		}

		if (this->status == ISSL_HANDSHAKING_WRITE || this->status == ISSL_HANDSHAKING_READ)
		{
			// The handshake isn't finished, try to finish it.
			Handshake(user);
			if (this->status != ISSL_CLOSING)
				return 0;
			return -1;
		}

		int ret = 0;

		if (this->status == ISSL_HANDSHAKEN)
		{
			ret = gnutls_record_send(this->sess, sendq.data(), sendq.length());

			if (ret == (int)sendq.length())
			{
				SocketEngine::ChangeEventMask(user, FD_WANT_NO_WRITE);
				return 1;
			}
			else if (ret > 0)
			{
				sendq.erase(0, ret);
				SocketEngine::ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED || ret == 0)
			{
				SocketEngine::ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else // (ret < 0)
			{
				user->SetError(gnutls_strerror(ret));
				CloseSession();
				return -1;
			}
		}

		return 0;
	}

	void TellCiphersAndFingerprint(LocalUser* user)
	{
		if (sess)
		{
			std::string text = "*** You are connected using SSL cipher '";

			text += UnknownIfNULL(gnutls_kx_get_name(gnutls_kx_get(sess)));
			text.append("-").append(UnknownIfNULL(gnutls_cipher_get_name(gnutls_cipher_get(sess)))).append("-");
			text.append(UnknownIfNULL(gnutls_mac_get_name(gnutls_mac_get(sess)))).append("'");

			if (!certificate->fingerprint.empty())
				text += " and your SSL certificate fingerprint is " + certificate->fingerprint;

			user->WriteNotice(text);
		}
	}

	GnuTLS::Profile* GetProfile() { return profile; }
};

int GnuTLS::X509Credentials::cert_callback(gnutls_session_t sess, const gnutls_datum_t* req_ca_rdn, int nreqs, const gnutls_pk_algorithm_t* sign_algos, int sign_algos_length, cert_cb_last_param_type* st)
{
#ifndef GNUTLS_NEW_CERT_CALLBACK_API
	st->type = GNUTLS_CRT_X509;
#else
	st->cert_type = GNUTLS_CRT_X509;
	st->key_type = GNUTLS_PRIVKEY_X509;
#endif
	StreamSocket* sock = reinterpret_cast<StreamSocket*>(gnutls_transport_get_ptr(sess));
	GnuTLS::X509Credentials& cred = static_cast<GnuTLSIOHook*>(sock->GetIOHook())->GetProfile()->GetX509Credentials();

	st->ncerts = cred.certs.size();
	st->cert.x509 = cred.certs.raw();
	st->key.x509 = cred.key.get();
	st->deinit_all = 0;

	return 0;
}

class GnuTLSIOHookProvider : public refcountbase, public IOHookProvider
{
	reference<GnuTLS::Profile> profile;

 public:
 	GnuTLSIOHookProvider(Module* mod, reference<GnuTLS::Profile>& prof)
		: IOHookProvider(mod, "ssl/" + prof->GetName(), IOHookProvider::IOH_SSL)
		, profile(prof)
	{
		ServerInstance->Modules->AddService(*this);
	}

	~GnuTLSIOHookProvider()
	{
		ServerInstance->Modules->DelService(*this);
	}

	void OnAccept(StreamSocket* sock, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		new GnuTLSIOHook(this, sock, true, profile);
	}

	void OnConnect(StreamSocket* sock) CXX11_OVERRIDE
	{
		new GnuTLSIOHook(this, sock, false, profile);
	}
};

class ModuleSSLGnuTLS : public Module
{
	typedef std::vector<reference<GnuTLSIOHookProvider> > ProfileList;

	// First member of the class, gets constructed first and destructed last
	GnuTLS::Init libinit;
	RandGen randhandler;
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
			// No <sslprofile> tags found, create a profile named "gnutls" from settings in the <gnutls> block
			const std::string defname = "gnutls";
			ConfigTag* tag = ServerInstance->Config->ConfValue(defname);
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "No <sslprofile> tags found; using settings from the <gnutls> tag");

			try
			{
				reference<GnuTLS::Profile> profile(GnuTLS::Profile::Create(defname, tag));
				newprofiles.push_back(new GnuTLSIOHookProvider(this, profile));
			}
			catch (CoreException& ex)
			{
				throw ModuleException("Error while initializing the default SSL profile - " + ex.GetReason());
			}
		}

		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			if (tag->getString("provider") != "gnutls")
				continue;

			std::string name = tag->getString("name");
			if (name.empty())
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Ignoring <sslprofile> tag without name at " + tag->getTagLocation());
				continue;
			}

			reference<GnuTLS::Profile> profile;
			try
			{
				profile = GnuTLS::Profile::Create(name, tag);
			}
			catch (CoreException& ex)
			{
				throw ModuleException("Error while initializing SSL profile \"" + name + "\" at " + tag->getTagLocation() + " - " + ex.GetReason());
			}

			newprofiles.push_back(new GnuTLSIOHookProvider(this, profile));
		}

		// New profiles are ok, begin using them
		// Old profiles are deleted when their refcount drops to zero
		profiles.swap(newprofiles);
	}

 public:
	ModuleSSLGnuTLS()
	{
#ifndef GNUTLS_HAS_RND
		gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
#endif
	}

	void init() CXX11_OVERRIDE
	{
		ReadProfiles();
		ServerInstance->GenRandom = &randhandler;
	}

	void OnModuleRehash(User* user, const std::string &param) CXX11_OVERRIDE
	{
		if(param != "ssl")
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

	~ModuleSSLGnuTLS()
	{
		ServerInstance->GenRandom = &ServerInstance->HandleGenRandom;
	}

	void OnCleanup(int target_type, void* item) CXX11_OVERRIDE
	{
		if(target_type == TYPE_USER)
		{
			LocalUser* user = IS_LOCAL(static_cast<User*>(item));

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

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		IOHook* hook = user->eh.GetIOHook();
		if (hook && hook->prov->creator == this)
			static_cast<GnuTLSIOHook*>(hook)->TellCiphersAndFingerprint(user);
	}
};

MODULE_INIT(ModuleSSLGnuTLS)
