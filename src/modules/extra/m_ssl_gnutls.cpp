/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
#include "transport.h"
#include "m_cap.h"

#ifdef WINDOWS
#pragma comment(lib, "libgnutls-13.lib")
#endif

/* $ModDesc: Provides SSL support for clients */
/* $CompileFlags: pkgconfincludes("gnutls","/gnutls/gnutls.h","") */
/* $LinkerFlags: rpath("pkg-config --libs gnutls") pkgconflibs("gnutls","/libgnutls.so","-lgnutls") */
/* $ModDep: transport.h */
/* $CopyInstall: conf/key.pem $(CONPATH) */
/* $CopyInstall: conf/cert.pem $(CONPATH) */

enum issl_status { ISSL_NONE, ISSL_HANDSHAKING_READ, ISSL_HANDSHAKING_WRITE, ISSL_HANDSHAKEN, ISSL_CLOSING, ISSL_CLOSED };

bool isin(const std::string &host, int port, const std::vector<std::string> &portlist)
{
	if (std::find(portlist.begin(), portlist.end(), "*:" + ConvToStr(port)) != portlist.end())
		return true;

	if (std::find(portlist.begin(), portlist.end(), ":" + ConvToStr(port)) != portlist.end())
		return true;

	return std::find(portlist.begin(), portlist.end(), host + ":" + ConvToStr(port)) != portlist.end();
}

/** Represents an SSL user's extra data
 */
class issl_session : public classbase
{
public:
	issl_session()
	{
		sess = NULL;
	}

	gnutls_session_t sess;
	issl_status status;
	std::string outbuf;
};

class CommandStartTLS : public Command
{
	Module* Caller;
 public:
	CommandStartTLS (InspIRCd* Instance, Module* mod) : Command(Instance,"STARTTLS", 0, 0, true), Caller(mod)
	{
		this->source = "m_ssl_gnutls.so";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		/* changed from == REG_ALL to catch clients sending STARTTLS
		 * after NICK and USER but before OnUserConnect completes and
		 * give a proper error message (see bug #645) - dz
		 */
		if (user->registered != REG_NONE)
		{
			user->WriteNumeric(691, "%s :STARTTLS is not permitted after client registration has started", user->nick.c_str());
		}
		else
		{
			if (!user->GetIOHook())
			{
				user->WriteNumeric(670, "%s :STARTTLS successful, go ahead with TLS handshake", user->nick.c_str());
				user->AddIOHook(Caller);
				Caller->OnRawSocketAccept(user->GetFd(), NULL, NULL);
			}
			else
				user->WriteNumeric(691, "%s :STARTTLS failure", user->nick.c_str());
		}

		return CMD_FAILURE;
	}
};

class ModuleSSLGnuTLS : public Module
{
	std::vector<std::string> listenports;

	issl_session* sessions;

	gnutls_certificate_credentials x509_cred;
	gnutls_dh_params dh_params;

	std::string keyfile;
	std::string certfile;
	std::string cafile;
	std::string crlfile;
	std::string sslports;
	int dh_bits;

	int clientactive;
	bool cred_alloc;

	CommandStartTLS* starttls;

 public:

	ModuleSSLGnuTLS(InspIRCd* Me)
		: Module(Me)
	{
		ServerInstance->Modules->PublishInterface("BufferedSocketHook", this);

		sessions = new issl_session[ServerInstance->SE->GetMaxFds()];

		gnutls_global_init(); // This must be called once in the program

		cred_alloc = false;
		// Needs the flag as it ignores a plain /rehash
		OnModuleRehash(NULL,"ssl");

		// Void return, guess we assume success
		gnutls_certificate_set_dh_params(x509_cred, dh_params);
		Implementation eventlist[] = { I_On005Numeric, I_OnRawSocketConnect, I_OnRawSocketAccept,
			I_OnRawSocketClose, I_OnRawSocketRead, I_OnRawSocketWrite, I_OnCleanup,
			I_OnBufferFlushed, I_OnRequest, I_OnUnloadModule, I_OnRehash, I_OnModuleRehash,
			I_OnPostConnect, I_OnEvent, I_OnHookUserIO };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		starttls = new CommandStartTLS(ServerInstance, this);
		ServerInstance->AddCommand(starttls);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf(ServerInstance);

		listenports.clear();
		clientactive = 0;
		sslports.clear();

		for(int index = 0; index < Conf.Enumerate("bind"); index++)
		{
			// For each <bind> tag
			std::string x = Conf.ReadValue("bind", "type", index);
			if(((x.empty()) || (x == "clients")) && (Conf.ReadValue("bind", "ssl", index) == "gnutls"))
			{
				// Get the port we're meant to be listening on with SSL
				std::string port = Conf.ReadValue("bind", "port", index);
				std::string addr = Conf.ReadValue("bind", "address", index);

				if (!addr.empty())
				{
					// normalize address, important for IPv6
					int portint = 0;
					irc::sockets::sockaddrs bin;
					if (irc::sockets::aptosa(addr.c_str(), portint, &bin))
						irc::sockets::satoap(&bin, addr, portint);
				}

				irc::portparser portrange(port, false);
				long portno = -1;
				while ((portno = portrange.GetToken()))
				{
					clientactive++;
					try
					{
						listenports.push_back(addr + ":" + ConvToStr(portno));

						for (size_t i = 0; i < ServerInstance->ports.size(); i++)
							if ((ServerInstance->ports[i]->GetPort() == portno) && (ServerInstance->ports[i]->GetIP() == addr))
								ServerInstance->ports[i]->SetDescription("ssl");
						ServerInstance->Logs->Log("m_ssl_gnutls",DEFAULT, "m_ssl_gnutls.so: Enabling SSL for port %ld", portno);

						if (addr != "127.0.0.1")
							sslports.append((addr.empty() ? "*" : addr)).append(":").append(ConvToStr(portno)).append(";");
					}
					catch (ModuleException &e)
					{
						ServerInstance->Logs->Log("m_ssl_gnutls",DEFAULT, "m_ssl_gnutls.so: FAILED to enable SSL on port %ld: %s. Maybe it's already hooked by the same port on a different IP, or you have an other SSL or similar module loaded?", portno, e.GetReason());
					}
				}
			}
		}

		if (!sslports.empty())
			sslports.erase(sslports.end() - 1);
	}

	virtual void OnModuleRehash(User* user, const std::string &param)
	{
		if(param != "ssl")
			return;

		OnRehash(user);

		ConfigReader Conf(ServerInstance);

		std::string confdir(ServerInstance->ConfigFileName);
		// +1 so we the path ends with a /
		confdir = confdir.substr(0, confdir.find_last_of('/') + 1);

		cafile = Conf.ReadValue("gnutls", "cafile", 0);
		crlfile	= Conf.ReadValue("gnutls", "crlfile", 0);
		certfile = Conf.ReadValue("gnutls", "certfile", 0);
		keyfile	= Conf.ReadValue("gnutls", "keyfile", 0);
		dh_bits	= Conf.ReadInteger("gnutls", "dhbits", 0, false);

		// Set all the default values needed.
		if (cafile.empty())
			cafile = "ca.pem";

		if (crlfile.empty())
			crlfile = "crl.pem";

		if (certfile.empty())
			certfile = "cert.pem";

		if (keyfile.empty())
			keyfile = "key.pem";

		if((dh_bits != 768) && (dh_bits != 1024) && (dh_bits != 2048) && (dh_bits != 3072) && (dh_bits != 4096))
			dh_bits = 1024;

		// Prepend relative paths with the path to the config directory.
		if ((cafile[0] != '/') && (!ServerInstance->Config->StartsWithWindowsDriveLetter(cafile)))
			cafile = confdir + cafile;

		if ((crlfile[0] != '/') && (!ServerInstance->Config->StartsWithWindowsDriveLetter(crlfile)))
			crlfile = confdir + crlfile;

		if ((certfile[0] != '/') && (!ServerInstance->Config->StartsWithWindowsDriveLetter(certfile)))
			certfile = confdir + certfile;

		if ((keyfile[0] != '/') && (!ServerInstance->Config->StartsWithWindowsDriveLetter(keyfile)))
			keyfile = confdir + keyfile;

		int ret;

		if (cred_alloc)
		{
			// Deallocate the old credentials
			gnutls_dh_params_deinit(dh_params);
			gnutls_certificate_free_credentials(x509_cred);
		}
		else
			cred_alloc = true;

		if((ret = gnutls_certificate_allocate_credentials(&x509_cred)) < 0)
			ServerInstance->Logs->Log("m_ssl_gnutls",DEBUG, "m_ssl_gnutls.so: Failed to allocate certificate credentials: %s", gnutls_strerror(ret));

		if((ret = gnutls_dh_params_init(&dh_params)) < 0)
			ServerInstance->Logs->Log("m_ssl_gnutls",DEBUG, "m_ssl_gnutls.so: Failed to initialise DH parameters: %s", gnutls_strerror(ret));

		if((ret =gnutls_certificate_set_x509_trust_file(x509_cred, cafile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
			ServerInstance->Logs->Log("m_ssl_gnutls",DEBUG, "m_ssl_gnutls.so: Failed to set X.509 trust file '%s': %s", cafile.c_str(), gnutls_strerror(ret));

		if((ret = gnutls_certificate_set_x509_crl_file (x509_cred, crlfile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
			ServerInstance->Logs->Log("m_ssl_gnutls",DEBUG, "m_ssl_gnutls.so: Failed to set X.509 CRL file '%s': %s", crlfile.c_str(), gnutls_strerror(ret));

		if((ret = gnutls_certificate_set_x509_key_file (x509_cred, certfile.c_str(), keyfile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
		{
			// If this fails, no SSL port will work. At all. So, do the smart thing - throw a ModuleException
			throw ModuleException("Unable to load GnuTLS server certificate (" + certfile + ", key: " + keyfile + "): " + std::string(gnutls_strerror(ret)));
		}

		// This may be on a large (once a day or week) timer eventually.
		GenerateDHParams();
	}

	void GenerateDHParams()
	{
 		// Generate Diffie Hellman parameters - for use with DHE
		// kx algorithms. These should be discarded and regenerated
		// once a day, once a week or once a month. Depending on the
		// security requirements.

		int ret;

		if((ret = gnutls_dh_params_generate2(dh_params, dh_bits)) < 0)
			ServerInstance->Logs->Log("m_ssl_gnutls",DEFAULT, "m_ssl_gnutls.so: Failed to generate DH parameters (%d bits): %s", dh_bits, gnutls_strerror(ret));
	}

	virtual ~ModuleSSLGnuTLS()
	{
		gnutls_dh_params_deinit(dh_params);
		gnutls_certificate_free_credentials(x509_cred);
		gnutls_global_deinit();
		ServerInstance->Modules->UnpublishInterface("BufferedSocketHook", this);
		delete[] sessions;
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			User* user = (User*)item;

			if (user->GetIOHook() == this)
			{
				// User is using SSL, they're a local user, and they're using one of *our* SSL ports.
				// Potentially there could be multiple SSL modules loaded at once on different ports.
				ServerInstance->Users->QuitUser(user, "SSL module unloading");
				user->DelIOHook();
			}
			if (user->GetExt("ssl_cert"))
			{
				ssl_cert* tofree;
				user->GetExt("ssl_cert", tofree);
				delete tofree;
				user->Shrink("ssl_cert");
			}
		}
	}

	virtual void OnUnloadModule(Module* mod, const std::string &name)
	{
		if(mod == this)
		{
			for(unsigned int i = 0; i < listenports.size(); i++)
			{
				for (size_t j = 0; j < ServerInstance->ports.size(); j++)
					if (listenports[i] == (ServerInstance->ports[j]->GetIP()+":"+ConvToStr(ServerInstance->ports[j]->GetPort())))
						ServerInstance->ports[j]->SetDescription("plaintext");
			}
		}
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}


	virtual void On005Numeric(std::string &output)
	{
		if (!sslports.empty())
			output.append(" SSL=" + sslports);
		output.append(" STARTTLS");
	}

	virtual void OnHookUserIO(User* user)
	{
		if (!user->GetIOHook() && isin(user->GetServerIP(),user->GetServerPort(),listenports))
		{
			/* Hook the user with our module */
			user->AddIOHook(this);
		}
	}

	virtual const char* OnRequest(Request* request)
	{
		ISHRequest* ISR = (ISHRequest*)request;
		if (strcmp("IS_NAME", request->GetId()) == 0)
		{
			return "gnutls";
		}
		else if (strcmp("IS_HOOK", request->GetId()) == 0)
		{
			const char* ret = "OK";
			try
			{
				ret = ISR->Sock->AddIOHook((Module*)this) ? "OK" : NULL;
			}
			catch (ModuleException &e)
			{
				return NULL;
			}
			return ret;
		}
		else if (strcmp("IS_UNHOOK", request->GetId()) == 0)
		{
			return ISR->Sock->DelIOHook() ? "OK" : NULL;
		}
		else if (strcmp("IS_HSDONE", request->GetId()) == 0)
		{
			if (ISR->Sock->GetFd() < 0)
				return "OK";

			issl_session* session = &sessions[ISR->Sock->GetFd()];
			return (session->status == ISSL_HANDSHAKING_READ || session->status == ISSL_HANDSHAKING_WRITE) ? NULL : "OK";
		}
		else if (strcmp("IS_ATTACH", request->GetId()) == 0)
		{
			if (ISR->Sock->GetFd() > -1)
			{
				issl_session* session = &sessions[ISR->Sock->GetFd()];
				if (session->sess)
				{
					if ((Extensible*)ServerInstance->SE->GetRef(ISR->Sock->GetFd()) == (Extensible*)(ISR->Sock))
					{
						VerifyCertificate(session, (BufferedSocket*)ISR->Sock);
						return "OK";
					}
				}
			}
		}
		else if (strcmp("GET_FP", request->GetId()) == 0)
		{
			if (ISR->Sock->GetFd() > -1)
			{
				issl_session* session = &sessions[ISR->Sock->GetFd()];
				if (session->sess)
				{
					Extensible* ext = ISR->Sock;
					ssl_cert* certinfo;
					if (ext->GetExt("ssl_cert",certinfo))
						return certinfo->GetFingerprint().c_str();
				}
			}
		}
		return NULL;
	}


	virtual void OnRawSocketAccept(int fd, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
	{
		/* Are there any possibilities of an out of range fd? Hope not, but lets be paranoid */
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds() - 1))
			return;

		issl_session* session = &sessions[fd];

		/* For STARTTLS: Don't try and init a session on a socket that already has a session */
		if (session->sess)
			return;

		gnutls_init(&session->sess, GNUTLS_SERVER);

		gnutls_set_default_priority(session->sess); // Avoid calling all the priority functions, defaults are adequate.
		gnutls_credentials_set(session->sess, GNUTLS_CRD_CERTIFICATE, x509_cred);
		gnutls_dh_set_prime_bits(session->sess, dh_bits);

		gnutls_transport_set_ptr(session->sess, (gnutls_transport_ptr_t) fd); // Give gnutls the fd for the socket.

		gnutls_certificate_server_set_request(session->sess, GNUTLS_CERT_REQUEST); // Request client certificate if any.

		Handshake(session, fd);
	}

	virtual void OnRawSocketConnect(int fd)
	{
		/* Are there any possibilities of an out of range fd? Hope not, but lets be paranoid */
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds() - 1))
			return;

		issl_session* session = &sessions[fd];

		gnutls_init(&session->sess, GNUTLS_CLIENT);

		gnutls_set_default_priority(session->sess); // Avoid calling all the priority functions, defaults are adequate.
		gnutls_credentials_set(session->sess, GNUTLS_CRD_CERTIFICATE, x509_cred);
		gnutls_dh_set_prime_bits(session->sess, dh_bits);
		gnutls_transport_set_ptr(session->sess, (gnutls_transport_ptr_t) fd); // Give gnutls the fd for the socket.

		Handshake(session, fd);
	}

	virtual void OnRawSocketClose(int fd)
	{
		/* Are there any possibilities of an out of range fd? Hope not, but lets be paranoid */
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds()))
			return;

		CloseSession(&sessions[fd]);

		EventHandler* user = ServerInstance->SE->GetRef(fd);

		if ((user) && (user->GetExt("ssl_cert")))
		{
			ssl_cert* tofree;
			user->GetExt("ssl_cert", tofree);
			delete tofree;
			user->Shrink("ssl_cert");
		}
	}

	virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult)
	{
		/* Are there any possibilities of an out of range fd? Hope not, but lets be paranoid */
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds() - 1))
			return 0;

		issl_session* session = &sessions[fd];

		if (!session->sess)
		{
			readresult = 0;
			CloseSession(session);
			return 1;
		}

		if (session->status == ISSL_HANDSHAKING_READ)
		{
			// The handshake isn't finished, try to finish it.

			if(!Handshake(session, fd))
			{
				errno = session->status == ISSL_CLOSING ? EIO : EAGAIN;
				// Couldn't resume handshake.
				return -1;
			}
		}
		else if (session->status == ISSL_HANDSHAKING_WRITE)
		{
			errno = EAGAIN;
			MakePollWrite(fd);
			return -1;
		}

		// If we resumed the handshake then session->status will be ISSL_HANDSHAKEN.

		if (session->status == ISSL_HANDSHAKEN)
		{
			unsigned int len = 0;
			while (len < count)
			{
				int ret = gnutls_record_recv(session->sess, buffer + len, count - len);
				if (ret > 0)
				{
					len += ret;
				}
				else if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
				{
					break;
				}
				else
				{
					if (ret != 0)
						ServerInstance->Logs->Log("m_ssl_gnutls", DEFAULT,
							"m_ssl_gnutls.so: Error while reading on fd %d: %s",
							fd, gnutls_strerror(ret));

					// if ret == 0, client closed connection.
					readresult = 0;
					CloseSession(session);
					return 1;
				}
			}
			readresult = len;
			if (len)
			{
				return 1;
			}
			else
			{
				errno = EAGAIN;
				return -1;
			}
		}
		else if (session->status == ISSL_CLOSING)
			readresult = 0;

		return 1;
	}

	virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
	{
		/* Are there any possibilities of an out of range fd? Hope not, but lets be paranoid */
		if ((fd < 0) || (fd > ServerInstance->SE->GetMaxFds() - 1))
			return 0;

		issl_session* session = &sessions[fd];
		const char* sendbuffer = buffer;

		if (!session->sess)
		{
			CloseSession(session);
			return 1;
		}

		session->outbuf.append(sendbuffer, count);
		sendbuffer = session->outbuf.c_str();
		count = session->outbuf.size();

		if (session->status == ISSL_HANDSHAKING_WRITE || session->status == ISSL_HANDSHAKING_READ)
		{
			// The handshake isn't finished, try to finish it.
			Handshake(session, fd);
			errno = session->status == ISSL_CLOSING ? EIO : EAGAIN;
			return -1;
		}

		int ret = 0;

		if (session->status == ISSL_HANDSHAKEN)
		{
			ret = gnutls_record_send(session->sess, sendbuffer, count);

			if (ret == 0)
			{
				CloseSession(session);
			}
			else if (ret < 0)
			{
				if(ret != GNUTLS_E_AGAIN && ret != GNUTLS_E_INTERRUPTED)
				{
					ServerInstance->Logs->Log("m_ssl_gnutls", DEFAULT,
							"m_ssl_gnutls.so: Error while writing to fd %d: %s",
							fd, gnutls_strerror(ret));
					CloseSession(session);
				}
				else
				{
					errno = EAGAIN;
				}
			}
			else
			{
				session->outbuf = session->outbuf.substr(ret);
			}
		}

		if (!session->outbuf.empty())
			MakePollWrite(fd);

		/* Who's smart idea was it to return 1 when we havent written anything?
		 * This fucks the buffer up in BufferedSocket :p
		 */
		return ret < 1 ? 0 : ret;
	}

	bool Handshake(issl_session* session, int fd)
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
				}
				else
				{
					// gnutls_handshake() wants to write() again.
					session->status = ISSL_HANDSHAKING_WRITE;
					MakePollWrite(fd);
				}
			}
			else
			{
				// Handshake failed.
				ServerInstance->Logs->Log("m_ssl_gnutls", DEFAULT,
						"m_ssl_gnutls.so: Handshake failed on fd %d: %s",
						fd, gnutls_strerror(ret));
				CloseSession(session);
				session->status = ISSL_CLOSING;
			}

			return false;
		}
		else
		{
			// Handshake complete.
			// This will do for setting the ssl flag...it could be done earlier if it's needed. But this seems neater.
			EventHandler *extendme = ServerInstance->SE->GetRef(fd);
			if (extendme)
			{
				if (!extendme->GetExt("ssl"))
					extendme->Extend("ssl", "ON");
			}

			// Change the seesion state
			session->status = ISSL_HANDSHAKEN;

			// Finish writing, if any left
			MakePollWrite(fd);

			return true;
		}
	}

	virtual void OnPostConnect(User* user)
	{
		// This occurs AFTER OnUserConnect so we can be sure the
		// protocol module has propagated the NICK message.
		if (user->GetIOHook() == this && (IS_LOCAL(user)))
		{
			ssl_cert* certdata = VerifyCertificate(&sessions[user->GetFd()],user);
			if (sessions[user->GetFd()].sess)
			{
				std::string cipher = gnutls_kx_get_name(gnutls_kx_get(sessions[user->GetFd()].sess));
				cipher.append("-").append(gnutls_cipher_get_name(gnutls_cipher_get(sessions[user->GetFd()].sess))).append("-");
				cipher.append(gnutls_mac_get_name(gnutls_mac_get(sessions[user->GetFd()].sess)));
				user->WriteServ("NOTICE %s :*** You are connected using SSL cipher \"%s\"", user->nick.c_str(), cipher.c_str());
			}

			ServerInstance->PI->SendMetaData(user, TYPE_USER, "ssl", "ON");
			if (certdata)
				ServerInstance->PI->SendMetaData(user, TYPE_USER, "ssl_cert", certdata->GetMetaLine().c_str());
		}
	}

	void MakePollWrite(int fd)
	{
		//OnRawSocketWrite(fd, NULL, 0);
		EventHandler* eh = ServerInstance->SE->GetRef(fd);
		if (eh)
			ServerInstance->SE->WantWrite(eh);
	}

	virtual void OnBufferFlushed(User* user)
	{
		if (user->GetIOHook() == this)
		{
			issl_session* session = &sessions[user->GetFd()];
			if (session && session->outbuf.size())
				OnRawSocketWrite(user->GetFd(), NULL, 0);
		}
	}

	void CloseSession(issl_session* session)
	{
		if(session->sess)
		{
			gnutls_bye(session->sess, GNUTLS_SHUT_WR);
			gnutls_deinit(session->sess);
		}

		session->outbuf.clear();
		session->sess = NULL;
		session->status = ISSL_NONE;
	}

	ssl_cert* VerifyCertificate(issl_session* session, Extensible* user)
	{
		if (!session->sess || !user)
			return NULL;

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

		user->Extend("ssl_cert",certinfo);

		/* This verification function uses the trusted CAs in the credentials
		 * structure. So you must have installed one or more CA certificates.
		 */
		ret = gnutls_certificate_verify_peers2(session->sess, &status);

		if (ret < 0)
		{
			certinfo->error = std::string(gnutls_strerror(ret));
			return certinfo;
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
			return certinfo;
		}

		ret = gnutls_x509_crt_init(&cert);
		if (ret < 0)
		{
			certinfo->error = gnutls_strerror(ret);
			return certinfo;
		}

		cert_list_size = 0;
		cert_list = gnutls_certificate_get_peers(session->sess, &cert_list_size);
		if (cert_list == NULL)
		{
			certinfo->error = "No certificate was found";
			return certinfo;
		}

		/* This is not a real world example, since we only check the first
		 * certificate in the given chain.
		 */

		ret = gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER);
		if (ret < 0)
		{
			certinfo->error = gnutls_strerror(ret);
			return certinfo;
		}

		gnutls_x509_crt_get_dn(cert, name, &name_size);
		certinfo->dn = name;

		gnutls_x509_crt_get_issuer_dn(cert, name, &name_size);
		certinfo->issuer = name;

		if ((ret = gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_MD5, digest, &digest_size)) < 0)
		{
			certinfo->error = gnutls_strerror(ret);
		}
		else
		{
			certinfo->fingerprint = irc::hex(digest, digest_size);
		}

		/* Beware here we do not check for errors.
		 */
		if ((gnutls_x509_crt_get_expiration_time(cert) < ServerInstance->Time()) || (gnutls_x509_crt_get_activation_time(cert) > ServerInstance->Time()))
		{
			certinfo->error = "Not activated, or expired certificate";
		}

		gnutls_x509_crt_deinit(cert);

		return certinfo;
	}

	void OnEvent(Event* ev)
	{
		GenericCapHandler(ev, "tls", "tls");
	}

	void Prioritize()
	{
		Module* server = ServerInstance->Modules->Find("m_spanningtree.so");
		ServerInstance->Modules->SetPriority(this, I_OnPostConnect, PRIORITY_AFTER, &server);
	}
};

MODULE_INIT(ModuleSSLGnuTLS)
