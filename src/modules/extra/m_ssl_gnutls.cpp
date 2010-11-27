/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gcrypt.h>
#include "ssl.h"
#include "m_cap.h"

#ifdef WINDOWS
#pragma comment(lib, "libgnutls-13.lib")
#endif

/* $ModDesc: Provides SSL support for clients */
/* $CompileFlags: pkgconfincludes("gnutls","/gnutls/gnutls.h","") */
/* $LinkerFlags: rpath("pkg-config --libs gnutls") pkgconflibs("gnutls","/libgnutls.so","-lgnutls") -lgcrypt */

namespace GnuTLS
{

enum issl_status { ISSL_NONE, ISSL_HANDSHAKING, ISSL_HANDSHAKEN, ISSL_CLOSING, ISSL_CLOSED };

static gnutls_digest_algorithm_t hash;

#define GNUTLS_HAS_PRIORITY (GNUTLS_VERSION_MAJOR > 2 || (GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR >= 2))

static int cert_callback (gnutls_session_t session, const gnutls_datum_t * req_ca_rdn, int nreqs,
	const gnutls_pk_algorithm_t * sign_algos, int sign_algos_length, gnutls_retr_st * st);

struct DH_info : public refcountbase
{
	int bits;
	gnutls_dh_params params;
	DH_info(int Bits) : bits(Bits)
	{
		gnutls_dh_params_init(&params);
		int ret = gnutls_dh_params_generate2(params, bits);
		if (ret < 0)
			ServerInstance->Logs->Log("m_ssl_gnutls",DEFAULT, "m_ssl_gnutls.so: Failed to generate DH parameters (%d bits): %s",
				bits, gnutls_strerror(ret));
	}

	~DH_info()
	{
		gnutls_dh_params_deinit(params);
	}
};

struct x509_key : public refcountbase
{
	gnutls_x509_privkey_t x509;

	x509_key(const std::string& pem)
	{
		gnutls_datum_t key_datum = { (unsigned char*)pem.data(), pem.length() };
		int ret = gnutls_x509_privkey_init(&x509);
		if (ret < 0)
			throw ModuleException("GnuTLS memory allocation error " + std::string(gnutls_strerror(ret)));
		ret = gnutls_x509_privkey_import(x509, &key_datum, GNUTLS_X509_FMT_PEM);
		if (ret < 0)
			throw ModuleException("Unable to load GnuTLS server private key: " + std::string(gnutls_strerror(ret)));
	}

	~x509_key()
	{
		gnutls_x509_privkey_deinit(x509);
	}
};

struct x509_crt_list : public refcountbase
{
	std::vector<gnutls_x509_crt_t> list;

	x509_crt_list(const std::string& pem)
	{
		gnutls_datum_t cert_datum = { (unsigned char*)pem.data(), pem.length() };

		unsigned int certcount = 100;
		list.resize(certcount);

		int ret = gnutls_x509_crt_list_import(data(), &certcount, &cert_datum, GNUTLS_X509_FMT_PEM, GNUTLS_X509_CRT_LIST_IMPORT_FAIL_IF_EXCEED);
		if (ret < 0)
			throw ModuleException("Unable to load GnuTLS server certificates: " + std::string(gnutls_strerror(ret)));
		list.resize(certcount);
	}

	inline gnutls_x509_crt_t* data()
	{
		return &list[0];
	}

	inline int count()
	{
		return list.size();
	}

	~x509_crt_list()
	{
		for(unsigned int i=0; i < list.size(); i++)
			gnutls_x509_crt_deinit(list[i]);
	}
};

struct x509_crl_list : public refcountbase
{
	std::vector<gnutls_x509_crl_t> list;

	x509_crl_list(const std::string& pem)
	{
		std::string::size_type pos = 0;
		while (1)
		{
			pos = pem.find("-----BEGIN", pos);
			if (pos == std::string::npos)
				break;
			gnutls_x509_crl_t item;
			gnutls_x509_crl_init(&item);
			gnutls_datum_t datum = { (unsigned char*)pem.data() + pos, pem.length() - pos };
			int ret = gnutls_x509_crl_import(item, &datum, GNUTLS_X509_FMT_PEM);
			if (ret < 0)
				throw ModuleException("Unable to load GnuTLS CRL: " + std::string(gnutls_strerror(ret)));
			list.push_back(item);
			pos++;
		}
	}

	inline gnutls_x509_crl_t* data()
	{
		return &list[0];
	}

	inline int count()
	{
		return list.size();
	}

	~x509_crl_list()
	{
		for(unsigned int i=0; i < list.size(); i++)
			gnutls_x509_crl_deinit(list[i]);
	}
};

struct x509_cred : public refcountbase
{
	reference<ConfigTag> tag;
	reference<x509_key> key;
	reference<x509_crt_list> certs;
	reference<x509_crt_list> ca_list;
	reference<x509_crl_list> crl_list;
	reference<DH_info> dh;
	gnutls_certificate_credentials cred;
#if GNUTLS_HAS_PRIORITY
	gnutls_priority_t cipher_prio;
#endif
	bool gnutlsonly;
	x509_cred(ConfigTag* Tag, x509_crt_list* CA, x509_crl_list* CRL, DH_info* DH)
		: tag(Tag), ca_list(CA), crl_list(CRL), dh(DH)
	{
		FileReader reader;

		errno = 0;
		reader.LoadFile(tag->getString("certfile", "conf/cert.pem"));
		std::string cert_string = reader.Contents();

		if (cert_string.empty())
			throw ModuleException("Unable to read GnuTLS server certificates from " +
				tag->getString("certfile", "conf/cert.pem") + ": " + (errno ? strerror(errno) : "Unknown error"));

		errno = 0;
		reader.LoadFile(tag->getString("keyfile", "conf/key.pem"));
		std::string key_string = reader.Contents();

		if (key_string.empty())
			throw ModuleException("Unable to read GnuTLS private key from " +
				tag->getString("keyfile", "conf/key.pem") + ": " + (errno ? strerror(errno) : "Unknown error"));

		key = new x509_key(key_string);
		certs = new x509_crt_list(cert_string);

		std::string ca_fn;
		if (tag->readString("cafile", ca_fn))
		{
			reader.LoadFile(ca_fn);
			std::string ca_string = reader.Contents();
			ca_list = new x509_crt_list(ca_string);
		}

		init();
	}

	x509_cred(x509_cred* src, DH_info* DH)
		: tag(src->tag), key(src->key), certs(src->certs),
		  ca_list(src->ca_list), crl_list(src->crl_list), dh(DH)
	{
		init();
	}

	void init()
	{
		int ret;

		ret = gnutls_certificate_allocate_credentials(&cred);
		if (ret < 0)
			throw ModuleException("GnuTLS memory allocation error " + std::string(gnutls_strerror(ret)));

		ret = gnutls_certificate_set_x509_key(cred, certs->data(), certs->count(), key->x509);
		if (ret < 0)
			throw ModuleException("Unable to set GnuTLS cert/key pair: " + std::string(gnutls_strerror(ret)));

		if (ca_list)
		{
			ret = gnutls_certificate_set_x509_trust(cred, ca_list->data(), ca_list->count());
			if (ret < 0)
				ServerInstance->Logs->Log("m_ssl_gnutls",DEBUG, "m_ssl_gnutls.so: Could not set X.509 trust file: %s", gnutls_strerror(ret));
		}

		if (crl_list)
		{
			ret = gnutls_certificate_set_x509_crl(cred, crl_list->data(), crl_list->count());
			if (ret < 0)
				ServerInstance->Logs->Log("m_ssl_gnutls",DEBUG, "m_ssl_gnutls.so: Failed to set X.509 CRL file: %s", gnutls_strerror(ret));
		}

#if GNUTLS_HAS_PRIORITY
		std::string prios = tag->getString("prio", "NORMAL:+COMP-DEFLATE");
		const char* errpos = NULL;
		ret = gnutls_priority_init(&cipher_prio, prios.c_str(), &errpos);
		if (ret != GNUTLS_E_SUCCESS)
			throw ModuleException("Bad GnuTLS priority settings: " + std::string(gnutls_strerror(ret)));
#endif

		gnutlsonly = tag->getBool("gnutls_only");

		gnutls_certificate_set_dh_params(cred, dh->params);
		gnutls_certificate_client_set_retrieve_function (cred, cert_callback);
	}

	~x509_cred()
	{
#if GNUTLS_HAS_PRIORITY
		gnutls_priority_deinit(cipher_prio);
#endif
		gnutls_certificate_free_credentials(cred);
	}
};

static ssize_t gnutls_pull_wrapper(gnutls_transport_ptr_t user_wrap, void* buffer, size_t size)
{
	StreamSocket* user = reinterpret_cast<StreamSocket*>(user_wrap);
	if (user->GetEventMask() & FD_READ_WILL_BLOCK)
	{
		errno = EAGAIN;
		return -1;
	}
	int rv = recv(user->GetFd(), buffer, size, 0);
	if (rv < 0)
	{
		if (errno == EAGAIN || errno == EINTR)
			ServerInstance->SE->ChangeEventMask(user, FD_READ_WILL_BLOCK);
		else
			user->SetError(strerror(errno));
	}
	else if (rv == 0)
		user->SetError("Connection Closed");
	else if (rv < (int)size)
		ServerInstance->SE->ChangeEventMask(user, FD_READ_WILL_BLOCK);
	return rv;
}

static ssize_t gnutls_push_wrapper(gnutls_transport_ptr_t user_wrap, const void* buffer, size_t size)
{
	StreamSocket* user = reinterpret_cast<StreamSocket*>(user_wrap);
	if (user->GetEventMask() & FD_WRITE_WILL_BLOCK)
	{
		errno = EAGAIN;
		return -1;
	}
	int rv = send(user->GetFd(), buffer, size, 0);
	if (rv < 0)
	{
		if (errno == EAGAIN || errno == EINTR)
			ServerInstance->SE->ChangeEventMask(user, FD_WRITE_WILL_BLOCK);
		else
			user->SetError(strerror(errno));
	}
	else if (rv < (int)size)
		ServerInstance->SE->ChangeEventMask(user, FD_WRITE_WILL_BLOCK);
	return rv;
}

class RandGen : public HandlerBase2<void, char*, size_t>
{
 public:
	RandGen() {}
	void Call(char* buffer, size_t len)
	{
		gcry_randomize(buffer, len, GCRY_STRONG_RANDOM);
	}
};

class GnuTLSHook : public SSLIOHook
{
 public:
	gnutls_session_t sess;
	issl_status status;
	reference<ssl_cert> cert;
	reference<x509_cred> creds;
	GnuTLSHook(Module* Creator, bool client, StreamSocket* user, x509_cred* mycert)
		: SSLIOHook(Creator), sess(NULL), status(ISSL_HANDSHAKING), creds(mycert)
	{
		int rv;
		rv = gnutls_init(&sess, client ? GNUTLS_CLIENT : GNUTLS_SERVER);
		if (rv < 0)
			throw ModuleException("Cannot init");

		if (mycert->gnutlsonly)
			gnutls_handshake_set_private_extensions(sess, 1);

#if GNUTLS_HAS_PRIORITY
		rv = gnutls_priority_set(sess, creds->cipher_prio);
#else
		rv = gnutls_set_default_priority(sess);
#endif
		if (rv < 0)
			throw ModuleException("Cannot set cipher priorities");

		rv = gnutls_credentials_set(sess, GNUTLS_CRD_CERTIFICATE, creds->cred);
		if (rv < 0)
			throw ModuleException("Cannot set credentials");
		gnutls_dh_set_prime_bits(sess, creds->dh->bits);

		gnutls_transport_set_ptr(sess, reinterpret_cast<void*>(user));
		gnutls_transport_set_push_function(sess, gnutls_push_wrapper);
		gnutls_transport_set_pull_function(sess, gnutls_pull_wrapper);

		if (!client)
			gnutls_certificate_server_set_request(sess, GNUTLS_CERT_REQUEST); // Request client certificate if any.

		user->SetIOHook(this);
	}

	~GnuTLSHook()
	{
		OnClose(NULL);
	}

	std::string GetFingerprint()
	{
		if (cert)
			return cert->GetFingerprint();
		return "";
	}

	void OnClose(StreamSocket* user)
	{
		if (sess)
		{
			gnutls_bye(sess, GNUTLS_SHUT_WR);
			gnutls_deinit(sess);
		}
		sess = NULL;
		cert = NULL;
		status = ISSL_NONE;
	}

	void VerifyCertificate(StreamSocket* user)
	{
		unsigned int result;
		const gnutls_datum_t* cert_list;
		int ret;
		unsigned int cert_list_size;
		gnutls_x509_crt_t raw_cert;
		char name[MAXBUF];
		unsigned char digest[MAXBUF];
		size_t digest_size = sizeof(digest);
		size_t name_size = sizeof(name);
		ssl_cert* certinfo = new ssl_cert;
		cert = certinfo;

		/* This verification function uses the trusted CAs in the credentials
		 * structure. So you must have installed one or more CA certificates.
		 */
		ret = gnutls_certificate_verify_peers2(sess, &result);

		if (ret < 0)
		{
			certinfo->error = std::string(gnutls_strerror(ret));
			return;
		}

		certinfo->invalid = (result & GNUTLS_CERT_INVALID);
		certinfo->unknownsigner = (result & GNUTLS_CERT_SIGNER_NOT_FOUND);
		certinfo->revoked = (result & GNUTLS_CERT_REVOKED);
		certinfo->trusted = !(result & GNUTLS_CERT_SIGNER_NOT_CA);

		/* Up to here the process is the same for X.509 certificates and
		 * OpenPGP keys. From now on X.509 certificates are assumed. This can
		 * be easily extended to work with openpgp keys as well.
		 */
		if (gnutls_certificate_type_get(sess) != GNUTLS_CRT_X509)
		{
			certinfo->error = "No X509 keys sent";
			return;
		}

		ret = gnutls_x509_crt_init(&raw_cert);
		if (ret < 0)
		{
			certinfo->error = gnutls_strerror(ret);
			return;
		}

		cert_list_size = 0;
		cert_list = gnutls_certificate_get_peers(sess, &cert_list_size);
		if (cert_list == NULL)
		{
			certinfo->error = "No certificate was found";
		}
		else
		{

			/* This is not a real world example, since we only check the first
			 * certificate in the given chain.
			 */

			ret = gnutls_x509_crt_import(raw_cert, &cert_list[0], GNUTLS_X509_FMT_DER);
			if (ret < 0)
			{
				certinfo->error = gnutls_strerror(ret);
			}
			else
			{
				gnutls_x509_crt_get_dn(raw_cert, name, &name_size);
				certinfo->dn = name;

				gnutls_x509_crt_get_issuer_dn(raw_cert, name, &name_size);
				certinfo->issuer = name;

				if ((ret = gnutls_x509_crt_get_fingerprint(raw_cert, hash, digest, &digest_size)) < 0)
				{
					certinfo->error = gnutls_strerror(ret);
				}
				else
				{
					certinfo->fingerprint = irc::hex(digest, digest_size);
				}

				/* Beware here we do not check for errors.
				 */
				if ((gnutls_x509_crt_get_expiration_time(raw_cert) < ServerInstance->Time()) || (gnutls_x509_crt_get_activation_time(raw_cert) > ServerInstance->Time()))
				{
					certinfo->error = "Not activated, or expired certificate";
				}
			}
		}
		gnutls_x509_crt_deinit(raw_cert);
	}

	int OnRead(StreamSocket* user, std::string& recvq)
	{
		if (!sess)
		{
			user->SetError("No SSL session");
			return -1;
		}

		// gnutls seems to need a bit of prodding to read everything on the socket
		ServerInstance->SE->ChangeEventMask(user, FD_ADD_TRIAL_READ | FD_ADD_TRIAL_WRITE);

		if (status == ISSL_HANDSHAKING)
		{
			// The handshake isn't finished, try to finish it.

			if(!Handshake(user))
			{
				if (status != ISSL_CLOSING)
					return 0;
				return -1;
			}
		}

		// If we resumed the handshake then status will be ISSL_HANDSHAKEN.

		if (status == ISSL_HANDSHAKEN)
		{
			char* buffer = ServerInstance->GetReadBuffer();
			size_t bufsiz = ServerInstance->Config->NetBufferSize;
			int ret = gnutls_record_recv(sess, buffer, bufsiz);
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
				user->SetError("SSL Connection closed");
				OnClose(user);
				return -1;
			}
			else
			{
				user->SetError(gnutls_strerror(ret));
				OnClose(user);
				return -1;
			}
		}
		else if (status == ISSL_CLOSING)
			return -1;

		return 0;
	}

	int OnWrite(StreamSocket* user, std::string& sendq)
	{
		if (!sess)
		{
			user->SetError("No SSL session");
			return -1;
		}

		// gnutls seems to need a bit of prodding to read everything on the socket
		ServerInstance->SE->ChangeEventMask(user, FD_ADD_TRIAL_READ | FD_ADD_TRIAL_WRITE);

		if (status == ISSL_HANDSHAKING)
		{
			// The handshake isn't finished, try to finish it.
			Handshake(user);
			if (status != ISSL_CLOSING)
				return 0;
			return -1;
		}

		int ret = 0;

		if (status == ISSL_HANDSHAKEN)
		{
			ret = gnutls_record_send(sess, sendq.data(), sendq.length());

			if (ret == (int)sendq.length())
			{
				ServerInstance->SE->ChangeEventMask(user, FD_WANT_NO_WRITE);
				return 1;
			}
			else if (ret > 0)
			{
				sendq = sendq.substr(ret);
				ServerInstance->SE->ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED || ret == 0)
			{
				ServerInstance->SE->ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else // (ret < 0)
			{
				user->SetError(gnutls_strerror(ret));
				OnClose(user);
				return -1;
			}
		}

		return 0;
	}

	bool Handshake(StreamSocket* user)
	{
		int ret = gnutls_handshake(sess);

		if (ret < 0)
		{
			if(ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
			{
				// Handshake needs resuming later, read() or write() would have blocked.

				if(gnutls_record_get_direction(sess) == 0)
				{
					// gnutls_handshake() wants to read() again.
					ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				}
				else
				{
					// gnutls_handshake() wants to write() again.
					ServerInstance->SE->ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
				}
			}
			else
			{
				user->SetError(std::string("Handshake Failed - ") + gnutls_strerror(ret));
				status = ISSL_CLOSING;
				OnClose(user);
			}

			return false;
		}
		else
		{
			// Change the seesion state
			status = ISSL_HANDSHAKEN;

			VerifyCertificate(user);

			// Finish writing, if any left
			ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

			return true;
		}
	}

	ssl_cert* GetCertificate()
	{
		return cert;
	}
};

class GnuTLSProvider : public IOHookProvider
{
 public:
	reference<x509_cred> def_creds; 
	std::map<std::string, reference<x509_cred> > creds;

	GnuTLSProvider(Module* Creator) : IOHookProvider(Creator, "ssl/gnutls") {}
	void OnClientConnection(StreamSocket* user, ConfigTag* tag)
	{
		std::string cred = tag->getString("ssl_cert");
		std::map<std::string, reference<x509_cred> >::iterator i = creds.find(cred);
		new GnuTLSHook(creator, true, user, i == creds.end() ? def_creds : i->second);
	}
	void OnServerConnection(StreamSocket* user, ListenSocket* from)
	{
		std::string cred = from ? from->bind_tag->getString("ssl_cert") : "starttls";
		std::map<std::string, reference<x509_cred> >::iterator i = creds.find(cred);
		new GnuTLSHook(creator, false, user, i == creds.end() ? def_creds : i->second);
	}
};

/** Client cert getter */
static int cert_callback (gnutls_session_t session, const gnutls_datum_t * req_ca_rdn, int nreqs,
	const gnutls_pk_algorithm_t * sign_algos, int sign_algos_length, gnutls_retr_st * st) {

	StreamSocket* socket = reinterpret_cast<StreamSocket*>(gnutls_transport_get_ptr(session));
	GnuTLSHook* hook = static_cast<GnuTLSHook*>(socket->GetIOHook());
	st->type = GNUTLS_CRT_X509;
	st->cert.x509 = hook->creds->certs->data();
	st->key.x509 = hook->creds->key->x509;
	st->ncerts = hook->creds->certs->count();
	st->deinit_all = 0;

	return 0;
}

class CommandStartTLS : public SplitCommand
{
 public:
	bool enabled;
	GnuTLSProvider& prov;
	CommandStartTLS (Module* mod, GnuTLSProvider& Prov) : SplitCommand(mod, "STARTTLS"), prov(Prov)
	{
		enabled = true;
		works_before_reg = true;
	}

	CmdResult HandleLocal(const std::vector<std::string> &parameters, LocalUser *user)
	{
		if (!enabled)
		{
			user->WriteNumeric(691, "%s :STARTTLS is not enabled", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (user->registered == REG_ALL)
		{
			user->WriteNumeric(691, "%s :STARTTLS is not permitted after client registration is complete", user->nick.c_str());
		}
		else
		{
			if (!user->eh.GetIOHook())
			{
				user->WriteNumeric(670, "%s :STARTTLS successful, go ahead with TLS handshake", user->nick.c_str());
				/* We need to flush the write buffer prior to adding the IOHook,
				 * otherwise we'll be sending this line inside the SSL session - which
				 * won't start its handshake until the client gets this line. Currently,
				 * we assume the write will not block here; this is usually safe, as
				 * STARTTLS is sent very early on in the registration phase, where the
				 * user hasn't built up much sendq. Handling a blocked write here would
				 * be very annoying.
				 */
				user->eh.DoWrite();
				prov.OnServerConnection(&user->eh, NULL);
			}
			else
				user->WriteNumeric(691, "%s :STARTTLS failure", user->nick.c_str());
		}

		return CMD_FAILURE;
	}
};


class ModuleSSLGnuTLS : public Module
{
	RandGen randhandler;
	GenericCap capHandler;
	GnuTLSProvider iohook;
	CommandStartTLS starttls;

	int once_in_a_while;
	std::string sslports;

	reference<DH_info> dh;

 public:

	ModuleSSLGnuTLS()
		: capHandler(this, "tls"), iohook(this), starttls(this, iohook), once_in_a_while(0)
	{
		gnutls_global_init(); // This must be called once in the program
	}

	void init()
	{
		OnModuleRehash(NULL,"ssl");

		ServerInstance->GenRandom = &randhandler;

		Implementation eventlist[] = { I_On005Numeric, I_OnModuleRehash, I_OnUserConnect, I_OnEvent, I_OnGarbageCollect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		ServerInstance->Modules->AddService(iohook);
		ServerInstance->AddCommand(&starttls);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		sslports.clear();

		ConfigTag* Conf = ServerInstance->Config->GetTag("gnutls");
		starttls.enabled = Conf->getBool("starttls", true);

		if (Conf->getBool("showports", true))
		{
			for (size_t i = 0; i < ServerInstance->ports.size(); i++)
			{
				ListenSocket* port = ServerInstance->ports[i];
				if (port->bind_tag->getString("ssl") != "gnutls")
					continue;

				const std::string& portid = port->bind_desc;
				ServerInstance->Logs->Log("m_ssl_gnutls", DEFAULT, "m_ssl_gnutls.so: Enabling SSL for port %s", portid.c_str());

				if (port->bind_tag->getString("type", "clients") == "clients" && port->bind_addr != "127.0.0.1")
					sslports.append(portid).append(";");
			}

			if (!sslports.empty())
				sslports.erase(sslports.end() - 1);
		}
	}

	void OnModuleRehash(User* user, const std::string &param)
	{
		if(param != "ssl")
			return;

		ConfigTag* Conf = ServerInstance->Config->GetTag("gnutls");

		int dh_bits = Conf->getInt("dhbits");
		std::string hashname = Conf->getString("hash", "md5");

		if((dh_bits != 768) && (dh_bits != 1024) && (dh_bits != 2048) && (dh_bits != 3072) && (dh_bits != 4096))
			dh_bits = 1024;
		dh = new DH_info(dh_bits);

		if (hashname == "md5")
			hash = GNUTLS_DIG_MD5;
		else if (hashname == "sha1")
			hash = GNUTLS_DIG_SHA1;
		else
			throw ModuleException("Unknown hash type " + hashname);

		FileReader reader;
		reference<x509_crt_list> ca_list;
		reference<x509_crl_list> crl_list;

		reader.LoadFile(Conf->getString("cafile", "conf/ca.pem"));
		if (reader.FileSize())
			ca_list = new x509_crt_list(reader.Contents());

		reader.LoadFile(Conf->getString("crlfile", "conf/crl.pem"));
		if (reader.FileSize())
			crl_list = new x509_crl_list(reader.Contents());

		iohook.def_creds = new x509_cred(Conf, ca_list, crl_list, dh);

		iohook.creds.clear();

		ConfigTagList tags = ServerInstance->Config->GetTags("ssl_cert");
		while (tags.first != tags.second)
		{
			ConfigTag* tag = tags.first->second;
			std::string name = tag->getString("name");
			if (name.empty())
				throw ModuleException("Invalid <ssl_cert> without name at " + tag->getTagLocation());

			iohook.creds[name] = new x509_cred(tag, ca_list, crl_list, dh);

			tags.first++;
		}
	}

	void OnGarbageCollect()
	{
 		// Generate Diffie Hellman parameters - for use with DHE
		// kx algorithms. These should be discarded and regenerated
		// once a day, once a week or once a month. Depending on the
		// security requirements.
		if (once_in_a_while++ & 0xFF)
			return;

		dh = new DH_info(dh->bits);
		reference<x509_cred> cred = iohook.def_creds;
		iohook.def_creds = new x509_cred(cred, dh);
		for(std::map<std::string, reference<x509_cred> >::iterator i = iohook.creds.begin(); i != iohook.creds.end(); i++)
		{
			cred = i->second;
			i->second = new x509_cred(cred, dh);
		}
	}

	~ModuleSSLGnuTLS()
	{
		iohook.creds.clear();
		iohook.def_creds = NULL;
		gnutls_global_deinit();
		ServerInstance->GenRandom = &ServerInstance->HandleGenRandom;
	}

	void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			LocalUser* user = IS_LOCAL(static_cast<User*>(item));

			if (user && user->eh.GetIOHook() && user->eh.GetIOHook()->creator == this)
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


	void On005Numeric(std::string &output)
	{
		if (!sslports.empty())
			output.append(" SSL=" + sslports);
		if (starttls.enabled)
			output.append(" STARTTLS");
	}

	void OnUserConnect(LocalUser* user)
	{
		GnuTLSHook* hook = static_cast<GnuTLSHook*>(user->eh.GetIOHook());
		if (hook && hook->creator == this)
		{
			ssl_cert* cert = hook->cert;
			std::string cipher = gnutls_kx_get_name(gnutls_kx_get(hook->sess));
			cipher.append("-").append(gnutls_cipher_get_name(gnutls_cipher_get(hook->sess))).append("-");
			cipher.append(gnutls_mac_get_name(gnutls_mac_get(hook->sess)));
			if (cert->fingerprint.empty())
				user->WriteServ("NOTICE %s :*** You are connected using SSL cipher \"%s\"", user->nick.c_str(), cipher.c_str());
			else
				user->WriteServ("NOTICE %s :*** You are connected using SSL cipher \"%s\""
					" and your SSL fingerprint is %s", user->nick.c_str(), cipher.c_str(), cert->fingerprint.c_str());
		}
	}

	void OnEvent(Event& ev)
	{
		if (starttls.enabled)
			capHandler.HandleEvent(ev);
	}
};

}

using GnuTLS::ModuleSSLGnuTLS;

MODULE_INIT(ModuleSSLGnuTLS)
