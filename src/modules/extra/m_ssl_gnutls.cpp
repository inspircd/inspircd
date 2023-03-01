/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013-2014, 2016-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2017 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

/// $CompilerFlags: find_compiler_flags("gnutls")
/// $CompilerFlags: require_version("gnutls" "1.0" "2.12") execute("libgcrypt-config --cflags" "LIBGCRYPT_CXXFLAGS")

/// $LinkerFlags: find_linker_flags("gnutls" "-lgnutls")
/// $LinkerFlags: require_version("gnutls" "1.0" "2.12") execute("libgcrypt-config --libs" "LIBGCRYPT_LDFLAGS")

/// $PackageInfo: require_system("arch") gnutls pkgconf
/// $PackageInfo: require_system("centos") gnutls-devel pkgconfig
/// $PackageInfo: require_system("darwin") gnutls pkg-config
/// $PackageInfo: require_system("debian") gnutls-bin libgnutls28-dev pkg-config
/// $PackageInfo: require_system("rocky") gnutls-devel pkgconfig
/// $PackageInfo: require_system("ubuntu") gnutls-bin libgnutls28-dev pkg-config

#include "inspircd.h"
#include "modules/ssl.h"
#include <memory>

#ifdef __GNUC__
# pragma GCC diagnostic push
#endif

// Fix warnings about the use of commas at end of enumerator lists on C++03.
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-extensions"
#elif defined __GNUC__
# if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8))
#  pragma GCC diagnostic ignored "-Wpedantic"
# else
#  pragma GCC diagnostic ignored "-pedantic"
# endif
#endif

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

// C++98 compilers can't use shared_ptr and C++17 compilers can't use auto_ptr.
#if defined _LIBCPP_VERSION || defined _WIN32 || __cplusplus >= 201103L
# define SMART_PTR std::shared_ptr
#else
# define SMART_PTR std::auto_ptr
#endif

#ifndef GNUTLS_VERSION_NUMBER
#define GNUTLS_VERSION_NUMBER LIBGNUTLS_VERSION_NUMBER
#define GNUTLS_VERSION LIBGNUTLS_VERSION
#endif

// Check if the GnuTLS library is at least version major.minor.patch
#define INSPIRCD_GNUTLS_HAS_VERSION(major, minor, patch) (GNUTLS_VERSION_NUMBER >= ((major << 16) | (minor << 8) | patch))

#if INSPIRCD_GNUTLS_HAS_VERSION(2, 9, 8)
#define GNUTLS_HAS_MAC_GET_ID
#include <gnutls/crypto.h>
#endif

#if INSPIRCD_GNUTLS_HAS_VERSION(2, 12, 0)
# define GNUTLS_HAS_RND
#else
# include <gcrypt.h>
#endif

#if INSPIRCD_GNUTLS_HAS_VERSION(3, 6, 0)
# define GNUTLS_AUTO_DH
#endif

#ifdef _WIN32
# pragma comment(lib, "libgnutls-30.lib")
#endif

// These don't exist in older GnuTLS versions
#if INSPIRCD_GNUTLS_HAS_VERSION(2, 1, 7)
#define GNUTLS_NEW_PRIO_API
#endif

#if INSPIRCD_GNUTLS_HAS_VERSION(2, 12, 0)
#define INSPIRCD_GNUTLS_HAS_VECTOR_PUSH
#define GNUTLS_NEW_CERT_CALLBACK_API
typedef gnutls_retr2_st cert_cb_last_param_type;
#else
typedef gnutls_retr_st cert_cb_last_param_type;
#endif

#if INSPIRCD_GNUTLS_HAS_VERSION(3, 3, 5)
#define INSPIRCD_GNUTLS_HAS_RECV_PACKET
#endif

#if INSPIRCD_GNUTLS_HAS_VERSION(2, 99, 0)
// The second parameter of gnutls_init() has changed in 2.99.0 from gnutls_connection_end_t to unsigned int
// (it became a general flags parameter) and the enum has been deprecated and generates a warning on use.
typedef unsigned int inspircd_gnutls_session_init_flags_t;
#else
typedef gnutls_connection_end_t inspircd_gnutls_session_init_flags_t;
#endif

#if INSPIRCD_GNUTLS_HAS_VERSION(3, 1, 9)
#define INSPIRCD_GNUTLS_HAS_CORK
#endif

static Module* thismod;

class RandGen
{
 public:
	static void Call(char* buffer, size_t len)
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
# if INSPIRCD_GNUTLS_HAS_VERSION(3, 2, 2)
			hash = gnutls_digest_get_id(hashname.c_str());
# else
			// As gnutls_digest_algorithm_t and gnutls_mac_algorithm_t are mapped 1:1, we can do this
			hash = (gnutls_digest_algorithm_t)gnutls_mac_get_id(hashname.c_str());
# endif
			if (hash == GNUTLS_DIG_UNKNOWN)
				throw Exception("Unknown hash type " + hashname);

			// Check if the user is giving us something that is a valid MAC but not digest
			gnutls_hash_hd_t is_digest;
			if (gnutls_hash_init(&is_digest, hash) < 0)
				throw Exception("Unknown hash type " + hashname);
			gnutls_hash_deinit(is_digest, NULL);
#else
			if (stdalgo::string::equalsci(hashname, "md5"))
				hash = GNUTLS_DIG_MD5;
			else if (stdalgo::string::equalsci(hashname, "sha1"))
				hash = GNUTLS_DIG_SHA1;
			else if (stdalgo::string::equalsci(hashname, "sha256"))
				hash = GNUTLS_DIG_SHA256;
			else
				throw Exception("Unknown hash type " + hashname);
#endif
		}

		gnutls_digest_algorithm_t get() const { return hash; }
	};

#ifndef GNUTLS_AUTO_DH
	class DHParams
	{
		gnutls_dh_params_t dh_params;

		DHParams()
		{
			ThrowOnError(gnutls_dh_params_init(&dh_params), "gnutls_dh_params_init() failed");
		}

	 public:
		/** Import */
		static SMART_PTR<DHParams> Import(const std::string& dhstr)
		{
			SMART_PTR<DHParams> dh(new DHParams);
			int ret = gnutls_dh_params_import_pkcs3(dh->dh_params, Datum(dhstr).get(), GNUTLS_X509_FMT_PEM);
			ThrowOnError(ret, "Unable to import DH params");
			return dh;
		}

		~DHParams()
		{
			gnutls_dh_params_deinit(dh_params);
		}

		const gnutls_dh_params_t& get() const { return dh_params; }
	};
#endif

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

		static const char* GetDefault()
		{
			return "NORMAL:%SERVER_PRECEDENCE:-VERS-SSL3.0";
		}

		static std::string RemoveUnknownTokens(const std::string& prio)
		{
			std::string ret;
			irc::sepstream ss(prio, ':');
			for (std::string token; ss.GetToken(token); )
			{
				// Save current position so we can revert later if needed
				const std::string::size_type prevpos = ret.length();
				// Append next token
				if (!ret.empty())
					ret.push_back(':');
				ret.append(token);

				gnutls_priority_t test;
				if (gnutls_priority_init(&test, ret.c_str(), NULL) < 0)
				{
					// The new token broke the priority string, revert to the previously working one
					ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Priority string token not recognized: \"%s\"", token.c_str());
					ret.erase(prevpos);
				}
				else
				{
					// Worked
					gnutls_priority_deinit(test);
				}
			}
			return ret;
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
			if (priorities != GetDefault())
				throw Exception("You've set a non-default priority string, but GnuTLS lacks support for it");
		}

		static void SetupSession(gnutls_session_t sess)
		{
			// Always set the default priorities
			gnutls_set_default_priority(sess);
		}

		static const char* GetDefault()
		{
			return "NORMAL";
		}

		static std::string RemoveUnknownTokens(const std::string& prio)
		{
			// We don't do anything here because only NORMAL is accepted
			return prio;
		}
	};
#endif

	class CertCredentials
	{
#ifndef GNUTLS_AUTO_DH
		/** DH parameters associated with these credentials
		 */
		SMART_PTR<DHParams> dh;
#endif

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

#ifndef GNUTLS_AUTO_DH
		/** Set the given DH parameters to be used with these credentials
		 */
		void SetDH(SMART_PTR<DHParams>& DH)
		{
			dh = DH;
			gnutls_certificate_set_dh_params(cred, dh->get());
		}
#endif
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
		SMART_PTR<X509CertList> trustedca;

		/** Certificate revocation list, may be NULL
		 */
		SMART_PTR<X509CRL> crl;

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
		void SetCA(SMART_PTR<X509CertList>& certlist, SMART_PTR<X509CRL>& CRL)
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

	class DataReader
	{
		int retval;
#ifdef INSPIRCD_GNUTLS_HAS_RECV_PACKET
		gnutls_packet_t packet;

	 public:
		DataReader(gnutls_session_t sess)
		{
			// Using the packet API avoids the final copy of the data which GnuTLS does if we supply
			// our own buffer. Instead, we get the buffer containing the data from GnuTLS and copy it
			// to the recvq directly from there in appendto().
			retval = gnutls_record_recv_packet(sess, &packet);
		}

		void appendto(std::string& recvq)
		{
			// Copy data from GnuTLS buffers to recvq
			gnutls_datum_t datum;
			gnutls_packet_get(packet, &datum, NULL);
			recvq.append(reinterpret_cast<const char*>(datum.data), datum.size);

			gnutls_packet_deinit(packet);
		}
#else
		char* const buffer;

	 public:
		DataReader(gnutls_session_t sess)
			: buffer(ServerInstance->GetReadBuffer())
		{
			// Read data from GnuTLS buffers into ReadBuffer
			retval = gnutls_record_recv(sess, buffer, ServerInstance->Config->NetBufferSize);
		}

		void appendto(std::string& recvq)
		{
			// Copy data from ReadBuffer to recvq
			recvq.append(buffer, retval);
		}
#endif

		int ret() const { return retval; }
	};

	class Profile
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

		/** Rough max size of records to send
		 */
		const unsigned int outrecsize;

		/** True to request a client certificate as a server
		 */
		const bool requestclientcert;

		static std::string ReadFile(const std::string& filename)
		{
			FileReader reader(filename);
			std::string ret = reader.GetString();
			if (ret.empty())
				throw Exception("Cannot read file " + filename);
			return ret;
		}

		static std::string GetPrioStr(const std::string& profilename, ConfigTag* tag)
		{
			// Use default priority string if this tag does not specify one
			std::string priostr = GnuTLS::Priority::GetDefault();
			bool found = tag->readString("priority", priostr);
			// If the prio string isn't set in the config don't be strict about the default one because it doesn't work on all versions of GnuTLS
			if (!tag->getBool("strictpriority", found))
			{
				std::string stripped = GnuTLS::Priority::RemoveUnknownTokens(priostr);
				if (stripped.empty())
				{
					// Stripping failed, act as if a prio string wasn't set
					stripped = GnuTLS::Priority::RemoveUnknownTokens(GnuTLS::Priority::GetDefault());
					ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Priority string for profile \"%s\" contains unknown tokens and stripping it didn't yield a working one either, falling back to \"%s\"", profilename.c_str(), stripped.c_str());
				}
				else if ((found) && (stripped != priostr))
				{
					// Prio string was set in the config and we ended up with something that works but different
					ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Priority string for profile \"%s\" contains unknown tokens, stripped to \"%s\"", profilename.c_str(), stripped.c_str());
				}
				priostr.swap(stripped);
			}
			return priostr;
		}

	 public:
		struct Config
		{
			std::string name;

			SMART_PTR<X509CertList> ca;
			SMART_PTR<X509CRL> crl;

			std::string certstr;
			std::string keystr;

#ifndef GNUTLS_AUTO_DH
			SMART_PTR<DHParams> dh;
#endif
			std::string priostr;
			unsigned int mindh;
			std::string hashstr;

			unsigned int outrecsize;
			bool requestclientcert;

			Config(const std::string& profilename, ConfigTag* tag)
				: name(profilename)
				, certstr(ReadFile(tag->getString("certfile", "cert.pem", 1)))
				, keystr(ReadFile(tag->getString("keyfile", "key.pem", 1)))
#ifndef GNUTLS_AUTO_DH
				, dh(DHParams::Import(ReadFile(tag->getString("dhfile", "dhparams.pem", 1))))
#endif
				, priostr(GetPrioStr(profilename, tag))
				, mindh(tag->getUInt("mindhbits", 1024))
				, hashstr(tag->getString("hash", "md5", 1))
				, requestclientcert(tag->getBool("requestclientcert", true))
			{
				// Load trusted CA and revocation list, if set
				std::string filename = tag->getString("cafile");
				if (!filename.empty())
				{
					ca.reset(new X509CertList(ReadFile(filename)));

					filename = tag->getString("crlfile");
					if (!filename.empty())
						crl.reset(new X509CRL(ReadFile(filename)));
				}

#ifdef INSPIRCD_GNUTLS_HAS_CORK
				// If cork support is available outrecsize represents the (rough) max amount of data we give GnuTLS while corked
				outrecsize = tag->getUInt("outrecsize", 2048, 512);
#else
				outrecsize = tag->getUInt("outrecsize", 2048, 512, 16384);
#endif
			}
		};

		Profile(Config& config)
			: name(config.name)
			, x509cred(config.certstr, config.keystr)
			, min_dh_bits(config.mindh)
			, hash(config.hashstr)
			, priority(config.priostr)
			, outrecsize(config.outrecsize)
			, requestclientcert(config.requestclientcert)
		{
#ifndef GNUTLS_AUTO_DH
			x509cred.SetDH(config.dh);
#endif
			x509cred.SetCA(config.ca, config.crl);
		}
		/** Set up the given session with the settings in this profile
		 */
		void SetupSession(gnutls_session_t sess)
		{
			priority.SetupSession(sess);
			x509cred.SetupSession(sess);
			gnutls_dh_set_prime_bits(sess, min_dh_bits);

			// Request client certificate if enabled and we are a server, no-op if we're a client
			if (requestclientcert)
				gnutls_certificate_server_set_request(sess, GNUTLS_CERT_REQUEST);
		}

		const std::string& GetName() const { return name; }
		X509Credentials& GetX509Credentials() { return x509cred; }
		gnutls_digest_algorithm_t GetHash() const { return hash.get(); }
		unsigned int GetOutgoingRecordSize() const { return outrecsize; }
	};
}

class GnuTLSIOHook : public SSLIOHook
{
 private:
	gnutls_session_t sess;
#ifdef INSPIRCD_GNUTLS_HAS_CORK
	size_t gbuffersize;
#endif

	void CloseSession()
	{
		if (this->sess)
		{
			gnutls_bye(this->sess, GNUTLS_SHUT_WR);
			gnutls_deinit(this->sess);
		}
		sess = NULL;
		certificate = NULL;
		status = STATUS_NONE;
	}

	// Returns 1 if handshake succeeded, 0 if it is still in progress, -1 if it failed
	int Handshake(StreamSocket* user)
	{
		int ret = gnutls_handshake(this->sess);

		if (ret < 0)
		{
			if(ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
			{
				// Handshake needs resuming later, read() or write() would have blocked.
				this->status = STATUS_HANDSHAKING;

				if (gnutls_record_get_direction(this->sess) == 0)
				{
					// gnutls_handshake() wants to read() again.
					SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				}
				else
				{
					// gnutls_handshake() wants to write() again.
					SocketEngine::ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
				}

				return 0;
			}
			else
			{
				user->SetError("Handshake Failed - " + std::string(gnutls_strerror(ret)));
				CloseSession();
				return -1;
			}
		}
		else
		{
			// Change the session state
			this->status = STATUS_OPEN;

			VerifyCertificate();

			// Finish writing, if any left
			SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

			return 1;
		}
	}

	void VerifyCertificate()
	{
		ssl_cert* certinfo = new ssl_cert;
		this->certificate = certinfo;

		unsigned int certstatus;
		int ret = gnutls_certificate_verify_peers2(this->sess, &certstatus);
		if (ret < 0)
		{
			certinfo->error = gnutls_strerror(ret);
			return;
		}

		certinfo->invalid = (certstatus & GNUTLS_CERT_INVALID);
		certinfo->unknownsigner = (certstatus & GNUTLS_CERT_SIGNER_NOT_FOUND);
		certinfo->revoked = (certstatus & GNUTLS_CERT_REVOKED);
		certinfo->trusted = !(certstatus & GNUTLS_CERT_SIGNER_NOT_CA);

		if (gnutls_certificate_type_get(this->sess) != GNUTLS_CRT_X509)
		{
			certinfo->error = "No X509 keys sent";
			return;
		}

		gnutls_x509_crt_t cert;
		ret = gnutls_x509_crt_init(&cert);
		if (ret < 0)
		{
			certinfo->error = gnutls_strerror(ret);
			return;
		}

		char buffer[512];
		size_t buffer_size = sizeof(buffer);

		unsigned int cert_list_size = 0;
		const gnutls_datum_t* cert_list = gnutls_certificate_get_peers(this->sess, &cert_list_size);
		if (cert_list == NULL)
		{
			certinfo->error = "No certificate was found";
			goto info_done_dealloc;
		}

		ret = gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER);
		if (ret < 0)
		{
			certinfo->error = gnutls_strerror(ret);
			goto info_done_dealloc;
		}

		if (gnutls_x509_crt_get_dn(cert, buffer, &buffer_size) == 0)
			ProcessDNString(buffer, buffer_size, certinfo->dn);

		buffer_size = sizeof(buffer);
		if (gnutls_x509_crt_get_issuer_dn(cert, buffer, &buffer_size) == 0)
			ProcessDNString(buffer, buffer_size, certinfo->issuer);

		buffer_size = sizeof(buffer);
		if ((ret = gnutls_x509_crt_get_fingerprint(cert, GetProfile().GetHash(), buffer, &buffer_size)) < 0)
		{
			certinfo->error = gnutls_strerror(ret);
		}
		else
		{
			certinfo->fingerprint = BinToHex(buffer, buffer_size);
		}

		certinfo->activation = gnutls_x509_crt_get_activation_time(cert);
		if (certinfo->activation == -1)
		{
			certinfo->activation = 0;
			certinfo->error = "Unable to check certificate activation time";
		}
		else if (certinfo->activation >= ServerInstance->Time())
		{
			certinfo->error = "Certificate not activated";
		}

		certinfo->expiration = gnutls_x509_crt_get_expiration_time(cert);
		if (certinfo->expiration == -1)
		{
			certinfo->expiration = 0;
			certinfo->error = "Unable to check certificate expiration time";
		}
		else if (certinfo->expiration <= ServerInstance->Time())
		{
			certinfo->error = "Certificate has expired";
		}

info_done_dealloc:
		gnutls_x509_crt_deinit(cert);
	}

	static void ProcessDNString(const char* buffer, size_t buffer_size, std::string& out)
	{
		out.assign(buffer, buffer_size);
		for (size_t pos = 0; ((pos = out.find_first_of("\r\n", pos)) != std::string::npos); )
			out[pos] = ' ';
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
		sock->SetError("No TLS (SSL) session");
		return -1;
	}

#ifdef INSPIRCD_GNUTLS_HAS_CORK
	int FlushBuffer(StreamSocket* sock)
	{
		// If GnuTLS has some data buffered, write it
		if (gbuffersize)
			return HandleWriteRet(sock, gnutls_record_uncork(this->sess, 0));
		return 1;
	}
#endif

	int HandleWriteRet(StreamSocket* sock, int ret)
	{
		if (ret > 0)
		{
#ifdef INSPIRCD_GNUTLS_HAS_CORK
			gbuffersize -= ret;
			if (gbuffersize)
			{
				SocketEngine::ChangeEventMask(sock, FD_WANT_SINGLE_WRITE);
				return 0;
			}
#endif
			return ret;
		}
		else if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED || ret == 0)
		{
			SocketEngine::ChangeEventMask(sock, FD_WANT_SINGLE_WRITE);
			return 0;
		}
		else // (ret < 0)
		{
			sock->SetError(gnutls_strerror(ret));
			CloseSession();
			return -1;
		}
	}

	static const char* UnknownIfNULL(const char* str)
	{
		return str ? str : "UNKNOWN";
	}

	static ssize_t gnutls_pull_wrapper(gnutls_transport_ptr_t session_wrap, void* buffer, size_t size)
	{
		StreamSocket* sock = reinterpret_cast<StreamSocket*>(session_wrap);
#ifdef _WIN32
		GnuTLSIOHook* session = static_cast<GnuTLSIOHook*>(sock->GetModHook(thismod));
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

#ifdef INSPIRCD_GNUTLS_HAS_VECTOR_PUSH
	static ssize_t VectorPush(gnutls_transport_ptr_t transportptr, const giovec_t* iov, int iovcnt)
	{
		StreamSocket* sock = reinterpret_cast<StreamSocket*>(transportptr);
#ifdef _WIN32
		GnuTLSIOHook* session = static_cast<GnuTLSIOHook*>(sock->GetModHook(thismod));
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

		// Cast the giovec_t to iovec not to IOVector so the correct function is called on Windows
		int ret = SocketEngine::WriteV(sock, reinterpret_cast<const iovec*>(iov), iovcnt);
#ifdef _WIN32
		// See the function above for more info about the usage of gnutls_transport_set_errno() on Windows
		if (ret < 0)
			gnutls_transport_set_errno(session->sess, SocketEngine::IgnoreError() ? EAGAIN : errno);
#endif

		int size = 0;
		for (int i = 0; i < iovcnt; i++)
			size += iov[i].iov_len;

		if (ret < size)
			SocketEngine::ChangeEventMask(sock, FD_WRITE_WILL_BLOCK);
		return ret;
	}

#else // INSPIRCD_GNUTLS_HAS_VECTOR_PUSH
	static ssize_t gnutls_push_wrapper(gnutls_transport_ptr_t session_wrap, const void* buffer, size_t size)
	{
		StreamSocket* sock = reinterpret_cast<StreamSocket*>(session_wrap);
#ifdef _WIN32
		GnuTLSIOHook* session = static_cast<GnuTLSIOHook*>(sock->GetModHook(thismod));
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
#endif // INSPIRCD_GNUTLS_HAS_VECTOR_PUSH

 public:
	GnuTLSIOHook(IOHookProvider* hookprov, StreamSocket* sock, inspircd_gnutls_session_init_flags_t flags)
		: SSLIOHook(hookprov)
		, sess(NULL)
#ifdef INSPIRCD_GNUTLS_HAS_CORK
		, gbuffersize(0)
#endif
	{
		gnutls_init(&sess, flags);
		gnutls_transport_set_ptr(sess, reinterpret_cast<gnutls_transport_ptr_t>(sock));
#ifdef INSPIRCD_GNUTLS_HAS_VECTOR_PUSH
		gnutls_transport_set_vec_push_function(sess, VectorPush);
#else
		gnutls_transport_set_push_function(sess, gnutls_push_wrapper);
#endif
		gnutls_transport_set_pull_function(sess, gnutls_pull_wrapper);
		GetProfile().SetupSession(sess);

		sock->AddIOHook(this);
		Handshake(sock);
	}

	void OnStreamSocketClose(StreamSocket* user) CXX11_OVERRIDE
	{
		CloseSession();
	}

	int OnStreamSocketRead(StreamSocket* user, std::string& recvq) CXX11_OVERRIDE
	{
		// Finish handshake if needed
		int prepret = PrepareIO(user);
		if (prepret <= 0)
			return prepret;

		// If we resumed the handshake then this->status will be STATUS_OPEN.
		{
			GnuTLS::DataReader reader(sess);
			int ret = reader.ret();
			if (ret > 0)
			{
				reader.appendto(recvq);
				// Schedule a read if there is still data in the GnuTLS buffer
				if (gnutls_record_check_pending(sess) > 0)
					SocketEngine::ChangeEventMask(user, FD_ADD_TRIAL_READ);
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
	}

	int OnStreamSocketWrite(StreamSocket* user, StreamSocket::SendQueue& sendq) CXX11_OVERRIDE
	{
		// Finish handshake if needed
		int prepret = PrepareIO(user);
		if (prepret <= 0)
			return prepret;

		// Session is ready for transferring application data

#ifdef INSPIRCD_GNUTLS_HAS_CORK
		while (true)
		{
			// If there is something in the GnuTLS buffer try to send() it
			int ret = FlushBuffer(user);
			if (ret <= 0)
				return ret; // Couldn't flush entire buffer, retry later (or close on error)

			// GnuTLS buffer is empty, if the sendq is empty as well then break to set FD_WANT_NO_WRITE
			if (sendq.empty())
				break;

			// GnuTLS buffer is empty but sendq is not, begin sending data from the sendq
			gnutls_record_cork(this->sess);
			while ((!sendq.empty()) && (gbuffersize < GetProfile().GetOutgoingRecordSize()))
			{
				const StreamSocket::SendQueue::Element& elem = sendq.front();
				gbuffersize += elem.length();
				ret = gnutls_record_send(this->sess, elem.data(), elem.length());
				if (ret < 0)
				{
					CloseSession();
					return -1;
				}
				sendq.pop_front();
			}
		}
#else
		int ret = 0;

		while (!sendq.empty())
		{
			FlattenSendQueue(sendq, GetProfile().GetOutgoingRecordSize());
			const StreamSocket::SendQueue::Element& buffer = sendq.front();
			ret = HandleWriteRet(user, gnutls_record_send(this->sess, buffer.data(), buffer.length()));

			if (ret <= 0)
				return ret;
			else if (ret < (int)buffer.length())
			{
				sendq.erase_front(ret);
				SocketEngine::ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
				return 0;
			}

			// Wrote entire record, continue sending
			sendq.pop_front();
		}
#endif

		SocketEngine::ChangeEventMask(user, FD_WANT_NO_WRITE);
		return 1;
	}

	void GetCiphersuite(std::string& out) const CXX11_OVERRIDE
	{
		if (!IsHookReady())
			return;
		out.append(UnknownIfNULL(gnutls_protocol_get_name(gnutls_protocol_get_version(sess)))).push_back('-');
		out.append(UnknownIfNULL(gnutls_kx_get_name(gnutls_kx_get(sess)))).push_back('-');
		out.append(UnknownIfNULL(gnutls_cipher_get_name(gnutls_cipher_get(sess)))).push_back('-');
		out.append(UnknownIfNULL(gnutls_mac_get_name(gnutls_mac_get(sess))));
	}

	bool GetServerName(std::string& out) const CXX11_OVERRIDE
	{
		std::vector<char> nameBuffer(1);
		size_t nameLength = 0;
		unsigned int nameType = GNUTLS_NAME_DNS;

		// First, determine the size of the hostname.
		if (gnutls_server_name_get(sess, &nameBuffer[0], &nameLength, &nameType, 0) != GNUTLS_E_SHORT_MEMORY_BUFFER)
			return false;

		// Then retrieve the hostname.
		nameBuffer.resize(nameLength);
		if (gnutls_server_name_get(sess, &nameBuffer[0], &nameLength, &nameType, 0) != GNUTLS_E_SUCCESS)
			return false;

		out.append(&nameBuffer[0]);
		return true;
	}

	GnuTLS::Profile& GetProfile();
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
	GnuTLS::X509Credentials& cred = static_cast<GnuTLSIOHook*>(sock->GetModHook(thismod))->GetProfile().GetX509Credentials();

	st->ncerts = cred.certs.size();
	st->cert.x509 = cred.certs.raw();
	st->key.x509 = cred.key.get();
	st->deinit_all = 0;

	return 0;
}

class GnuTLSIOHookProvider : public SSLIOHookProvider
{
	GnuTLS::Profile profile;

 public:
	GnuTLSIOHookProvider(Module* mod, GnuTLS::Profile::Config& config)
		: SSLIOHookProvider(mod, config.name)
		, profile(config)
	{
		ServerInstance->Modules->AddService(*this);
	}

	~GnuTLSIOHookProvider()
	{
		ServerInstance->Modules->DelService(*this);
	}

	void OnAccept(StreamSocket* sock, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		new GnuTLSIOHook(this, sock, GNUTLS_SERVER);
	}

	void OnConnect(StreamSocket* sock) CXX11_OVERRIDE
	{
		new GnuTLSIOHook(this, sock, GNUTLS_CLIENT);
	}

	GnuTLS::Profile& GetProfile() { return profile; }
};

GnuTLS::Profile& GnuTLSIOHook::GetProfile()
{
	IOHookProvider* hookprov = prov;
	return static_cast<GnuTLSIOHookProvider*>(hookprov)->GetProfile();
}

class ModuleSSLGnuTLS : public Module
{
	typedef std::vector<reference<GnuTLSIOHookProvider> > ProfileList;

	// First member of the class, gets constructed first and destructed last
	GnuTLS::Init libinit;
	ProfileList profiles;

	void ReadProfiles()
	{
		// First, store all profiles in a new, temporary container. If no problems occur, swap the two
		// containers; this way if something goes wrong we can go back and continue using the current profiles,
		// avoiding unpleasant situations where no new TLS (SSL) connections are possible.
		ProfileList newprofiles;

		ConfigTagList tags = ServerInstance->Config->ConfTags("sslprofile");
		if (tags.first == tags.second)
		{
			// No <sslprofile> tags found, create a profile named "gnutls" from settings in the <gnutls> block
			const std::string defname = "gnutls";
			ConfigTag* tag = ServerInstance->Config->ConfValue(defname);
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "No <sslprofile> tags found; using settings from the deprecated <gnutls> tag");

			try
			{
				GnuTLS::Profile::Config profileconfig(defname, tag);
				newprofiles.push_back(new GnuTLSIOHookProvider(this, profileconfig));
			}
			catch (CoreException& ex)
			{
				throw ModuleException("Error while initializing the default TLS (SSL) profile - " + ex.GetReason());
			}
		}
		else
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "You have defined an <sslprofile> tag; you should use this in place of \"gnutls\" when configuring TLS (SSL) connections in <bind:sslprofile> or <link:sslprofile>");
			for (ConfigIter i = tags.first; i != tags.second; ++i)
			{
				ConfigTag* tag = i->second;
				if (!stdalgo::string::equalsci(tag->getString("provider"), "gnutls"))
				{
					ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Ignoring non-GnuTLS <sslprofile> tag at " + tag->getTagLocation());
					continue;
				}

				std::string name = tag->getString("name");
				if (name.empty())
				{
					ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Ignoring <sslprofile> tag without name at " + tag->getTagLocation());
					continue;
				}

				reference<GnuTLSIOHookProvider> prov;
				try
				{
					GnuTLS::Profile::Config profileconfig(name, tag);
					prov = new GnuTLSIOHookProvider(this, profileconfig);
				}
				catch (CoreException& ex)
				{
					throw ModuleException("Error while initializing TLS (SSL) profile \"" + name + "\" at " + tag->getTagLocation() + " - " + ex.GetReason());
				}

				newprofiles.push_back(prov);
			}
		}

		// New profiles are ok, begin using them
		// Old profiles are deleted when their refcount drops to zero
		for (ProfileList::iterator i = profiles.begin(); i != profiles.end(); ++i)
		{
			GnuTLSIOHookProvider& prov = **i;
			ServerInstance->Modules.DelService(prov);
		}

		profiles.swap(newprofiles);
	}

 public:
	ModuleSSLGnuTLS()
	{
#ifndef GNUTLS_HAS_RND
		gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
#endif
		thismod = this;
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "GnuTLS lib version %s module was compiled for " GNUTLS_VERSION, gnutls_check_version(NULL));
		ServerInstance->GenRandom = RandGen::Call;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("gnutls");
		if (status.initial || tag->getBool("onrehash"))
			ReadProfiles();
	}

	void OnModuleRehash(User* user, const std::string &param) CXX11_OVERRIDE
	{
		if (!irc::equals(param, "tls") && !irc::equals(param, "ssl"))
			return;

		try
		{
			ReadProfiles();
			ServerInstance->SNO->WriteToSnoMask('a', "GnuTLS TLS (SSL) profiles have been reloaded.");
		}
		catch (ModuleException& ex)
		{
			ServerInstance->SNO->WriteToSnoMask('a', "Failed to reload the GnuTLS TLS (SSL) profiles. " + ex.GetReason());
		}
	}

	~ModuleSSLGnuTLS()
	{
		ServerInstance->GenRandom = &InspIRCd::DefaultGenRandom;
	}

	void OnCleanup(ExtensionItem::ExtensibleType type, Extensible* item) CXX11_OVERRIDE
	{
		if (type == ExtensionItem::EXT_USER)
		{
			LocalUser* user = IS_LOCAL(static_cast<User*>(item));

			if ((user) && (user->eh.GetModHook(this)))
			{
				// User is using TLS (SSL), they're a local user, and they're using one of *our* TLS (SSL) ports.
				// Potentially there could be multiple TLS (SSL) modules loaded at once on different ports.
				ServerInstance->Users->QuitUser(user, "GnuTLS module unloading");
			}
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows TLS (SSL) encrypted connections using the GnuTLS library.", VF_VENDOR);
	}

	ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE
	{
		const GnuTLSIOHook* const iohook = static_cast<GnuTLSIOHook*>(user->eh.GetModHook(this));
		if ((iohook) && (!iohook->IsHookReady()))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleSSLGnuTLS)
