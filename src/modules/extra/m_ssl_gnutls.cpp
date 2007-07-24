/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "inspircd_config.h"
#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "socket.h"
#include "hashcomp.h"
#include "transport.h"

#ifdef WINDOWS
#pragma comment(lib, "libgnutls-13.lib")
#undef MAX_DESCRIPTORS
#define MAX_DESCRIPTORS 10000
#endif

/* $ModDesc: Provides SSL support for clients */
/* $CompileFlags: exec("libgnutls-config --cflags") */
/* $LinkerFlags: rpath("libgnutls-config --libs") exec("libgnutls-config --libs") */
/* $ModDep: transport.h */


enum issl_status { ISSL_NONE, ISSL_HANDSHAKING_READ, ISSL_HANDSHAKING_WRITE, ISSL_HANDSHAKEN, ISSL_CLOSING, ISSL_CLOSED };

bool isin(int port, const std::vector<int> &portlist)
{
	for(unsigned int i = 0; i < portlist.size(); i++)
		if(portlist[i] == port)
			return true;

	return false;
}

/** Represents an SSL user's extra data
 */
class issl_session : public classbase
{
public:
	gnutls_session_t sess;
	issl_status status;
	std::string outbuf;
	int inbufoffset;
	char* inbuf;
	int fd;
};

class ModuleSSLGnuTLS : public Module
{

	ConfigReader* Conf;

	char* dummy;

	std::vector<int> listenports;

	int inbufsize;
	issl_session sessions[MAX_DESCRIPTORS];

	gnutls_certificate_credentials x509_cred;
	gnutls_dh_params dh_params;

	std::string keyfile;
	std::string certfile;
	std::string cafile;
	std::string crlfile;
	std::string sslports;
	int dh_bits;

	int clientactive;

 public:

	ModuleSSLGnuTLS(InspIRCd* Me)
		: Module(Me)
	{
		ServerInstance->PublishInterface("InspSocketHook", this);

		// Not rehashable...because I cba to reduce all the sizes of existing buffers.
		inbufsize = ServerInstance->Config->NetBufferSize;

		gnutls_global_init(); // This must be called once in the program

		if(gnutls_certificate_allocate_credentials(&x509_cred) != 0)
			ServerInstance->Log(DEFAULT, "m_ssl_gnutls.so: Failed to allocate certificate credentials");

		// Guessing return meaning
		if(gnutls_dh_params_init(&dh_params) < 0)
			ServerInstance->Log(DEFAULT, "m_ssl_gnutls.so: Failed to initialise DH parameters");

		// Needs the flag as it ignores a plain /rehash
		OnRehash(NULL,"ssl");

		// Void return, guess we assume success
		gnutls_certificate_set_dh_params(x509_cred, dh_params);
	}

	virtual void OnRehash(userrec* user, const std::string &param)
	{
		if(param != "ssl")
			return;

		Conf = new ConfigReader(ServerInstance);

		for(unsigned int i = 0; i < listenports.size(); i++)
		{
			ServerInstance->Config->DelIOHook(listenports[i]);
		}

		listenports.clear();
		clientactive = 0;
		sslports.clear();

		for(int i = 0; i < Conf->Enumerate("bind"); i++)
		{
			// For each <bind> tag
			std::string x = Conf->ReadValue("bind", "type", i);
			if(((x.empty()) || (x == "clients")) && (Conf->ReadValue("bind", "ssl", i) == "gnutls"))
			{
				// Get the port we're meant to be listening on with SSL
				std::string port = Conf->ReadValue("bind", "port", i);
				irc::portparser portrange(port, false);
				long portno = -1;
				while ((portno = portrange.GetToken()))
				{
					clientactive++;
					try
					{
						if (ServerInstance->Config->AddIOHook(portno, this))
						{
							listenports.push_back(portno);
							for (size_t i = 0; i < ServerInstance->Config->ports.size(); i++)
								if (ServerInstance->Config->ports[i]->GetPort() == portno)
									ServerInstance->Config->ports[i]->SetDescription("ssl");
							ServerInstance->Log(DEFAULT, "m_ssl_gnutls.so: Enabling SSL for port %d", portno);
							sslports.append("*:").append(ConvToStr(portno)).append(";");
						}
						else
						{
							ServerInstance->Log(DEFAULT, "m_ssl_gnutls.so: FAILED to enable SSL on port %d, maybe you have another ssl or similar module loaded?", portno);
						}
					}
					catch (ModuleException &e)
					{
						ServerInstance->Log(DEFAULT, "m_ssl_gnutls.so: FAILED to enable SSL on port %d: %s. Maybe it's already hooked by the same port on a different IP, or you have an other SSL or similar module loaded?", portno, e.GetReason());
					}
				}
			}
		}

		std::string confdir(ServerInstance->ConfigFileName);
		// +1 so we the path ends with a /
		confdir = confdir.substr(0, confdir.find_last_of('/') + 1);

		cafile	= Conf->ReadValue("gnutls", "cafile", 0);
		crlfile	= Conf->ReadValue("gnutls", "crlfile", 0);
		certfile	= Conf->ReadValue("gnutls", "certfile", 0);
		keyfile	= Conf->ReadValue("gnutls", "keyfile", 0);
		dh_bits	= Conf->ReadInteger("gnutls", "dhbits", 0, false);

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
		if(cafile[0] != '/')
			cafile = confdir + cafile;

		if(crlfile[0] != '/')
			crlfile = confdir + crlfile;

		if(certfile[0] != '/')
			certfile = confdir + certfile;

		if(keyfile[0] != '/')
			keyfile = confdir + keyfile;

		int ret;

		if((ret =gnutls_certificate_set_x509_trust_file(x509_cred, cafile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
			ServerInstance->Log(DEFAULT, "m_ssl_gnutls.so: Failed to set X.509 trust file '%s': %s", cafile.c_str(), gnutls_strerror(ret));

		if((ret = gnutls_certificate_set_x509_crl_file (x509_cred, crlfile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
			ServerInstance->Log(DEFAULT, "m_ssl_gnutls.so: Failed to set X.509 CRL file '%s': %s", crlfile.c_str(), gnutls_strerror(ret));

		if((ret = gnutls_certificate_set_x509_key_file (x509_cred, certfile.c_str(), keyfile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
		{
			// If this fails, no SSL port will work. At all. So, do the smart thing - throw a ModuleException
			throw ModuleException("Unable to load GnuTLS server certificate: " + std::string(gnutls_strerror(ret)));
		}

		// This may be on a large (once a day or week) timer eventually.
		GenerateDHParams();

		DELETE(Conf);
	}

	void GenerateDHParams()
	{
 		// Generate Diffie Hellman parameters - for use with DHE
		// kx algorithms. These should be discarded and regenerated
		// once a day, once a week or once a month. Depending on the
		// security requirements.

		int ret;

		if((ret = gnutls_dh_params_generate2(dh_params, dh_bits)) < 0)
			ServerInstance->Log(DEFAULT, "m_ssl_gnutls.so: Failed to generate DH parameters (%d bits): %s", dh_bits, gnutls_strerror(ret));
	}

	virtual ~ModuleSSLGnuTLS()
	{
		gnutls_dh_params_deinit(dh_params);
		gnutls_certificate_free_credentials(x509_cred);
		gnutls_global_deinit();
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;

			if(user->GetExt("ssl", dummy) && isin(user->GetPort(), listenports))
			{
				// User is using SSL, they're a local user, and they're using one of *our* SSL ports.
				// Potentially there could be multiple SSL modules loaded at once on different ports.
				userrec::QuitUser(ServerInstance, user, "SSL module unloading");
			}
			if (user->GetExt("ssl_cert", dummy) && isin(user->GetPort(), listenports))
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
				ServerInstance->Config->DelIOHook(listenports[i]);
				for (size_t j = 0; j < ServerInstance->Config->ports.size(); j++)
					if (ServerInstance->Config->ports[j]->GetPort() == listenports[i])
						ServerInstance->Config->ports[j]->SetDescription("plaintext");
			}
		}
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnRawSocketConnect] = List[I_OnRawSocketAccept] = List[I_OnRawSocketClose] = List[I_OnRawSocketRead] = List[I_OnRawSocketWrite] = List[I_OnCleanup] = 1;
		List[I_OnRequest] = List[I_OnSyncUserMetaData] = List[I_OnDecodeMetaData] = List[I_OnUnloadModule] = List[I_OnRehash] = List[I_OnWhois] = List[I_OnPostConnect] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" SSL=" + sslports);
	}

	virtual char* OnRequest(Request* request)
	{
		ISHRequest* ISR = (ISHRequest*)request;
		if (strcmp("IS_NAME", request->GetId()) == 0)
		{
			return "gnutls";
		}
		else if (strcmp("IS_HOOK", request->GetId()) == 0)
		{
			char* ret = "OK";
			try
			{
				ret = ServerInstance->Config->AddIOHook((Module*)this, (InspSocket*)ISR->Sock) ? (char*)"OK" : NULL;
			}
			catch (ModuleException &e)
			{
				return NULL;
			}
			return ret;
		}
		else if (strcmp("IS_UNHOOK", request->GetId()) == 0)
		{
			return ServerInstance->Config->DelIOHook((InspSocket*)ISR->Sock) ? (char*)"OK" : NULL;
		}
		else if (strcmp("IS_HSDONE", request->GetId()) == 0)
		{
			if (ISR->Sock->GetFd() < 0)
				return (char*)"OK";

			issl_session* session = &sessions[ISR->Sock->GetFd()];
			return (session->status == ISSL_HANDSHAKING_READ || session->status == ISSL_HANDSHAKING_WRITE) ? NULL : (char*)"OK";
		}
		else if (strcmp("IS_ATTACH", request->GetId()) == 0)
		{
			if (ISR->Sock->GetFd() > -1)
			{
				issl_session* session = &sessions[ISR->Sock->GetFd()];
				if (session->sess)
				{
					if ((Extensible*)ServerInstance->FindDescriptor(ISR->Sock->GetFd()) == (Extensible*)(ISR->Sock))
					{
						VerifyCertificate(session, (InspSocket*)ISR->Sock);
						return "OK";
					}
				}
			}
		}
		return NULL;
	}


	virtual void OnRawSocketAccept(int fd, const std::string &ip, int localport)
	{
		issl_session* session = &sessions[fd];

		session->fd = fd;
		session->inbuf = new char[inbufsize];
		session->inbufoffset = 0;

		gnutls_init(&session->sess, GNUTLS_SERVER);

		gnutls_set_default_priority(session->sess); // Avoid calling all the priority functions, defaults are adequate.
		gnutls_credentials_set(session->sess, GNUTLS_CRD_CERTIFICATE, x509_cred);
		gnutls_dh_set_prime_bits(session->sess, dh_bits);

		/* This is an experimental change to avoid a warning on 64bit systems about casting between integer and pointer of different sizes
		 * This needs testing, but it's easy enough to rollback if need be
		 * Old: gnutls_transport_set_ptr(session->sess, (gnutls_transport_ptr_t) fd); // Give gnutls the fd for the socket.
		 * New: gnutls_transport_set_ptr(session->sess, &fd); // Give gnutls the fd for the socket.
		 *
		 * With testing this seems to...not work :/
		 */

		gnutls_transport_set_ptr(session->sess, (gnutls_transport_ptr_t) fd); // Give gnutls the fd for the socket.

		gnutls_certificate_server_set_request(session->sess, GNUTLS_CERT_REQUEST); // Request client certificate if any.

		Handshake(session);
	}

	virtual void OnRawSocketConnect(int fd)
	{
		issl_session* session = &sessions[fd];

		session->fd = fd;
		session->inbuf = new char[inbufsize];
		session->inbufoffset = 0;

		gnutls_init(&session->sess, GNUTLS_CLIENT);

		gnutls_set_default_priority(session->sess); // Avoid calling all the priority functions, defaults are adequate.
		gnutls_credentials_set(session->sess, GNUTLS_CRD_CERTIFICATE, x509_cred);
		gnutls_dh_set_prime_bits(session->sess, dh_bits);
		gnutls_transport_set_ptr(session->sess, (gnutls_transport_ptr_t) fd); // Give gnutls the fd for the socket.

		Handshake(session);
	}

	virtual void OnRawSocketClose(int fd)
	{
		CloseSession(&sessions[fd]);

		EventHandler* user = ServerInstance->SE->GetRef(fd);

		if ((user) && (user->GetExt("ssl_cert", dummy)))
		{
			ssl_cert* tofree;
			user->GetExt("ssl_cert", tofree);
			delete tofree;
			user->Shrink("ssl_cert");
		}
	}

	virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult)
	{
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

			if(!Handshake(session))
			{
				// Couldn't resume handshake.
				return -1;
			}
		}
		else if (session->status == ISSL_HANDSHAKING_WRITE)
		{
			errno = EAGAIN;
			return -1;
		}

		// If we resumed the handshake then session->status will be ISSL_HANDSHAKEN.

		if (session->status == ISSL_HANDSHAKEN)
		{
			// Is this right? Not sure if the unencrypted data is garaunteed to be the same length.
			// Read into the inbuffer, offset from the beginning by the amount of data we have that insp hasn't taken yet.
			int ret = gnutls_record_recv(session->sess, session->inbuf + session->inbufoffset, inbufsize - session->inbufoffset);

			if (ret == 0)
			{
				// Client closed connection.
				readresult = 0;
				CloseSession(session);
				return 1;
			}
			else if (ret < 0)
			{
				if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
				{
					errno = EAGAIN;
					return -1;
				}
				else
				{
					readresult = 0;
					CloseSession(session);
				}
			}
			else
			{
				// Read successfully 'ret' bytes into inbuf + inbufoffset
				// There are 'ret' + 'inbufoffset' bytes of data in 'inbuf'
				// 'buffer' is 'count' long

				unsigned int length = ret + session->inbufoffset;

				if(count <= length)
				{
					memcpy(buffer, session->inbuf, count);
					// Move the stuff left in inbuf to the beginning of it
					memcpy(session->inbuf, session->inbuf + count, (length - count));
					// Now we need to set session->inbufoffset to the amount of data still waiting to be handed to insp.
					session->inbufoffset = length - count;
					// Insp uses readresult as the count of how much data there is in buffer, so:
					readresult = count;
				}
				else
				{
					// There's not as much in the inbuf as there is space in the buffer, so just copy the whole thing.
					memcpy(buffer, session->inbuf, length);
					// Zero the offset, as there's nothing there..
					session->inbufoffset = 0;
					// As above
					readresult = length;
				}
			}
		}
		else if(session->status == ISSL_CLOSING)
			readresult = 0;

		return 1;
	}

	virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
	{
		if (!count)
			return 0;

		issl_session* session = &sessions[fd];
		const char* sendbuffer = buffer;

		if (!session->sess)
		{
			ServerInstance->Log(DEBUG,"No session");
			CloseSession(session);
			return 1;
		}

		session->outbuf.append(sendbuffer, count);
		sendbuffer = session->outbuf.c_str();
		count = session->outbuf.size();

		if (session->status == ISSL_HANDSHAKING_WRITE)
		{
			// The handshake isn't finished, try to finish it.
			ServerInstance->Log(DEBUG,"Finishing handshake");
			Handshake(session);
			errno = EAGAIN;
			return -1;
		}

		int ret = 0;

		if (session->status == ISSL_HANDSHAKEN)
		{
			ServerInstance->Log(DEBUG,"Send record");
			ret = gnutls_record_send(session->sess, sendbuffer, count);
			ServerInstance->Log(DEBUG,"Return: %d", ret);

			if (ret == 0)
			{
				CloseSession(session);
			}
			else if (ret < 0)
			{
				if(ret != GNUTLS_E_AGAIN && ret != GNUTLS_E_INTERRUPTED)
				{
					ServerInstance->Log(DEBUG,"Not egain or interrupt, close session");
					CloseSession(session);
				}
				else
				{
					ServerInstance->Log(DEBUG,"Again please");
					errno = EAGAIN;
					return -1;
				}
			}
			else
			{
				ServerInstance->Log(DEBUG,"Trim buffer");
				session->outbuf = session->outbuf.substr(ret);
			}
		}

		/* Who's smart idea was it to return 1 when we havent written anything?
		 * This fucks the buffer up in InspSocket :p
		 */
		return ret < 1 ? 0 : ret;
	}

	// :kenny.chatspike.net 320 Om Epy|AFK :is a Secure Connection
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		if (!clientactive)
			return;

		// Bugfix, only send this numeric for *our* SSL users
		if(dest->GetExt("ssl", dummy) || (IS_LOCAL(dest) &&  isin(dest->GetPort(), listenports)))
		{
			ServerInstance->SendWhoisLine(source, dest, 320, "%s %s :is using a secure connection", source->nick, dest->nick);
		}
	}

	virtual void OnSyncUserMetaData(userrec* user, Module* proto, void* opaque, const std::string &extname, bool displayable)
	{
		// check if the linking module wants to know about OUR metadata
		if(extname == "ssl")
		{
			// check if this user has an swhois field to send
			if(user->GetExt(extname, dummy))
			{
				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, displayable ? "Enabled" : "ON");
			}
		}
	}

	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_USER) && (extname == "ssl"))
		{
			userrec* dest = (userrec*)target;
			// if they dont already have an ssl flag, accept the remote server's
			if (!dest->GetExt(extname, dummy))
			{
				dest->Extend(extname, "ON");
			}
		}
	}

	bool Handshake(issl_session* session)
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
					MakePollWrite(session);
				}
			}
			else
			{
				// Handshake failed.
				CloseSession(session);
				session->status = ISSL_CLOSING;
			}

			return false;
		}
		else
		{
			// Handshake complete.
			// This will do for setting the ssl flag...it could be done earlier if it's needed. But this seems neater.
			userrec* extendme = ServerInstance->FindDescriptor(session->fd);
			if (extendme)
			{
				if (!extendme->GetExt("ssl", dummy))
					extendme->Extend("ssl", "ON");
			}

			// Change the seesion state
			session->status = ISSL_HANDSHAKEN;

			// Finish writing, if any left
			MakePollWrite(session);

			return true;
		}
	}

	virtual void OnPostConnect(userrec* user)
	{
		// This occurs AFTER OnUserConnect so we can be sure the
		// protocol module has propogated the NICK message.
		if ((user->GetExt("ssl", dummy)) && (IS_LOCAL(user)))
		{
			// Tell whatever protocol module we're using that we need to inform other servers of this metadata NOW.
			std::deque<std::string>* metadata = new std::deque<std::string>;
			metadata->push_back(user->nick);
			metadata->push_back("ssl");		// The metadata id
			metadata->push_back("ON");		// The value to send
			Event* event = new Event((char*)metadata,(Module*)this,"send_metadata");
			event->Send(ServerInstance);		// Trigger the event. We don't care what module picks it up.
			DELETE(event);
			DELETE(metadata);

			VerifyCertificate(&sessions[user->GetFd()],user);
			if (sessions[user->GetFd()].sess)
			{
				std::string cipher = gnutls_kx_get_name(gnutls_kx_get(sessions[user->GetFd()].sess));
				cipher.append("-").append(gnutls_cipher_get_name(gnutls_cipher_get(sessions[user->GetFd()].sess))).append("-");
				cipher.append(gnutls_mac_get_name(gnutls_mac_get(sessions[user->GetFd()].sess)));
				user->WriteServ("NOTICE %s :*** You are connected using SSL cipher \"%s\"", user->nick, cipher.c_str());
			}
		}
	}

	void MakePollWrite(issl_session* session)
	{
		OnRawSocketWrite(session->fd, NULL, 0);
	}

	void CloseSession(issl_session* session)
	{
		if(session->sess)
		{
			gnutls_bye(session->sess, GNUTLS_SHUT_WR);
			gnutls_deinit(session->sess);
		}

		if(session->inbuf)
		{
			delete[] session->inbuf;
		}

		session->outbuf.clear();
		session->inbuf = NULL;
		session->sess = NULL;
		session->status = ISSL_NONE;
	}

	void VerifyCertificate(issl_session* session, Extensible* user)
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

		user->Extend("ssl_cert",certinfo);

		/* This verification function uses the trusted CAs in the credentials
		 * structure. So you must have installed one or more CA certificates.
		 */
		ret = gnutls_certificate_verify_peers2(session->sess, &status);

		if (ret < 0)
		{
			certinfo->data.insert(std::make_pair("error",std::string(gnutls_strerror(ret))));
			return;
		}

		if (status & GNUTLS_CERT_INVALID)
		{
			certinfo->data.insert(std::make_pair("invalid",ConvToStr(1)));
		}
		else
		{
			certinfo->data.insert(std::make_pair("invalid",ConvToStr(0)));
		}
		if (status & GNUTLS_CERT_SIGNER_NOT_FOUND)
		{
			certinfo->data.insert(std::make_pair("unknownsigner",ConvToStr(1)));
		}
		else
		{
			certinfo->data.insert(std::make_pair("unknownsigner",ConvToStr(0)));
		}
		if (status & GNUTLS_CERT_REVOKED)
		{
			certinfo->data.insert(std::make_pair("revoked",ConvToStr(1)));
		}
		else
		{
			certinfo->data.insert(std::make_pair("revoked",ConvToStr(0)));
		}
		if (status & GNUTLS_CERT_SIGNER_NOT_CA)
		{
			certinfo->data.insert(std::make_pair("trusted",ConvToStr(0)));
		}
		else
		{
			certinfo->data.insert(std::make_pair("trusted",ConvToStr(1)));
		}

		/* Up to here the process is the same for X.509 certificates and
		 * OpenPGP keys. From now on X.509 certificates are assumed. This can
		 * be easily extended to work with openpgp keys as well.
		 */
		if (gnutls_certificate_type_get(session->sess) != GNUTLS_CRT_X509)
		{
			certinfo->data.insert(std::make_pair("error","No X509 keys sent"));
			return;
		}

		ret = gnutls_x509_crt_init(&cert);
		if (ret < 0)
		{
			certinfo->data.insert(std::make_pair("error",gnutls_strerror(ret)));
			return;
		}

		cert_list_size = 0;
		cert_list = gnutls_certificate_get_peers(session->sess, &cert_list_size);
		if (cert_list == NULL)
		{
			certinfo->data.insert(std::make_pair("error","No certificate was found"));
			return;
		}

		/* This is not a real world example, since we only check the first
		 * certificate in the given chain.
		 */

		ret = gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER);
		if (ret < 0)
		{
			certinfo->data.insert(std::make_pair("error",gnutls_strerror(ret)));
			return;
		}

		gnutls_x509_crt_get_dn(cert, name, &name_size);

		certinfo->data.insert(std::make_pair("dn",name));

		gnutls_x509_crt_get_issuer_dn(cert, name, &name_size);

		certinfo->data.insert(std::make_pair("issuer",name));

		if ((ret = gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_MD5, digest, &digest_size)) < 0)
		{
			certinfo->data.insert(std::make_pair("error",gnutls_strerror(ret)));
		}
		else
		{
			certinfo->data.insert(std::make_pair("fingerprint",irc::hex(digest, digest_size)));
		}

		/* Beware here we do not check for errors.
		 */
		if ((gnutls_x509_crt_get_expiration_time(cert) < time(0)) || (gnutls_x509_crt_get_activation_time(cert) > time(0)))
		{
			certinfo->data.insert(std::make_pair("error","Not activated, or expired certificate"));
		}

		gnutls_x509_crt_deinit(cert);

		return;
	}

};

MODULE_INIT(ModuleSSLGnuTLS);

