/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013-2014, 2016-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2017 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 John Brooks <john@jbrooks.io>
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
/// $LinkerFlags: find_linker_flags("gnutls")

/// $PackageInfo: require_system("arch") gnutls pkgconf
/// $PackageInfo: require_system("alpine") gnutls-dev gnutls-utils pkgconf
/// $PackageInfo: require_system("darwin") gnutls pkg-config
/// $PackageInfo: require_system("debian~") gnutls-bin libgnutls28-dev pkg-config
/// $PackageInfo: require_system("rhel~") gnutls-devel pkgconfig

#include "inspircd.h"
#include "modules/ssl.h"
#include "stringutils.h"
#include "timeutils.h"
#include "utility/string.h"

#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/crypto.h>
#include <gnutls/x509.h>

// Check if the GnuTLS library is at least version major.minor.patch
#define INSPIRCD_GNUTLS_HAS_VERSION(major, minor, patch) (GNUTLS_VERSION_NUMBER >= ((major << 16) | (minor << 8) | patch))

#if !INSPIRCD_GNUTLS_HAS_VERSION(3, 3, 5)
# error GnuTLS 3.3.5 or newer is required by the ssl_openssl module.
#elif INSPIRCD_GNUTLS_HAS_VERSION(3, 6, 0)
# define GNUTLS_AUTO_DH
#endif

#ifdef _WIN32
# pragma comment(lib, "libgnutls-30.lib")
#endif

static Module* thismod;

namespace GnuTLS
{
	void GenRandom(char* buffer, size_t len)
	{
		gnutls_rnd(GNUTLS_RND_RANDOM, buffer, len);
	}

	class Init final
	{
	public:
		Init() { gnutls_global_init(); }
		~Init() { gnutls_global_deinit(); }
	};

	class Exception final
		: public ModuleException
	{
	public:
		Exception(const std::string& msg)
			: ModuleException(thismod, msg)
		{
		}
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
	class Datum final
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

	class Hash final
	{
		std::vector<std::pair<gnutls_digest_algorithm_t, bool>> hashes;

	public:
		// Nothing to deallocate, constructor may throw freely
		Hash(const std::string& hashlist)
		{
			irc::spacesepstream hashstream(hashlist);
			for (std::string hashname; hashstream.GetToken(hashname); )
			{
				bool spki = false;
				if (!hashname.compare(0, 5, "spki-", 5) && hashname.length() > 5)
				{
					spki = true;
					hashname.erase(0, 5);
				}

				auto hash = gnutls_digest_get_id(hashname.c_str());
				if (hash == GNUTLS_DIG_UNKNOWN)
					throw Exception("Unknown hash type " + hashname);

				// Check if the user is giving us something that is a valid MAC but not digest
				gnutls_hash_hd_t is_digest;
				if (gnutls_hash_init(&is_digest, hash) < 0)
					throw Exception("Unknown hash type " + hashname);
				gnutls_hash_deinit(is_digest, nullptr);

				hashes.emplace_back(hash, spki);
			}
		}

		std::vector<std::pair<gnutls_digest_algorithm_t, bool>> get() const { return hashes; }
	};

#ifndef GNUTLS_AUTO_DH
	class DHParams final
	{
		gnutls_dh_params_t dh_params;

		DHParams()
		{
			ThrowOnError(gnutls_dh_params_init(&dh_params), "gnutls_dh_params_init() failed");
		}

	public:
		/** Import */
		static std::shared_ptr<DHParams> Import(const std::string& dhstr)
		{
			std::shared_ptr<DHParams> dh(new DHParams);
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

	class X509Key final
	{
		/** Ensure that the key is deinited in case the constructor of X509Key throws
		 */
		class RAIIKey final
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

	class X509CertList final
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
			for (auto* cert : certs)
				gnutls_x509_crt_deinit(cert);
		}

		gnutls_x509_crt_t* raw() { return certs.data(); }
		size_t size() const { return certs.size(); }
	};

	class X509CRL final
	{
		class RAIICRL final
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

	class Priority final
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
				if (gnutls_priority_init(&test, ret.c_str(), nullptr) < 0)
				{
					// The new token broke the priority string, revert to the previously working one
					ServerInstance->Logs.Debug(MODNAME, "Priority string token not recognized: \"{}\"", token);
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

	class CertCredentials
	{
#ifndef GNUTLS_AUTO_DH
		/** DH parameters associated with these credentials
		 */
		std::shared_ptr<DHParams> dh;
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
		void SetDH(const std::shared_ptr<DHParams>& DH)
		{
			dh = DH;
			gnutls_certificate_set_dh_params(cred, dh->get());
		}
#endif
	};

	class X509Credentials final
		: public CertCredentials
	{
		/** Private key
		 */
		X509Key key;

		/** Certificate list, presented to the peer
		 */
		X509CertList certs;

		/** Trusted CA, may be NULL
		 */
		std::shared_ptr<X509CertList> trustedca;

		/** Certificate revocation list, may be NULL
		 */
		std::shared_ptr<X509CRL> crl;

		static int cert_callback(gnutls_session_t session, const gnutls_datum_t* req_ca_rdn, int nreqs, const gnutls_pk_algorithm_t* sign_algos, int sign_algos_length, gnutls_retr2_st* st);

	public:
		X509Credentials(const std::string& certstr, const std::string& keystr)
			: key(keystr)
			, certs(certstr)
		{
			// Throwing is ok here, the destructor of Credentials is called in that case
			int ret = gnutls_certificate_set_x509_key(cred, certs.raw(), static_cast<int>(certs.size()), key.get());
			ThrowOnError(ret, "Unable to set cert/key pair");

			gnutls_certificate_set_retrieve_function(cred, cert_callback);
		}

		/** Sets the trusted CA and the certificate revocation list
		 * to use when verifying certificates
		 */
		void SetCA(const std::shared_ptr<X509CertList>& certlist, const std::shared_ptr<X509CRL>& CRL)
		{
			// Do nothing if certlist is NULL
			if (certlist.get())
			{
				int ret = gnutls_certificate_set_x509_trust(cred, certlist->raw(), static_cast<int>(certlist->size()));
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

	class DataReader final
	{
		ssize_t retval;
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
			gnutls_packet_get(packet, &datum, nullptr);
			recvq.append(reinterpret_cast<const char*>(datum.data), datum.size);

			gnutls_packet_deinit(packet);
		}

		ssize_t ret() const { return retval; }
	};

	class Profile final
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
			auto file = ServerInstance->Config->ReadFile(filename, ServerInstance->Time());
			if (!file)
				throw Exception("Cannot read file " + filename + ": " + file.error);
			return file.contents;
		}

		static std::string GetPrioStr(const std::string& profilename, const std::shared_ptr<ConfigTag>& tag)
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
					ServerInstance->Logs.Normal(MODNAME, "Priority string for profile \"{}\" contains unknown tokens and stripping it didn't yield a working one either, falling back to \"{}\"", profilename, stripped);
				}
				else if ((found) && (stripped != priostr))
				{
					// Prio string was set in the config and we ended up with something that works but different
					ServerInstance->Logs.Warning(MODNAME, "Priority string for profile \"{}\" contains unknown tokens, stripped to \"{}\"", profilename, stripped);
				}
				priostr.swap(stripped);
			}
			return priostr;
		}

	public:
		struct Config final
		{
			std::string name;

			std::shared_ptr<X509CertList> ca;
			std::shared_ptr<X509CRL> crl;

			std::string certstr;
			std::string keystr;
#ifndef GNUTLS_AUTO_DH
			std::shared_ptr<DHParams> dh;
#endif

			std::string priostr;
			unsigned int mindh;
			std::string hashstr;

			unsigned int outrecsize;
			bool requestclientcert;

			Config(const std::string& profilename, const std::shared_ptr<ConfigTag>& tag)
				: name(profilename)
				, certstr(ReadFile(tag->getString("certfile", "cert.pem", 1)))
				, keystr(ReadFile(tag->getString("keyfile", "key.pem", 1)))
#ifndef GNUTLS_AUTO_DH
				, dh(DHParams::Import(ReadFile(tag->getString("dhfile", "dhparams.pem", 1))))
#endif
				, priostr(GetPrioStr(profilename, tag))
				, mindh(tag->getNum<unsigned int>("mindhbits", 1024))
				, hashstr(tag->getString("hash", "sha256 md5", 1))
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

				outrecsize = tag->getNum<unsigned int>("outrecsize", 2048, 512);
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
		std::vector<std::pair<gnutls_digest_algorithm_t, bool>> GetHash() const { return hash.get(); }
		unsigned int GetOutgoingRecordSize() const { return outrecsize; }
	};
}

class GnuTLSIOHook final
	: public SSLIOHook
{
private:
	gnutls_session_t sess = nullptr;
	size_t gbuffersize = 0;

	void CloseSession()
	{
		if (this->sess)
		{
			gnutls_bye(this->sess, GNUTLS_SHUT_WR);
			gnutls_deinit(this->sess);
		}
		sess = nullptr;
		certificate = nullptr;
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

	int GetCertFP(gnutls_x509_crt_t& cert, gnutls_digest_algorithm_t algo, char* buffer, size_t& buffer_size)
	{
		return gnutls_x509_crt_get_fingerprint(cert, algo, buffer, &buffer_size);
	}

	int GetSPKIFP(gnutls_x509_crt_t& cert, gnutls_digest_algorithm_t algo, char* buffer, size_t& buffer_size)
	{
		int ret;
		gnutls_pubkey_t pubkey;

		if ((ret = gnutls_pubkey_init(&pubkey)) != GNUTLS_E_SUCCESS)
			return ret;

		if ((ret = gnutls_pubkey_import_x509(pubkey, cert, 0)) != GNUTLS_E_SUCCESS)
		{
			gnutls_pubkey_deinit(pubkey);
			return ret;
		}

		// We need to do this twice in order to find the size of the public key.
		size_t derkey_size = 0;
		if (gnutls_pubkey_export(pubkey, GNUTLS_X509_FMT_DER, buffer, &derkey_size) != GNUTLS_E_SHORT_MEMORY_BUFFER)
			return GNUTLS_E_SHORT_MEMORY_BUFFER;

		std::vector<char> derkey(derkey_size);
		if ((ret = gnutls_pubkey_export(pubkey, GNUTLS_X509_FMT_DER, &derkey[0], &derkey_size)) != GNUTLS_E_SUCCESS)
			return ret;

		gnutls_pubkey_deinit(pubkey);
		if (gnutls_hash_fast(algo, &derkey[0], derkey_size, buffer) != GNUTLS_E_SUCCESS)
			return ret;

		buffer_size = gnutls_hash_get_len(algo);
		return GNUTLS_E_SUCCESS;
	}

	void VerifyCertificate()
	{
		auto* certinfo = new ssl_cert();
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
		if (!cert_list)
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


		for (const auto& [hash, spki] : GetProfile().GetHash())
		{
			buffer_size = sizeof(buffer);
			ret = spki ? GetSPKIFP(cert, hash, buffer, buffer_size) : GetCertFP(cert, hash, buffer, buffer_size);
			if (ret != GNUTLS_E_SUCCESS)
			{
				certinfo->error = gnutls_strerror(ret);
			}
			else
			{
				certinfo->fingerprints.push_back(Hex::Encode(buffer, buffer_size));
			}
		}

		certinfo->activation = gnutls_x509_crt_get_activation_time(cert);
		if (certinfo->activation == -1)
		{
			certinfo->activation = 0;
			certinfo->error = "Unable to check certificate activation time";
		}
		else if (certinfo->activation >= ServerInstance->Time())
		{
			certinfo->error = INSP_FORMAT("Certificate not active for {} (on {})",
				Duration::ToString(certinfo->activation - ServerInstance->Time()),
				Time::ToString(certinfo->activation));
		}

		certinfo->expiration = gnutls_x509_crt_get_expiration_time(cert);
		if (certinfo->expiration == -1)
		{
			certinfo->expiration = 0;
			certinfo->error = "Unable to check certificate expiration time";
		}
		else if (certinfo->expiration <= ServerInstance->Time())
		{
			certinfo->error = INSP_FORMAT("Certificate expired {} ago (on {})",
				Duration::ToString(ServerInstance->Time() - certinfo->expiration),
				Time::ToString(certinfo->expiration));
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
		sock->SetError("No TLS session");
		return -1;
	}

	int FlushBuffer(StreamSocket* sock)
	{
		// If GnuTLS has some data buffered, write it
		if (gbuffersize)
			return HandleWriteRet(sock, gnutls_record_uncork(this->sess, 0));
		return 1;
	}

	int HandleWriteRet(StreamSocket* sock, int ret)
	{
		if (ret > 0)
		{
			gbuffersize -= ret;
			if (gbuffersize)
			{
				SocketEngine::ChangeEventMask(sock, FD_WANT_SINGLE_WRITE);
				return 0;
			}
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

		ssize_t rv = SocketEngine::Recv(sock, reinterpret_cast<char *>(buffer), size, 0);

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

		if (rv < 0 || size_t(rv) < size)
			SocketEngine::ChangeEventMask(sock, FD_READ_WILL_BLOCK);
		return rv;
	}
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
		ssize_t ret = SocketEngine::WriteV(sock, reinterpret_cast<const iovec*>(iov), iovcnt);
#ifdef _WIN32
		// See the function above for more info about the usage of gnutls_transport_set_errno() on Windows
		if (ret < 0)
			gnutls_transport_set_errno(session->sess, SocketEngine::IgnoreError() ? EAGAIN : errno);
#endif

		ssize_t size = 0;
		for (int i = 0; i < iovcnt; i++)
			size += iov[i].iov_len;

		if (ret < size)
			SocketEngine::ChangeEventMask(sock, FD_WRITE_WILL_BLOCK);
		return ret;
	}

public:
	GnuTLSIOHook(const std::shared_ptr<IOHookProvider>& hookprov, StreamSocket* sock, unsigned int flags)
		: SSLIOHook(hookprov)
	{
		gnutls_init(&sess, flags);
		gnutls_transport_set_ptr(sess, reinterpret_cast<gnutls_transport_ptr_t>(sock));
		gnutls_transport_set_vec_push_function(sess, VectorPush);
		gnutls_transport_set_pull_function(sess, gnutls_pull_wrapper);
		GetProfile().SetupSession(sess);

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

		// If we resumed the handshake then this->status will be STATUS_OPEN.
		{
			GnuTLS::DataReader reader(sess);
			ssize_t ret = reader.ret();
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
				user->SetError(gnutls_strerror(int(ret)));
				CloseSession();
				return -1;
			}
		}
	}

	ssize_t OnStreamSocketWrite(StreamSocket* user, StreamSocket::SendQueue& sendq) override
	{
		// Finish handshake if needed
		int prepret = PrepareIO(user);
		if (prepret <= 0)
			return prepret;

		// Session is ready for transferring application data

		while (true)
		{
			// If there is something in the GnuTLS buffer try to send() it
			ssize_t ret = FlushBuffer(user);
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

		SocketEngine::ChangeEventMask(user, FD_WANT_NO_WRITE);
		return 1;
	}

	void GetCiphersuite(std::string& out) const override
	{
		if (!IsHookReady())
			return;
		out.append(UnknownIfNULL(gnutls_protocol_get_name(gnutls_protocol_get_version(sess)))).push_back('-');
		out.append(UnknownIfNULL(gnutls_kx_get_name(gnutls_kx_get(sess)))).push_back('-');
		out.append(UnknownIfNULL(gnutls_cipher_get_name(gnutls_cipher_get(sess)))).push_back('-');
		out.append(UnknownIfNULL(gnutls_mac_get_name(gnutls_mac_get(sess))));
	}

	bool GetServerName(std::string& out) const override
	{
		std::vector<char> nameBuffer(1);
		size_t nameLength = 0;
		unsigned int nameType = GNUTLS_NAME_DNS;

		// First, determine the size of the hostname.
		if (gnutls_server_name_get(sess, nameBuffer.data(), &nameLength, &nameType, 0) != GNUTLS_E_SHORT_MEMORY_BUFFER)
			return false;

		// Then retrieve the hostname.
		nameBuffer.resize(nameLength);
		if (gnutls_server_name_get(sess, nameBuffer.data(), &nameLength, &nameType, 0) != GNUTLS_E_SUCCESS)
			return false;

		out.append(nameBuffer.data());
		return true;
	}

	GnuTLS::Profile& GetProfile();
};

int GnuTLS::X509Credentials::cert_callback(gnutls_session_t sess, const gnutls_datum_t* req_ca_rdn, int nreqs, const gnutls_pk_algorithm_t* sign_algos, int sign_algos_length, gnutls_retr2_st* st)
{
	st->cert_type = GNUTLS_CRT_X509;
	st->key_type = GNUTLS_PRIVKEY_X509;

	StreamSocket* sock = reinterpret_cast<StreamSocket*>(gnutls_transport_get_ptr(sess));
	GnuTLS::X509Credentials& cred = static_cast<GnuTLSIOHook*>(sock->GetModHook(thismod))->GetProfile().GetX509Credentials();

	st->ncerts = static_cast<unsigned int>(cred.certs.size());
	st->cert.x509 = cred.certs.raw();
	st->key.x509 = cred.key.get();
	st->deinit_all = 0;

	return 0;
}

class GnuTLSIOHookProvider final
	: public SSLIOHookProvider
{
	GnuTLS::Profile profile;

public:
	GnuTLSIOHookProvider(Module* mod, GnuTLS::Profile::Config& config)
		: SSLIOHookProvider(mod, config.name)
		, profile(config)
	{
		ServerInstance->Modules.AddService(*this);
	}

	~GnuTLSIOHookProvider() override
	{
		ServerInstance->Modules.DelService(*this);
	}

	void OnAccept(StreamSocket* sock, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server) override
	{
		new GnuTLSIOHook(shared_from_this(), sock, GNUTLS_SERVER);
	}

	void OnConnect(StreamSocket* sock) override
	{
		new GnuTLSIOHook(shared_from_this(), sock, GNUTLS_CLIENT);
	}

	GnuTLS::Profile& GetProfile() { return profile; }
};

GnuTLS::Profile& GnuTLSIOHook::GetProfile()
{
	return std::static_pointer_cast<GnuTLSIOHookProvider>(prov)->GetProfile();
}

class ModuleSSLGnuTLS final
	: public Module
{
	typedef std::vector<std::shared_ptr<GnuTLSIOHookProvider>> ProfileList;

	// First member of the class, gets constructed first and destructed last
	GnuTLS::Init libinit;
	ProfileList profiles;
	std::function<void(char*, size_t)> rememberer;

	void ReadProfiles()
	{
		// First, store all profiles in a new, temporary container. If no problems occur, swap the two
		// containers; this way if something goes wrong we can go back and continue using the current profiles,
		// avoiding unpleasant situations where no new TLS connections are possible.
		ProfileList newprofiles;

		auto tags = ServerInstance->Config->ConfTags("sslprofile");
		if (tags.empty())
			throw ModuleException(this, "You have not specified any <sslprofile> tags that are usable by this module!");

		for (const auto& [_, tag] : tags)
		{
			if (!insp::equalsci(tag->getString("provider", "gnutls", 1), "gnutls"))
			{
				ServerInstance->Logs.Debug(MODNAME, "Ignoring non-GnuTLS <sslprofile> tag at {}", tag->source.str());
				continue;
			}

			const std::string name = tag->getString("name");
			if (name.empty())
			{
				ServerInstance->Logs.Warning(MODNAME, "Ignoring <sslprofile> tag without name at {}", tag->source.str());
				continue;
			}

			std::shared_ptr<GnuTLSIOHookProvider> prov;
			try
			{
				GnuTLS::Profile::Config profileconfig(name, tag);
				prov = std::make_shared<GnuTLSIOHookProvider>(this, profileconfig);
			}
			catch (const CoreException& ex)
			{
				throw ModuleException(this, "Error while initializing TLS profile \"" + name + "\" at " + tag->source.str() + " - " + ex.GetReason());
			}

			newprofiles.push_back(prov);
		}

		// New profiles are ok, begin using them
		// Old profiles are deleted when their refcount drops to zero
		for (const auto& profile : profiles)
			ServerInstance->Modules.DelService(*profile);

		profiles.swap(newprofiles);
	}

public:
	ModuleSSLGnuTLS()
		: Module(VF_VENDOR, "Allows TLS encrypted connections using the GnuTLS library.")
		, rememberer(ServerInstance->GenRandom)
	{
		thismod = this;
	}

	void init() override
	{
		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against GnuTLS version {} and is running against version {}",
			GNUTLS_VERSION, gnutls_check_version(nullptr));
		ServerInstance->GenRandom = &GnuTLS::GenRandom;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("gnutls");
		if (status.initial || tag->getBool("onrehash", true))
		{
			// Try to help people who have outdated configs.
			for (const auto& field : {"cafile", "certfile", "crlfile", "dhfile", "hash", "keyfile", "mindhbits", "outrecsize", "priority", "requestclientcert", "strictpriority"})
			{
				if (!tag->getString(field).empty())
					throw ModuleException(this, "TLS settings have moved from <gnutls> to <sslprofile>. See " INSPIRCD_DOCS "modules/ssl_gnutls/#sslprofile for more information.");
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
			ServerInstance->SNO.WriteToSnoMask('r', "GnuTLS TLS profiles have been reloaded.");
		}
		catch (const ModuleException& ex)
		{
			ServerInstance->SNO.WriteToSnoMask('r', "Failed to reload the GnuTLS TLS profiles. " + ex.GetReason());
		}
	}

	~ModuleSSLGnuTLS() override
	{
		ServerInstance->GenRandom = rememberer;
	}

	void OnCleanup(ExtensionType type, Extensible* item) override
	{
		if (type == ExtensionType::USER)
		{
			LocalUser* user = IS_LOCAL(static_cast<User*>(item));

			if ((user) && (user->eh.GetModHook(this)))
			{
				// User is using TLS, they're a local user, and they're using one of *our* TLS ports.
				// Potentially there could be multiple TLS modules loaded at once on different ports.
				ServerInstance->Users.QuitUser(user, "GnuTLS module unloading");
			}
		}
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		const GnuTLSIOHook* const iohook = static_cast<GnuTLSIOHook*>(user->eh.GetModHook(this));
		if ((iohook) && (!iohook->IsHookReady()))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleSSLGnuTLS)
