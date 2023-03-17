/*
 *
 * (C) 2010-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

/* RequiredLibraries: ssl,crypto */
/* RequiredWindowsLibraries: ssleay32,libeay32 */

#include "module.h"
#include "modules/ssl.h"

#define OPENSSL_NO_SHA512
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>

static SSL_CTX *server_ctx, *client_ctx;

class MySSLService : public SSLService {
  public:
    MySSLService(Module *o, const Anope::string &n);

    /** Initialize a socket to use SSL
     * @param s The socket
     */
    void Init(Socket *s) anope_override;
};

class SSLSocketIO : public SocketIO {
  public:
    /* The SSL socket for this socket */
    SSL *sslsock;

    /** Constructor
     */
    SSLSocketIO();

    /** Really receive something from the buffer
     * @param s The socket
     * @param buf The buf to read to
     * @param sz How much to read
     * @return Number of bytes received
     */
    int Recv(Socket *s, char *buf, size_t sz) anope_override;

    /** Write something to the socket
     * @param s The socket
     * @param buf The data to write
     * @param size The length of the data
     */
    int Send(Socket *s, const char *buf, size_t sz) anope_override;

    /** Accept a connection from a socket
     * @param s The socket
     * @return The new socket
     */
    ClientSocket *Accept(ListenSocket *s) anope_override;

    /** Finished accepting a connection from a socket
     * @param s The socket
     * @return SF_ACCEPTED if accepted, SF_ACCEPTING if still in process, SF_DEAD on error
     */
    SocketFlag FinishAccept(ClientSocket *cs) anope_override;

    /** Connect the socket
     * @param s THe socket
     * @param target IP to connect to
     * @param port to connect to
     */
    void Connect(ConnectionSocket *s, const Anope::string &target,
                 int port) anope_override;

    /** Called to potentially finish a pending connection
     * @param s The socket
     * @return SF_CONNECTED on success, SF_CONNECTING if still pending, and SF_DEAD on error.
     */
    SocketFlag FinishConnect(ConnectionSocket *s) anope_override;

    /** Called when the socket is destructing
     */
    void Destroy() anope_override;
};

class SSLModule;
static SSLModule *me;
class SSLModule : public Module {
    Anope::string certfile, keyfile;

  public:
    MySSLService service;

    SSLModule(const Anope::string &modname,
              const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR),
        service(this, "ssl") {
        me = this;

        this->SetPermanent(true);

        SSL_library_init();
        SSL_load_error_strings();

        client_ctx = SSL_CTX_new(SSLv23_client_method());
        server_ctx = SSL_CTX_new(SSLv23_server_method());

        if (!client_ctx || !server_ctx) {
            throw ModuleException("Error initializing SSL CTX");
        }

        long opts = SSL_OP_NO_SSLv2 | SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
                    SSL_OP_CIPHER_SERVER_PREFERENCE;
        SSL_CTX_set_options(client_ctx, opts);
        SSL_CTX_set_options(server_ctx, opts);

        SSL_CTX_set_mode(client_ctx,
                         SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
        SSL_CTX_set_mode(server_ctx,
                         SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

        Anope::string context_name = "Anope";
        SSL_CTX_set_session_id_context(client_ctx,
                                       reinterpret_cast<const unsigned char *>(context_name.c_str()),
                                       context_name.length());
        SSL_CTX_set_session_id_context(server_ctx,
                                       reinterpret_cast<const unsigned char *>(context_name.c_str()),
                                       context_name.length());
    }

    ~SSLModule() {
        for (std::map<int, Socket *>::const_iterator it = SocketEngine::Sockets.begin(),
                it_end = SocketEngine::Sockets.end(); it != it_end;) {
            Socket *s = it->second;
            ++it;

            if (dynamic_cast<SSLSocketIO *>(s->io)) {
                delete s;
            }
        }

        SSL_CTX_free(client_ctx);
        SSL_CTX_free(server_ctx);
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *config = conf->GetModule(this);

        this->certfile = config->Get<const Anope::string>("cert", "data/anope.crt");
        this->keyfile = config->Get<const Anope::string>("key", "data/anope.key");

        if (Anope::IsFile(this->certfile.c_str())) {
            if (!SSL_CTX_use_certificate_chain_file(client_ctx, this->certfile.c_str())
                    || !SSL_CTX_use_certificate_chain_file(server_ctx, this->certfile.c_str())) {
                throw ConfigException("Error loading certificate");
            } else {
                Log(LOG_DEBUG) << "m_ssl_openssl: Successfully loaded certificate " <<
                               this->certfile;
            }
        } else {
            Log() << "Unable to open certificate " << this->certfile;
        }

        if (Anope::IsFile(this->keyfile.c_str())) {
            if (!SSL_CTX_use_PrivateKey_file(client_ctx, this->keyfile.c_str(),
                                             SSL_FILETYPE_PEM)
                    || !SSL_CTX_use_PrivateKey_file(server_ctx, this->keyfile.c_str(),
                                                    SSL_FILETYPE_PEM)) {
                throw ConfigException("Error loading private key");
            } else {
                Log(LOG_DEBUG) << "m_ssl_openssl: Successfully loaded private key " <<
                               this->keyfile;
            }
        } else {
            if (Anope::IsFile(this->certfile.c_str())) {
                throw ConfigException("Error loading private key " + this->keyfile +
                                      " - file not found");
            } else {
                Log() << "Unable to open private key " << this->keyfile;
            }
        }

        // Allow disabling SSLv3
        if (!config->Get<Anope::string>("sslv3").empty()) {
            if (config->Get<bool>("sslv3")) {
                SSL_CTX_clear_options(client_ctx, SSL_OP_NO_SSLv3);
                SSL_CTX_clear_options(server_ctx, SSL_OP_NO_SSLv3);
            } else {
                SSL_CTX_set_options(client_ctx, SSL_OP_NO_SSLv3);
                SSL_CTX_set_options(server_ctx, SSL_OP_NO_SSLv3);
            }
        }
    }

    void OnPreServerConnect() anope_override {
        Configuration::Block *config = Config->GetBlock("uplink", Anope::CurrentUplink);

        if (config->Get<bool>("ssl")) {
            this->service.Init(UplinkSock);
        }
    }
};

MySSLService::MySSLService(Module *o, const Anope::string &n) : SSLService(o,
            n) {
}

void MySSLService::Init(Socket *s) {
    if (s->io != &NormalSocketIO) {
        throw CoreException("Socket initializing SSL twice");
    }

    s->io = new SSLSocketIO();
}

SSLSocketIO::SSLSocketIO() {
    this->sslsock = NULL;
}

int SSLSocketIO::Recv(Socket *s, char *buf, size_t sz) {
    int i = SSL_read(this->sslsock, buf, sz);
    if (i > 0) {
        TotalRead += i;
    } else if (i < 0) {
        int err = SSL_get_error(this->sslsock, i);
        switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            SocketEngine::SetLastError(EAGAIN);
        }
    }

    return i;
}

int SSLSocketIO::Send(Socket *s, const char *buf, size_t sz) {
    int i = SSL_write(this->sslsock, buf, sz);
    if (i > 0) {
        TotalWritten += i;
    } else if (i < 0) {
        int err = SSL_get_error(this->sslsock, i);
        switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            SocketEngine::SetLastError(EAGAIN);
        }
    }
    return i;
}

ClientSocket *SSLSocketIO::Accept(ListenSocket *s) {
    if (s->io == &NormalSocketIO) {
        throw SocketException("Attempting to accept on uninitialized socket with SSL");
    }

    sockaddrs conaddr;

    socklen_t size = sizeof(conaddr);
    int newsock = accept(s->GetFD(), &conaddr.sa, &size);

#ifndef INVALID_SOCKET
    const int INVALID_SOCKET = -1;
#endif

    if (newsock < 0 || newsock == INVALID_SOCKET) {
        throw SocketException("Unable to accept connection: " + Anope::LastError());
    }

    ClientSocket *newsocket = s->OnAccept(newsock, conaddr);
    me->service.Init(newsocket);
    SSLSocketIO *io = anope_dynamic_static_cast<SSLSocketIO *>(newsocket->io);

    io->sslsock = SSL_new(server_ctx);
    if (!io->sslsock) {
        throw SocketException("Unable to initialize SSL socket");
    }

    SSL_set_accept_state(io->sslsock);

    if (!SSL_set_fd(io->sslsock, newsocket->GetFD())) {
        throw SocketException("Unable to set SSL fd");
    }

    newsocket->flags[SF_ACCEPTING] = true;
    this->FinishAccept(newsocket);

    return newsocket;
}

SocketFlag SSLSocketIO::FinishAccept(ClientSocket *cs) {
    if (cs->io == &NormalSocketIO) {
        throw SocketException("Attempting to finish connect uninitialized socket with SSL");
    } else if (cs->flags[SF_ACCEPTED]) {
        return SF_ACCEPTED;
    } else if (!cs->flags[SF_ACCEPTING]) {
        throw SocketException("SSLSocketIO::FinishAccept called for a socket not accepted nor accepting?");
    }

    SSLSocketIO *io = anope_dynamic_static_cast<SSLSocketIO *>(cs->io);

    int ret = SSL_accept(io->sslsock);
    if (ret <= 0) {
        int error = SSL_get_error(io->sslsock, ret);
        if (ret == -1 && (error == SSL_ERROR_WANT_READ
                          || error == SSL_ERROR_WANT_WRITE)) {
            SocketEngine::Change(cs, error == SSL_ERROR_WANT_WRITE, SF_WRITABLE);
            SocketEngine::Change(cs, error == SSL_ERROR_WANT_READ, SF_READABLE);
            return SF_ACCEPTING;
        } else {
            cs->OnError(ERR_error_string(ERR_get_error(), NULL));
            cs->flags[SF_DEAD] = true;
            cs->flags[SF_ACCEPTING] = false;
            return SF_DEAD;
        }
    } else {
        cs->flags[SF_ACCEPTED] = true;
        cs->flags[SF_ACCEPTING] = false;
        SocketEngine::Change(cs, false, SF_WRITABLE);
        SocketEngine::Change(cs, true, SF_READABLE);
        cs->OnAccept();
        return SF_ACCEPTED;
    }
}

void SSLSocketIO::Connect(ConnectionSocket *s, const Anope::string &target,
                          int port) {
    if (s->io == &NormalSocketIO) {
        throw SocketException("Attempting to connect uninitialized socket with SSL");
    }

    s->flags[SF_CONNECTING] = s->flags[SF_CONNECTED] = false;

    s->conaddr.pton(s->IsIPv6() ? AF_INET6 : AF_INET, target, port);
    int c = connect(s->GetFD(), &s->conaddr.sa, s->conaddr.size());
    if (c == -1) {
        if (Anope::LastErrorCode() != EINPROGRESS) {
            s->OnError(Anope::LastError());
            s->flags[SF_DEAD] = true;
            return;
        } else {
            SocketEngine::Change(s, true, SF_WRITABLE);
            s->flags[SF_CONNECTING] = true;
            return;
        }
    } else {
        s->flags[SF_CONNECTING] = true;
        this->FinishConnect(s);
    }
}

SocketFlag SSLSocketIO::FinishConnect(ConnectionSocket *s) {
    if (s->io == &NormalSocketIO) {
        throw SocketException("Attempting to finish connect uninitialized socket with SSL");
    } else if (s->flags[SF_CONNECTED]) {
        return SF_CONNECTED;
    } else if (!s->flags[SF_CONNECTING]) {
        throw SocketException("SSLSocketIO::FinishConnect called for a socket not connected nor connecting?");
    }

    SSLSocketIO *io = anope_dynamic_static_cast<SSLSocketIO *>(s->io);

    if (io->sslsock == NULL) {
        io->sslsock = SSL_new(client_ctx);
        if (!io->sslsock) {
            throw SocketException("Unable to initialize SSL socket");
        }

        if (!SSL_set_fd(io->sslsock, s->GetFD())) {
            throw SocketException("Unable to set SSL fd");
        }
    }

    int ret = SSL_connect(io->sslsock);
    if (ret <= 0) {
        int error = SSL_get_error(io->sslsock, ret);
        if (ret == -1 && (error == SSL_ERROR_WANT_READ
                          || error == SSL_ERROR_WANT_WRITE)) {
            SocketEngine::Change(s, error == SSL_ERROR_WANT_WRITE, SF_WRITABLE);
            SocketEngine::Change(s, error == SSL_ERROR_WANT_READ, SF_READABLE);
            return SF_CONNECTING;
        } else {
            s->OnError(ERR_error_string(ERR_get_error(), NULL));
            s->flags[SF_CONNECTING] = false;
            s->flags[SF_DEAD] = true;
            return SF_DEAD;
        }
    } else {
        s->flags[SF_CONNECTING] = false;
        s->flags[SF_CONNECTED] = true;
        SocketEngine::Change(s, false, SF_WRITABLE);
        SocketEngine::Change(s, true, SF_READABLE);
        s->OnConnect();
        return SF_CONNECTED;
    }
}

void SSLSocketIO::Destroy() {
    if (this->sslsock) {
        SSL_shutdown(this->sslsock);
        SSL_free(this->sslsock);
    }

    delete this;
}

MODULE_INIT(SSLModule)
