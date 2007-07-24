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

#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef WINDOWS
#include <openssl/applink.c>
#endif

#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "socket.h"
#include "hashcomp.h"

#include "transport.h"

#ifdef WINDOWS
#pragma comment(lib, "libeay32MTd")
#pragma comment(lib, "ssleay32MTd")
#undef MAX_DESCRIPTORS
#define MAX_DESCRIPTORS 10000
#endif

/* $ModDesc: Provides SSL support for clients */
/* $CompileFlags: pkgconfversion("openssl","0.9.7") pkgconfincludes("openssl","/openssl/ssl.h","") */
/* $LinkerFlags: rpath("pkg-config --libs openssl") pkgconflibs("openssl","/libssl.so","-lssl -lcrypto -ldl") */
/* $ModDep: transport.h */

enum issl_status { ISSL_NONE, ISSL_HANDSHAKING, ISSL_OPEN };
enum issl_io_status { ISSL_WRITE, ISSL_READ };

static bool SelfSigned = false;

bool isin(int port, const std::vector<int> &portlist)
{
	for(unsigned int i = 0; i < portlist.size(); i++)
		if(portlist[i] == port)
			return true;

	return false;
}

char* get_error()
{
	return ERR_error_string(ERR_get_error(), NULL);
}

static int error_callback(const char *str, size_t len, void *u);

/** Represents an SSL user's extra data
 */
class issl_session : public classbase
{
public:
	SSL* sess;
	issl_status status;
	issl_io_status rstat;
	issl_io_status wstat;

	unsigned int inbufoffset;
	char* inbuf; 			// Buffer OpenSSL reads into.
	std::string outbuf;	// Buffer for outgoing data that OpenSSL will not take.
	int fd;
	bool outbound;

	issl_session()
	{
		outbound = false;
		rstat = ISSL_READ;
		wstat = ISSL_WRITE;
	}
};

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

class ModuleSSLOpenSSL : public Module
{

	ConfigReader* Conf;

	std::vector<int> listenports;

	int inbufsize;
	issl_session sessions[MAX_DESCRIPTORS];

	SSL_CTX* ctx;
	SSL_CTX* clictx;

	char* dummy;
	char cipher[MAXBUF];

	std::string keyfile;
	std::string certfile;
	std::string cafile;
	// std::string crlfile;
	std::string dhfile;
	std::string sslports;

	int clientactive;

 public:

	InspIRCd* PublicInstance;

	ModuleSSLOpenSSL(InspIRCd* Me)
		: Module(Me), PublicInstance(Me)
	{
		ServerInstance->PublishInterface("InspSocketHook", this);

		// Not rehashable...because I cba to reduce all the sizes of existing buffers.
		inbufsize = ServerInstance->Config->NetBufferSize;

		/* Global SSL library initialization*/
		SSL_library_init();
		SSL_load_error_strings();

		/* Build our SSL contexts:
		 * NOTE: OpenSSL makes us have two contexts, one for servers and one for clients. ICK.
		 */
		ctx = SSL_CTX_new( SSLv23_server_method() );
		clictx = SSL_CTX_new( SSLv23_client_method() );

		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, OnVerify);
		SSL_CTX_set_verify(clictx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, OnVerify);

		// Needs the flag as it ignores a plain /rehash
		OnRehash(NULL,"ssl");
	}

	virtual void OnRehash(userrec* user, const std::string &param)
	{
		if (param != "ssl")
			return;

		Conf = new ConfigReader(ServerInstance);

		for (unsigned int i = 0; i < listenports.size(); i++)
		{
			ServerInstance->Config->DelIOHook(listenports[i]);
		}

		listenports.clear();
		clientactive = 0;
		sslports.clear();

		for (int i = 0; i < Conf->Enumerate("bind"); i++)
		{
			// For each <bind> tag
			std::string x = Conf->ReadValue("bind", "type", i);
			if (((x.empty()) || (x == "clients")) && (Conf->ReadValue("bind", "ssl", i) == "openssl"))
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
							ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: Enabling SSL for port %d", portno);
							sslports.append("*:").append(ConvToStr(portno)).append(";");
						}
						else
						{
							ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: FAILED to enable SSL on port %d, maybe you have another ssl or similar module loaded?",	portno);
						}
					}
					catch (ModuleException &e)
					{
						ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: FAILED to enable SSL on port %d: %s. Maybe it's already hooked by the same port on a different IP, or you have another SSL or similar module loaded?", portno, e.GetReason());
					}
				}
			}
		}

		if (!sslports.empty())
			sslports.erase(sslports.end() - 1);

		std::string confdir(ServerInstance->ConfigFileName);
		// +1 so we the path ends with a /
		confdir = confdir.substr(0, confdir.find_last_of('/') + 1);

		cafile	 = Conf->ReadValue("openssl", "cafile", 0);
		certfile = Conf->ReadValue("openssl", "certfile", 0);
		keyfile	 = Conf->ReadValue("openssl", "keyfile", 0);
		dhfile	 = Conf->ReadValue("openssl", "dhfile", 0);

		// Set all the default values needed.
		if (cafile.empty())
			cafile = "ca.pem";

		if (certfile.empty())
			certfile = "cert.pem";

		if (keyfile.empty())
			keyfile = "key.pem";

		if (dhfile.empty())
			dhfile = "dhparams.pem";

		// Prepend relative paths with the path to the config directory.
		if (cafile[0] != '/')
			cafile = confdir + cafile;

		if (certfile[0] != '/')
			certfile = confdir + certfile;

		if (keyfile[0] != '/')
			keyfile = confdir + keyfile;

		if (dhfile[0] != '/')
			dhfile = confdir + dhfile;

		/* Load our keys and certificates
		 * NOTE: OpenSSL's error logging API sucks, don't blame us for this clusterfuck.
		 */
		if ((!SSL_CTX_use_certificate_chain_file(ctx, certfile.c_str())) || (!SSL_CTX_use_certificate_chain_file(clictx, certfile.c_str())))
		{
			ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: Can't read certificate file %s. %s", certfile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		if (((!SSL_CTX_use_PrivateKey_file(ctx, keyfile.c_str(), SSL_FILETYPE_PEM))) || (!SSL_CTX_use_PrivateKey_file(clictx, keyfile.c_str(), SSL_FILETYPE_PEM)))
		{
			ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: Can't read key file %s. %s", keyfile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		/* Load the CAs we trust*/
		if (((!SSL_CTX_load_verify_locations(ctx, cafile.c_str(), 0))) || (!SSL_CTX_load_verify_locations(clictx, cafile.c_str(), 0)))
		{
			ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: Can't read CA list from %s. %s", cafile.c_str(), strerror(errno));
			ERR_print_errors_cb(error_callback, this);
		}

		FILE* dhpfile = fopen(dhfile.c_str(), "r");
		DH* ret;

		if (dhpfile == NULL)
		{
			ServerInstance->Log(DEFAULT, "m_ssl_openssl.so Couldn't open DH file %s: %s", dhfile.c_str(), strerror(errno));
			throw ModuleException("Couldn't open DH file " + dhfile + ": " + strerror(errno));
		}
		else
		{
			ret = PEM_read_DHparams(dhpfile, NULL, NULL, NULL);
			if ((SSL_CTX_set_tmp_dh(ctx, ret) < 0) || (SSL_CTX_set_tmp_dh(clictx, ret) < 0))
			{
				ServerInstance->Log(DEFAULT, "m_ssl_openssl.so: Couldn't set DH parameters %s. SSL errors follow:", dhfile.c_str());
				ERR_print_errors_cb(error_callback, this);
			}
		}

		fclose(dhpfile);

		DELETE(Conf);
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" SSL=" + sslports);
	}

	virtual ~ModuleSSLOpenSSL()
	{
		SSL_CTX_free(ctx);
		SSL_CTX_free(clictx);
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;

			if (user->GetExt("ssl", dummy) && IS_LOCAL(user) && isin(user->GetPort(), listenports))
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
		if (mod == this)
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
		List[I_OnRawSocketConnect] = List[I_OnRawSocketAccept] = List[I_OnRawSocketClose] = List[I_OnRawSocketRead] = List[I_OnRawSocketWrite] = List[I_OnCleanup] = List[I_On005Numeric] = 1;
		List[I_OnRequest] = List[I_OnSyncUserMetaData] = List[I_OnDecodeMetaData] = List[I_OnUnloadModule] = List[I_OnRehash] = List[I_OnWhois] = List[I_OnPostConnect] = 1;
	}

	virtual char* OnRequest(Request* request)
	{
		ISHRequest* ISR = (ISHRequest*)request;
		if (strcmp("IS_NAME", request->GetId()) == 0)
		{
			return "openssl";
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
			ServerInstance->Log(DEBUG,"Module checking if handshake is done");
			if (ISR->Sock->GetFd() < 0)
				return (char*)"OK";

			issl_session* session = &sessions[ISR->Sock->GetFd()];
			return (session->status == ISSL_HANDSHAKING) ? NULL : (char*)"OK";
		}
		else if (strcmp("IS_ATTACH", request->GetId()) == 0)
		{
			issl_session* session = &sessions[ISR->Sock->GetFd()];
			if (session->sess)
			{
				VerifyCertificate(session, (InspSocket*)ISR->Sock);
				return "OK";
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
		session->sess = SSL_new(ctx);
		session->status = ISSL_NONE;
		session->outbound = false;

		if (session->sess == NULL)
			return;

		if (SSL_set_fd(session->sess, fd) == 0)
		{
			ServerInstance->Log(DEBUG,"BUG: Can't set fd with SSL_set_fd: %d", fd);
			return;
		}

 		Handshake(session);
	}

	virtual void OnRawSocketConnect(int fd)
	{
		ServerInstance->Log(DEBUG,"OnRawSocketConnect connecting");
		issl_session* session = &sessions[fd];

		session->fd = fd;
		session->inbuf = new char[inbufsize];
		session->inbufoffset = 0;
		session->sess = SSL_new(clictx);
		session->status = ISSL_NONE;
		session->outbound = true;

		if (session->sess == NULL)
			return;

		if (SSL_set_fd(session->sess, fd) == 0)
		{
			ServerInstance->Log(DEBUG,"BUG: Can't set fd with SSL_set_fd: %d", fd);
			return;
		}

		Handshake(session);
		ServerInstance->Log(DEBUG,"Exiting OnRawSocketConnect");
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

		ServerInstance->Log(DEBUG,"OnRawSocketRead");

		if (!session->sess)
		{
			ServerInstance->Log(DEBUG,"OnRawSocketRead has no session");
			readresult = 0;
			CloseSession(session);
			return 1;
		}

		if (session->status == ISSL_HANDSHAKING)
		{
			if (session->rstat == ISSL_READ || session->wstat == ISSL_READ)
			{
				ServerInstance->Log(DEBUG,"Resume handshake in read");
				// The handshake isn't finished and it wants to read, try to finish it.
				if (!Handshake(session))
				{
					ServerInstance->Log(DEBUG,"Cant resume handshake in read");
					// Couldn't resume handshake.
					return -1;
				}
			}
			else
			{
				errno = EAGAIN;
				return -1;
			}
		}

		// If we resumed the handshake then session->status will be ISSL_OPEN

		if (session->status == ISSL_OPEN)
		{
			if (session->wstat == ISSL_READ)
			{
				if(DoWrite(session) == 0)
					return 0;
			}

			if (session->rstat == ISSL_READ)
			{
				int ret = DoRead(session);

				if (ret > 0)
				{
					if (count <= session->inbufoffset)
					{
						memcpy(buffer, session->inbuf, count);
						// Move the stuff left in inbuf to the beginning of it
						memcpy(session->inbuf, session->inbuf + count, (session->inbufoffset - count));
						// Now we need to set session->inbufoffset to the amount of data still waiting to be handed to insp.
						session->inbufoffset -= count;
						// Insp uses readresult as the count of how much data there is in buffer, so:
						readresult = count;
					}
					else
					{
						// There's not as much in the inbuf as there is space in the buffer, so just copy the whole thing.
						memcpy(buffer, session->inbuf, session->inbufoffset);

						readresult = session->inbufoffset;
						// Zero the offset, as there's nothing there..
						session->inbufoffset = 0;
					}

					return 1;
				}
				else
				{
					return ret;
				}
			}
		}

		return -1;
	}

	virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
	{
		issl_session* session = &sessions[fd];

		if (!session->sess)
		{
			ServerInstance->Log(DEBUG,"Close session missing sess");
			CloseSession(session);
			return -1;
		}

		session->outbuf.append(buffer, count);

		if (session->status == ISSL_HANDSHAKING)
		{
			// The handshake isn't finished, try to finish it.
			if (session->rstat == ISSL_WRITE || session->wstat == ISSL_WRITE)
			{
				ServerInstance->Log(DEBUG,"Handshake resume");
				Handshake(session);
			}
		}

		if (session->status == ISSL_OPEN)
		{
			if (session->rstat == ISSL_WRITE)
			{
				ServerInstance->Log(DEBUG,"DoRead");
				DoRead(session);
			}

			if (session->wstat == ISSL_WRITE)
			{
				ServerInstance->Log(DEBUG,"DoWrite");
				return DoWrite(session);
			}
		}

		return 1;
	}

	int DoWrite(issl_session* session)
	{
		if (!session->outbuf.size())
			return -1;

		int ret = SSL_write(session->sess, session->outbuf.data(), session->outbuf.size());

		if (ret == 0)
		{
			ServerInstance->Log(DEBUG,"Oops, got 0 from SSL_write");
			CloseSession(session);
			return 0;
		}
		else if (ret < 0)
		{
			int err = SSL_get_error(session->sess, ret);

			if (err == SSL_ERROR_WANT_WRITE)
			{
				session->wstat = ISSL_WRITE;
				return -1;
			}
			else if (err == SSL_ERROR_WANT_READ)
			{
				session->wstat = ISSL_READ;
				return -1;
			}
			else
			{
				ServerInstance->Log(DEBUG,"Close due to returned -1 in SSL_Write");
				CloseSession(session);
				return 0;
			}
		}
		else
		{
			session->outbuf = session->outbuf.substr(ret);
			return ret;
		}
	}

	int DoRead(issl_session* session)
	{
		// Is this right? Not sure if the unencrypted data is garaunteed to be the same length.
		// Read into the inbuffer, offset from the beginning by the amount of data we have that insp hasn't taken yet.
		
		ServerInstance->Log(DEBUG,"DoRead");

		int ret = SSL_read(session->sess, session->inbuf + session->inbufoffset, inbufsize - session->inbufoffset);

		if (ret == 0)
		{
			// Client closed connection.
			ServerInstance->Log(DEBUG,"Oops, got 0 from SSL_read");
			CloseSession(session);
			return 0;
		}
		else if (ret < 0)
		{
			int err = SSL_get_error(session->sess, ret);

			if (err == SSL_ERROR_WANT_READ)
			{
				session->rstat = ISSL_READ;
				ServerInstance->Log(DEBUG,"Setting want_read");
				return -1;
			}
			else if (err == SSL_ERROR_WANT_WRITE)
			{
				session->rstat = ISSL_WRITE;
				ServerInstance->Log(DEBUG,"Setting want_write");
				return -1;
			}
			else
			{
				ServerInstance->Log(DEBUG,"Closed due to returned -1 in SSL_Read");
				CloseSession(session);
				return 0;
			}
		}
		else
		{
			// Read successfully 'ret' bytes into inbuf + inbufoffset
			// There are 'ret' + 'inbufoffset' bytes of data in 'inbuf'
			// 'buffer' is 'count' long

			session->inbufoffset += ret;

			return ret;
		}
	}

	// :kenny.chatspike.net 320 Om Epy|AFK :is a Secure Connection
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		if (!clientactive)
			return;

		// Bugfix, only send this numeric for *our* SSL users
		if (dest->GetExt("ssl", dummy) || (IS_LOCAL(dest) &&  isin(dest->GetPort(), listenports)))
		{
			ServerInstance->SendWhoisLine(source, dest, 320, "%s %s :is using a secure connection", source->nick, dest->nick);
		}
	}

	virtual void OnSyncUserMetaData(userrec* user, Module* proto, void* opaque, const std::string &extname, bool displayable)
	{
		// check if the linking module wants to know about OUR metadata
		if (extname == "ssl")
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
		ServerInstance->Log(DEBUG,"Handshake");
		int ret;

		if (session->outbound)
		{
			ServerInstance->Log(DEBUG,"SSL_connect");
			ret = SSL_connect(session->sess);
		}
		else
			ret = SSL_accept(session->sess);

		if (ret < 0)
		{
			int err = SSL_get_error(session->sess, ret);

			if (err == SSL_ERROR_WANT_READ)
			{
				ServerInstance->Log(DEBUG,"Want read, handshaking");
				session->rstat = ISSL_READ;
				session->status = ISSL_HANDSHAKING;
				return true;
			}
			else if (err == SSL_ERROR_WANT_WRITE)
			{
				ServerInstance->Log(DEBUG,"Want write, handshaking");
				session->wstat = ISSL_WRITE;
				session->status = ISSL_HANDSHAKING;
				MakePollWrite(session);
				return true;
			}
			else
			{
				ServerInstance->Log(DEBUG,"Handshake failed");
				CloseSession(session);
			}

			return false;
		}
		else if (ret > 0)
		{
			// Handshake complete.
			// This will do for setting the ssl flag...it could be done earlier if it's needed. But this seems neater.
			userrec* u = ServerInstance->FindDescriptor(session->fd);
			if (u)
			{
				if (!u->GetExt("ssl", dummy))
					u->Extend("ssl", "ON");
			}

			session->status = ISSL_OPEN;

			MakePollWrite(session);

			return true;
		}
		else if (ret == 0)
		{
			int ssl_err = SSL_get_error(session->sess, ret);
			char buf[1024];
			ERR_print_errors_fp(stderr);
			ServerInstance->Log(DEBUG,"Handshake fail 2: %d: %s", ssl_err, ERR_error_string(ssl_err,buf));
			CloseSession(session);
			return true;
		}

		return true;
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

			VerifyCertificate(&sessions[user->GetFd()], user);
			if (sessions[user->GetFd()].sess)
				user->WriteServ("NOTICE %s :*** You are connected using SSL cipher \"%s\"", user->nick, SSL_get_cipher(sessions[user->GetFd()].sess));
		}
	}

	void MakePollWrite(issl_session* session)
	{
		OnRawSocketWrite(session->fd, NULL, 0);
		//EventHandler* eh = ServerInstance->FindDescriptor(session->fd);
		//if (eh)
		//	ServerInstance->SE->WantWrite(eh);
	}

	void CloseSession(issl_session* session)
	{
		if (session->sess)
		{
			SSL_shutdown(session->sess);
			SSL_free(session->sess);
		}

		if (session->inbuf)
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

		X509* cert;
		ssl_cert* certinfo = new ssl_cert;
		unsigned int n;
		unsigned char md[EVP_MAX_MD_SIZE];
		const EVP_MD *digest = EVP_md5();

		user->Extend("ssl_cert",certinfo);

		cert = SSL_get_peer_certificate((SSL*)session->sess);

		if (!cert)
		{
			certinfo->data.insert(std::make_pair("error","Could not get peer certificate: "+std::string(get_error())));
			return;
		}

		certinfo->data.insert(std::make_pair("invalid", SSL_get_verify_result(session->sess) != X509_V_OK ? ConvToStr(1) : ConvToStr(0)));

		if (SelfSigned)
		{
			certinfo->data.insert(std::make_pair("unknownsigner",ConvToStr(0)));
			certinfo->data.insert(std::make_pair("trusted",ConvToStr(1)));
		}
		else
		{
			certinfo->data.insert(std::make_pair("unknownsigner",ConvToStr(1)));
			certinfo->data.insert(std::make_pair("trusted",ConvToStr(0)));
		}

		certinfo->data.insert(std::make_pair("dn",std::string(X509_NAME_oneline(X509_get_subject_name(cert),0,0))));
		certinfo->data.insert(std::make_pair("issuer",std::string(X509_NAME_oneline(X509_get_issuer_name(cert),0,0))));

		if (!X509_digest(cert, digest, md, &n))
		{
			certinfo->data.insert(std::make_pair("error","Out of memory generating fingerprint"));
		}
		else
		{
			certinfo->data.insert(std::make_pair("fingerprint",irc::hex(md, n)));
		}

		if ((ASN1_UTCTIME_cmp_time_t(X509_get_notAfter(cert), time(NULL)) == -1) || (ASN1_UTCTIME_cmp_time_t(X509_get_notBefore(cert), time(NULL)) == 0))
		{
			certinfo->data.insert(std::make_pair("error","Not activated, or expired certificate"));
		}

		X509_free(cert);
	}
};

static int error_callback(const char *str, size_t len, void *u)
{
	ModuleSSLOpenSSL* mssl = (ModuleSSLOpenSSL*)u;
	mssl->PublicInstance->Log(DEFAULT, "SSL error: " + std::string(str, len - 1));
	return 0;
}

MODULE_INIT(ModuleSSLOpenSSL);

