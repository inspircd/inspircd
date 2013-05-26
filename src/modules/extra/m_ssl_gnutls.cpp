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
#include <gcrypt.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include "modules/ssl.h"
#include "modules/cap.h"

#ifdef _WIN32
# pragma comment(lib, "libgnutls.lib")
# pragma comment(lib, "libgcrypt.lib")
# pragma comment(lib, "libgpg-error.lib")
# pragma comment(lib, "user32.lib")
# pragma comment(lib, "advapi32.lib")
# pragma comment(lib, "libgcc.lib")
# pragma comment(lib, "libmingwex.lib")
# pragma comment(lib, "gdi32.lib")
#endif

/* $ModDesc: Provides SSL support for clients */
/* $CompileFlags: pkgconfincludes("gnutls","/gnutls/gnutls.h","") exec("libgcrypt-config --cflags") */
/* $LinkerFlags: rpath("pkg-config --libs gnutls") pkgconflibs("gnutls","/libgnutls.so","-lgnutls") exec("libgcrypt-config --libs") */
/* $NoPedantic */

// These don't exist in older GnuTLS versions
#if ((GNUTLS_VERSION_MAJOR > 2) || (GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR > 1) || (GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR == 1 && GNUTLS_VERSION_MICRO >= 7))
#define GNUTLS_NEW_PRIO_API
#endif

#if(GNUTLS_VERSION_MAJOR < 2)
typedef gnutls_certificate_credentials_t gnutls_certificate_credentials;
typedef gnutls_dh_params_t gnutls_dh_params;
#endif

enum issl_status { ISSL_NONE, ISSL_HANDSHAKING_READ, ISSL_HANDSHAKING_WRITE, ISSL_HANDSHAKEN, ISSL_CLOSING, ISSL_CLOSED };

static std::vector<gnutls_x509_crt_t> x509_certs;
static gnutls_x509_privkey_t x509_key;
#if(GNUTLS_VERSION_MAJOR < 2 || ( GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR < 12 ) )
static int cert_callback (gnutls_session_t session, const gnutls_datum_t * req_ca_rdn, int nreqs,
	const gnutls_pk_algorithm_t * sign_algos, int sign_algos_length, gnutls_retr_st * st) {

	st->type = GNUTLS_CRT_X509;
#else
static int cert_callback (gnutls_session_t session, const gnutls_datum_t * req_ca_rdn, int nreqs,
	const gnutls_pk_algorithm_t * sign_algos, int sign_algos_length, gnutls_retr2_st * st) {
	st->cert_type = GNUTLS_CRT_X509;
	st->key_type = GNUTLS_PRIVKEY_X509;
#endif
	st->ncerts = x509_certs.size();
	st->cert.x509 = &x509_certs[0];
	st->key.x509 = x509_key;
	st->deinit_all = 0;

	return 0;
}

static ssize_t gnutls_pull_wrapper(gnutls_transport_ptr_t user_wrap, void* buffer, size_t size)
{
	StreamSocket* user = reinterpret_cast<StreamSocket*>(user_wrap);
	if (user->GetEventMask() & FD_READ_WILL_BLOCK)
	{
		errno = EAGAIN;
		return -1;
	}
	int rv = ServerInstance->SE->Recv(user, reinterpret_cast<char *>(buffer), size, 0);
	if (rv < 0)
	{
		/* On Windows we need to set errno for gnutls */
		if (SocketEngine::IgnoreError())
			errno = EAGAIN;
	}
	if (rv < (int)size)
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
	int rv = ServerInstance->SE->Send(user, reinterpret_cast<const char *>(buffer), size, 0);
	if (rv < 0)
	{
		/* On Windows we need to set errno for gnutls */
		if (SocketEngine::IgnoreError())
			errno = EAGAIN;
	}
	if (rv < (int)size)
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

/** Represents an SSL user's extra data
 */
class issl_session
{
public:
	gnutls_session_t sess;
	issl_status status;
	reference<ssl_cert> cert;
	issl_session() : sess(NULL) {}
};

class CommandStartTLS : public SplitCommand
{
 public:
	bool enabled;
	CommandStartTLS (Module* mod) : SplitCommand(mod, "STARTTLS")
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
				user->eh.AddIOHook(creator);
				creator->OnStreamSocketAccept(&user->eh, NULL, NULL);
			}
			else
				user->WriteNumeric(691, "%s :STARTTLS failure", user->nick.c_str());
		}

		return CMD_FAILURE;
	}
};

class ModuleSSLGnuTLS : public Module
{
	issl_session* sessions;

	gnutls_certificate_credentials_t x509_cred;
	gnutls_dh_params_t dh_params;
	gnutls_digest_algorithm_t hash;
	#ifdef GNUTLS_NEW_PRIO_API
	gnutls_priority_t priority;
	#endif

	std::string sslports;
	int dh_bits;

	bool cred_alloc;
	bool dh_alloc;

	RandGen randhandler;
	CommandStartTLS starttls;

	GenericCap capHandler;
	ServiceProvider iohook;

	inline static const char* UnknownIfNULL(const char* str)
	{
		return str ? str : "UNKNOWN";
	}

 public:
	ModuleSSLGnuTLS()
		: starttls(this), capHandler(this, "tls"), iohook(this, "ssl/gnutls", SERVICE_IOHOOK)
	{
		gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);

		sessions = new issl_session[ServerInstance->SE->GetMaxFds()];

		gnutls_global_init(); // This must be called once in the program
		gnutls_x509_privkey_init(&x509_key);

		#ifdef GNUTLS_NEW_PRIO_API
		// Init this here so it's always initialized, avoids an extra boolean
		gnutls_priority_init(&priority, "NORMAL", NULL);
		#endif

		cred_alloc = false;
		dh_alloc = false;
	}

	void init() CXX11_OVERRIDE
	{
		// Needs the flag as it ignores a plain /rehash
		OnModuleRehash(NULL,"ssl");

		ServerInstance->GenRandom = &randhandler;

		// Void return, guess we assume success
		gnutls_certificate_set_dh_params(x509_cred, dh_params);
		Implementation eventlist[] = { I_On005Numeric, I_OnRehash, I_OnModuleRehash, I_OnUserConnect,
			I_OnEvent, I_OnHookIO };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		ServerInstance->Modules->AddService(iohook);
		ServerInstance->Modules->AddService(starttls);
	}

	void OnRehash(User* user) CXX11_OVERRIDE
	{
		sslports.clear();

		ConfigTag* Conf = ServerInstance->Config->ConfValue("gnutls");
		starttls.enabled = Conf->getBool("starttls", true);

		if (Conf->getBool("showports", true))
		{
			sslports = Conf->getString("advertisedports");
			if (!sslports.empty())
				return;

			for (size_t i = 0; i < ServerInstance->ports.size(); i++)
			{
				ListenSocket* port = ServerInstance->ports[i];
				if (port->bind_tag->getString("ssl") != "gnutls")
					continue;

				const std::string& portid = port->bind_desc;
				ServerInstance->Logs->Log("m_ssl_gnutls", LOG_DEFAULT, "m_ssl_gnutls.so: Enabling SSL for port %s", portid.c_str());

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

	void OnModuleRehash(User* user, const std::string &param) CXX11_OVERRIDE
	{
		if(param != "ssl")
			return;

		std::string keyfile;
		std::string certfile;
		std::string cafile;
		std::string crlfile;
		OnRehash(user);

		ConfigTag* Conf = ServerInstance->Config->ConfValue("gnutls");

		cafile = Conf->getString("cafile", CONFIG_PATH "/ca.pem");
		crlfile	= Conf->getString("crlfile", CONFIG_PATH "/crl.pem");
		certfile = Conf->getString("certfile", CONFIG_PATH "/cert.pem");
		keyfile	= Conf->getString("keyfile", CONFIG_PATH "/key.pem");
		dh_bits	= Conf->getInt("dhbits");
		std::string hashname = Conf->getString("hash", "md5");

		// The GnuTLS manual states that the gnutls_set_default_priority()
		// call we used previously when initializing the session is the same
		// as setting the "NORMAL" priority string.
		// Thus if the setting below is not in the config we will behave exactly
		// the same as before, when the priority setting wasn't available.
		std::string priorities = Conf->getString("priority", "NORMAL");

		if((dh_bits != 768) && (dh_bits != 1024) && (dh_bits != 2048) && (dh_bits != 3072) && (dh_bits != 4096))
			dh_bits = 1024;

		if (hashname == "md5")
			hash = GNUTLS_DIG_MD5;
		else if (hashname == "sha1")
			hash = GNUTLS_DIG_SHA1;
		else
			throw ModuleException("Unknown hash type " + hashname);


		int ret;

		if (dh_alloc)
		{
			gnutls_dh_params_deinit(dh_params);
			dh_alloc = false;
			dh_params = NULL;
		}

		if (cred_alloc)
		{
			// Deallocate the old credentials
			gnutls_certificate_free_credentials(x509_cred);

			for(unsigned int i=0; i < x509_certs.size(); i++)
				gnutls_x509_crt_deinit(x509_certs[i]);
			x509_certs.clear();
		}

		ret = gnutls_certificate_allocate_credentials(&x509_cred);
		cred_alloc = (ret >= 0);
		if (!cred_alloc)
			ServerInstance->Logs->Log("m_ssl_gnutls", LOG_DEBUG, "m_ssl_gnutls.so: Failed to allocate certificate credentials: %s", gnutls_strerror(ret));

		if((ret =gnutls_certificate_set_x509_trust_file(x509_cred, cafile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
			ServerInstance->Logs->Log("m_ssl_gnutls", LOG_DEBUG, "m_ssl_gnutls.so: Failed to set X.509 trust file '%s': %s", cafile.c_str(), gnutls_strerror(ret));

		if((ret = gnutls_certificate_set_x509_crl_file (x509_cred, crlfile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
			ServerInstance->Logs->Log("m_ssl_gnutls", LOG_DEBUG, "m_ssl_gnutls.so: Failed to set X.509 CRL file '%s': %s", crlfile.c_str(), gnutls_strerror(ret));

		FileReader reader;

		reader.Load(certfile);
		std::string cert_string = reader.GetString();
		gnutls_datum_t cert_datum = { (unsigned char*)cert_string.data(), static_cast<unsigned int>(cert_string.length()) };

		reader.Load(keyfile);
		std::string key_string = reader.GetString();
		gnutls_datum_t key_datum = { (unsigned char*)key_string.data(), static_cast<unsigned int>(key_string.length()) };

		// If this fails, no SSL port will work. At all. So, do the smart thing - throw a ModuleException
		unsigned int certcount = 3;
		x509_certs.resize(certcount);
		ret = gnutls_x509_crt_list_import(&x509_certs[0], &certcount, &cert_datum, GNUTLS_X509_FMT_PEM, GNUTLS_X509_CRT_LIST_IMPORT_FAIL_IF_EXCEED);
		if (ret == GNUTLS_E_SHORT_MEMORY_BUFFER)
		{
			// the buffer wasn't big enough to hold all certs but gnutls updated certcount to the number of available certs, try again with a bigger buffer
			x509_certs.resize(certcount);
			ret = gnutls_x509_crt_list_import(&x509_certs[0], &certcount, &cert_datum, GNUTLS_X509_FMT_PEM, GNUTLS_X509_CRT_LIST_IMPORT_FAIL_IF_EXCEED);
		}

		if (ret <= 0)
		{
			// clear the vector so we won't call gnutls_x509_crt_deinit() on the (uninited) certs later
			x509_certs.clear();
			throw ModuleException("Unable to load GnuTLS server certificate (" + certfile + "): " + ((ret < 0) ? (std::string(gnutls_strerror(ret))) : "No certs could be read"));
		}
		x509_certs.resize(ret);

		if((ret = gnutls_x509_privkey_import(x509_key, &key_datum, GNUTLS_X509_FMT_PEM)) < 0)
			throw ModuleException("Unable to load GnuTLS server private key (" + keyfile + "): " + std::string(gnutls_strerror(ret)));

		if((ret = gnutls_certificate_set_x509_key(x509_cred, &x509_certs[0], certcount, x509_key)) < 0)
			throw ModuleException("Unable to set GnuTLS cert/key pair: " + std::string(gnutls_strerror(ret)));

		#ifdef GNUTLS_NEW_PRIO_API
		// It's safe to call this every time as we cannot have this uninitialized, see constructor and below.
		gnutls_priority_deinit(priority);

		// Try to set the priorities for ciphers, kex methods etc. to the user supplied string
		// If the user did not supply anything then the string is already set to "NORMAL"
		const char* priocstr = priorities.c_str();
		const char* prioerror;

		if ((ret = gnutls_priority_init(&priority, priocstr, &prioerror)) < 0)
		{
			// gnutls did not understand the user supplied string, log and fall back to the default priorities
			ServerInstance->Logs->Log("m_ssl_gnutls", LOG_DEFAULT, "m_ssl_gnutls.so: Failed to set priorities to \"%s\": %s Syntax error at position %u, falling back to default (NORMAL)", priorities.c_str(), gnutls_strerror(ret), (unsigned int) (prioerror - priocstr));
			gnutls_priority_init(&priority, "NORMAL", NULL);
		}

		#else
		if (priorities != "NORMAL")
			ServerInstance->Logs->Log("m_ssl_gnutls", LOG_DEFAULT, "m_ssl_gnutls.so: You've set <gnutls:priority> to a value other than the default, but this is only supported with GnuTLS v2.1.7 or newer. Your GnuTLS version is older than that so the option will have no effect.");
		#endif

		#if(GNUTLS_VERSION_MAJOR < 2 || ( GNUTLS_VERSION_MAJOR == 2 && GNUTLS_VERSION_MINOR < 12 ) )
		gnutls_certificate_client_set_retrieve_function (x509_cred, cert_callback);
		#else
		gnutls_certificate_set_retrieve_function (x509_cred, cert_callback);
		#endif
		ret = gnutls_dh_params_init(&dh_params);
		dh_alloc = (ret >= 0);
		if (!dh_alloc)
		{
			ServerInstance->Logs->Log("m_ssl_gnutls", LOG_DEFAULT, "m_ssl_gnutls.so: Failed to initialise DH parameters: %s", gnutls_strerror(ret));
			return;
		}

		std::string dhfile = Conf->getString("dhfile");
		if (!dhfile.empty())
		{
			// Try to load DH params from file
			reader.Load(dhfile);
			std::string dhstring = reader.GetString();
			gnutls_datum_t dh_datum = { (unsigned char*)dhstring.data(), static_cast<unsigned int>(dhstring.length()) };

			if ((ret = gnutls_dh_params_import_pkcs3(dh_params, &dh_datum, GNUTLS_X509_FMT_PEM)) < 0)
			{
				// File unreadable or GnuTLS was unhappy with the contents, generate the DH primes now
				ServerInstance->Logs->Log("m_ssl_gnutls", LOG_DEFAULT, "m_ssl_gnutls.so: Generating DH parameters because I failed to load them from file '%s': %s", dhfile.c_str(), gnutls_strerror(ret));
				GenerateDHParams();
			}
		}
		else
		{
			GenerateDHParams();
		}
	}

	void GenerateDHParams()
	{
 		// Generate Diffie Hellman parameters - for use with DHE
		// kx algorithms. These should be discarded and regenerated
		// once a day, once a week or once a month. Depending on the
		// security requirements.

		if (!dh_alloc)
			return;

		int ret;

		if((ret = gnutls_dh_params_generate2(dh_params, dh_bits)) < 0)
			ServerInstance->Logs->Log("m_ssl_gnutls", LOG_DEFAULT, "m_ssl_gnutls.so: Failed to generate DH parameters (%d bits): %s", dh_bits, gnutls_strerror(ret));
	}

	~ModuleSSLGnuTLS()
	{
		for(unsigned int i=0; i < x509_certs.size(); i++)
			gnutls_x509_crt_deinit(x509_certs[i]);

		gnutls_x509_privkey_deinit(x509_key);
		#ifdef GNUTLS_NEW_PRIO_API
		gnutls_priority_deinit(priority);
		#endif

		if (dh_alloc)
			gnutls_dh_params_deinit(dh_params);
		if (cred_alloc)
			gnutls_certificate_free_credentials(x509_cred);

		gnutls_global_deinit();
		delete[] sessions;
		ServerInstance->GenRandom = &ServerInstance->HandleGenRandom;
	}

	void OnCleanup(int target_type, void* item) CXX11_OVERRIDE
	{
		if(target_type == TYPE_USER)
		{
			LocalUser* user = IS_LOCAL(static_cast<User*>(item));

			if (user && user->eh.GetIOHook() == this)
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

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		if (!sslports.empty())
			tokens["SSL"] = sslports;
		if (starttls.enabled)
			tokens["STARTTLS"];
	}

	void OnHookIO(StreamSocket* user, ListenSocket* lsb) CXX11_OVERRIDE
	{
		if (!user->GetIOHook() && lsb->bind_tag->getString("ssl") == "gnutls")
		{
			/* Hook the user with our module */
			user->AddIOHook(this);
		}
	}

	void OnRequest(Request& request) CXX11_OVERRIDE
	{
		if (strcmp("GET_SSL_CERT", request.id) == 0)
		{
			SocketCertificateRequest& req = static_cast<SocketCertificateRequest&>(request);
			int fd = req.sock->GetFd();
			issl_session* session = &sessions[fd];

			req.cert = session->cert;
		}
	}

	void InitSession(StreamSocket* user, bool me_server)
	{
		issl_session* session = &sessions[user->GetFd()];

		gnutls_init(&session->sess, me_server ? GNUTLS_SERVER : GNUTLS_CLIENT);

		#ifdef GNUTLS_NEW_PRIO_API
		gnutls_priority_set(session->sess, priority);
		#endif
		gnutls_credentials_set(session->sess, GNUTLS_CRD_CERTIFICATE, x509_cred);
		gnutls_dh_set_prime_bits(session->sess, dh_bits);
		gnutls_transport_set_ptr(session->sess, reinterpret_cast<gnutls_transport_ptr_t>(user));
		gnutls_transport_set_push_function(session->sess, gnutls_push_wrapper);
		gnutls_transport_set_pull_function(session->sess, gnutls_pull_wrapper);

		if (me_server)
			gnutls_certificate_server_set_request(session->sess, GNUTLS_CERT_REQUEST); // Request client certificate if any.

		Handshake(session, user);
	}

	void OnStreamSocketAccept(StreamSocket* user, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		issl_session* session = &sessions[user->GetFd()];

		/* For STARTTLS: Don't try and init a session on a socket that already has a session */
		if (session->sess)
			return;

		InitSession(user, true);
	}

	void OnStreamSocketConnect(StreamSocket* user) CXX11_OVERRIDE
	{
		InitSession(user, false);
	}

	void OnStreamSocketClose(StreamSocket* user) CXX11_OVERRIDE
	{
		CloseSession(&sessions[user->GetFd()]);
	}

	int OnStreamSocketRead(StreamSocket* user, std::string& recvq) CXX11_OVERRIDE
	{
		issl_session* session = &sessions[user->GetFd()];

		if (!session->sess)
		{
			CloseSession(session);
			user->SetError("No SSL session");
			return -1;
		}

		if (session->status == ISSL_HANDSHAKING_READ || session->status == ISSL_HANDSHAKING_WRITE)
		{
			// The handshake isn't finished, try to finish it.

			if(!Handshake(session, user))
			{
				if (session->status != ISSL_CLOSING)
					return 0;
				return -1;
			}
		}

		// If we resumed the handshake then session->status will be ISSL_HANDSHAKEN.

		if (session->status == ISSL_HANDSHAKEN)
		{
			char* buffer = ServerInstance->GetReadBuffer();
			size_t bufsiz = ServerInstance->Config->NetBufferSize;
			int ret = gnutls_record_recv(session->sess, buffer, bufsiz);
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
				CloseSession(session);
				return -1;
			}
			else
			{
				user->SetError(gnutls_strerror(ret));
				CloseSession(session);
				return -1;
			}
		}
		else if (session->status == ISSL_CLOSING)
			return -1;

		return 0;
	}

	int OnStreamSocketWrite(StreamSocket* user, std::string& sendq) CXX11_OVERRIDE
	{
		issl_session* session = &sessions[user->GetFd()];

		if (!session->sess)
		{
			CloseSession(session);
			user->SetError("No SSL session");
			return -1;
		}

		if (session->status == ISSL_HANDSHAKING_WRITE || session->status == ISSL_HANDSHAKING_READ)
		{
			// The handshake isn't finished, try to finish it.
			Handshake(session, user);
			if (session->status != ISSL_CLOSING)
				return 0;
			return -1;
		}

		int ret = 0;

		if (session->status == ISSL_HANDSHAKEN)
		{
			ret = gnutls_record_send(session->sess, sendq.data(), sendq.length());

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
				CloseSession(session);
				return -1;
			}
		}

		return 0;
	}

	bool Handshake(issl_session* session, StreamSocket* user)
	{
		int ret = gnutls_handshake(session->sess);

		if (ret < 0)
		{
			if(ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
			{
				// Handshake needs resuming later, read() or write() would have blocked.

				if(gnutls_record_get_direction(session->sess) == 0)
				{
					// gnutls_handshake() wants to read() again.
					session->status = ISSL_HANDSHAKING_READ;
					ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				}
				else
				{
					// gnutls_handshake() wants to write() again.
					session->status = ISSL_HANDSHAKING_WRITE;
					ServerInstance->SE->ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
				}
			}
			else
			{
				user->SetError("Handshake Failed - " + std::string(gnutls_strerror(ret)));
				CloseSession(session);
				session->status = ISSL_CLOSING;
			}

			return false;
		}
		else
		{
			// Change the seesion state
			session->status = ISSL_HANDSHAKEN;

			VerifyCertificate(session,user);

			// Finish writing, if any left
			ServerInstance->SE->ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

			return true;
		}
	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		if (user->eh.GetIOHook() == this)
		{
			if (sessions[user->eh.GetFd()].sess)
			{
				const gnutls_session_t& sess = sessions[user->eh.GetFd()].sess;
				std::string cipher = UnknownIfNULL(gnutls_kx_get_name(gnutls_kx_get(sess)));
				cipher.append("-").append(UnknownIfNULL(gnutls_cipher_get_name(gnutls_cipher_get(sess)))).append("-");
				cipher.append(UnknownIfNULL(gnutls_mac_get_name(gnutls_mac_get(sess))));

				ssl_cert* cert = sessions[user->eh.GetFd()].cert;
				if (cert->fingerprint.empty())
					user->WriteNotice("*** You are connected using SSL cipher '" + cipher + "'");
				else
					user->WriteNotice("*** You are connected using SSL cipher '" + cipher +
						"' and your SSL fingerprint is " + cert->fingerprint);
			}
		}
	}

	void CloseSession(issl_session* session)
	{
		if (session->sess)
		{
			gnutls_bye(session->sess, GNUTLS_SHUT_WR);
			gnutls_deinit(session->sess);
		}
		session->sess = NULL;
		session->cert = NULL;
		session->status = ISSL_NONE;
	}

	void VerifyCertificate(issl_session* session, StreamSocket* user)
	{
		if (!session->sess || !user)
			return;

		unsigned int status;
		const gnutls_datum_t* cert_list;
		int ret;
		unsigned int cert_list_size;
		gnutls_x509_crt_t cert;
		char name[MAXBUF];
		unsigned char digest[MAXBUF];
		size_t digest_size = sizeof(digest);
		size_t name_size = sizeof(name);
		ssl_cert* certinfo = new ssl_cert;
		session->cert = certinfo;

		/* This verification function uses the trusted CAs in the credentials
		 * structure. So you must have installed one or more CA certificates.
		 */
		ret = gnutls_certificate_verify_peers2(session->sess, &status);

		if (ret < 0)
		{
			certinfo->error = std::string(gnutls_strerror(ret));
			return;
		}

		certinfo->invalid = (status & GNUTLS_CERT_INVALID);
		certinfo->unknownsigner = (status & GNUTLS_CERT_SIGNER_NOT_FOUND);
		certinfo->revoked = (status & GNUTLS_CERT_REVOKED);
		certinfo->trusted = !(status & GNUTLS_CERT_SIGNER_NOT_CA);

		/* Up to here the process is the same for X.509 certificates and
		 * OpenPGP keys. From now on X.509 certificates are assumed. This can
		 * be easily extended to work with openpgp keys as well.
		 */
		if (gnutls_certificate_type_get(session->sess) != GNUTLS_CRT_X509)
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
		cert_list = gnutls_certificate_get_peers(session->sess, &cert_list_size);
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

		gnutls_x509_crt_get_dn(cert, name, &name_size);
		certinfo->dn = name;

		gnutls_x509_crt_get_issuer_dn(cert, name, &name_size);
		certinfo->issuer = name;

		if ((ret = gnutls_x509_crt_get_fingerprint(cert, hash, digest, &digest_size)) < 0)
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

	void OnEvent(Event& ev) CXX11_OVERRIDE
	{
		if (starttls.enabled)
			capHandler.HandleEvent(ev);
	}
};

MODULE_INIT(ModuleSSLGnuTLS)
