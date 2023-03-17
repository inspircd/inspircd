/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017 Wade Cline <wadecline@hotmail.com>
 *   Copyright (C) 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2014 Julien Vehent <julien@linuxwall.info>
 *   Copyright (C) 2013-2014, 2016-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2017 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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

/// $CompilerFlags: find_compiler_flags("openssl" "")
/// $LinkerFlags: find_linker_flags("openssl" "-lssl -lcrypto")

/// $PackageInfo: require_system("arch") openssl pkgconf
/// $PackageInfo: require_system("centos") openssl-devel pkgconfig
/// $PackageInfo: require_system("darwin") openssl pkg-config
/// $PackageInfo: require_system("debian") libssl-dev openssl pkg-config
/// $PackageInfo: require_system("ubuntu") libssl-dev openssl pkg-config


#include "inspircd.h"
#include "iohook.h"
#include "modules/ssl.h"

#ifdef __GNUC__
# pragma GCC diagnostic push
#endif

// Ignore OpenSSL deprecation warnings on OS X Lion and newer.
#if defined __APPLE__
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// LibreSSL lies about the version of OpenSSL it is compatible with and is a general pain
// to support. Support for it was removed in the master branch at the same time that
// support for OpenSSL pre-1.1 was.
#if defined __GNUC__ && defined LIBRESSL_VERSION_NUMBER
# undef OPENSSL_VERSION_NUMBER
# define OPENSSL_VERSION_NUMBER 0x10000000L
# warning LibreSSL support will be removed in v4. Consider using the ssl_gnutls or ssl_mbedtls modules instead if you can not use OpenSSL.
#endif

// Fix warnings about the use of `long long` on C++03.
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-long-long"
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wlong-long"
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dh.h>

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

#ifdef _WIN32
# pragma comment(lib, "libcrypto.lib")
# pragma comment(lib, "libssl.lib")
#endif

// Compatibility layer to allow OpenSSL 1.0 to use the 1.1 API.
#if OPENSSL_VERSION_NUMBER < 0x10100000L

// BIO is opaque in OpenSSL 1.1 but the access API does not exist in 1.0.
# define BIO_get_data(BIO) BIO->ptr
# define BIO_set_data(BIO, VALUE) BIO->ptr = VALUE;
# define BIO_set_init(BIO, VALUE) BIO->init = VALUE;

// These functions have been renamed in OpenSSL 1.1.
# define OpenSSL_version SSLeay_version
# define X509_getm_notAfter X509_get_notAfter
# define X509_getm_notBefore X509_get_notBefore
# define OPENSSL_init_ssl(OPTIONS, SETTINGS) \
    SSL_library_init(); \
    SSL_load_error_strings();

// These macros have been renamed in OpenSSL 1.1.
# define OPENSSL_VERSION SSLEAY_VERSION

#else
# define INSPIRCD_OPENSSL_OPAQUE_BIO
# if OPENSSL_VERSION_NUMBER >= 0x30000000L
#  define INSPIRCD_OPENSSL_AUTO_DH
# endif
#endif

static bool SelfSigned = false;
static int exdataindex;

char* get_error() {
    return ERR_error_string(ERR_get_error(), NULL);
}

static int OnVerify(int preverify_ok, X509_STORE_CTX* ctx);
static void StaticSSLInfoCallback(const SSL* ssl, int where, int rc);

namespace OpenSSL {
class Exception : public ModuleException {
  public:
    Exception(const std::string& reason)
        : ModuleException(reason) { }
};

#ifndef INSPIRCD_OPENSSL_AUTO_DH
class DHParams {
    DH* dh;

  public:
    DHParams(const std::string& filename) {
        BIO* dhpfile = BIO_new_file(filename.c_str(), "r");
        if (dhpfile == NULL) {
            throw Exception("Couldn't open DH file " + filename);
        }

        dh = PEM_read_bio_DHparams(dhpfile, NULL, NULL, NULL);
        BIO_free(dhpfile);

        if (!dh) {
            throw Exception("Couldn't read DH params from file " + filename);
        }
    }

    ~DHParams() {
        DH_free(dh);
    }

    DH* get() {
        return dh;
    }
};
#endif

class Context {
    SSL_CTX* const ctx;
    long ctx_options;

  public:
    Context(SSL_CTX* context)
        : ctx(context) {
        // Sane default options for OpenSSL see https://www.openssl.org/docs/ssl/SSL_CTX_set_options.html
        // and when choosing a cipher, use the server's preferences instead of the client preferences.
        long opts = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION | SSL_OP_CIPHER_SERVER_PREFERENCE
                    | SSL_OP_SINGLE_DH_USE;
        // Only turn options on if they exist
#ifdef SSL_OP_SINGLE_ECDH_USE
        opts |= SSL_OP_SINGLE_ECDH_USE;
#endif
#ifdef SSL_OP_NO_TICKET
        opts |= SSL_OP_NO_TICKET;
#endif

        ctx_options = SSL_CTX_set_options(ctx, opts);

        long mode = SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER;
#ifdef SSL_MODE_RELEASE_BUFFERS
        mode |= SSL_MODE_RELEASE_BUFFERS;
#endif
        SSL_CTX_set_mode(ctx, mode);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
        SSL_CTX_set_info_callback(ctx, StaticSSLInfoCallback);
    }

    ~Context() {
        SSL_CTX_free(ctx);
    }

#ifndef INSPIRCD_OPENSSL_AUTO_DH
    bool SetDH(DHParams& dh) {
        ERR_clear_error();
        return (SSL_CTX_set_tmp_dh(ctx, dh.get()) >= 0);
    }
#endif

#ifndef OPENSSL_NO_ECDH
    void SetECDH(const std::string& curvename) {
        int nid = OBJ_sn2nid(curvename.c_str());
        if (nid == NID_undef) {
            throw Exception("Unknown curve: " + curvename);
        }

# if OPENSSL_VERSION_NUMBER >= 0x10101000L
        ERR_clear_error();
        if (!SSL_CTX_set1_groups(ctx, &nid, 1)) {
            throw Exception("Couldn't set ECDH curve");
        }
# else
        EC_KEY* eckey = EC_KEY_new_by_curve_name(nid);
        if (!eckey) {
            throw Exception("Unable to create EC key object");
        }

        ERR_clear_error();
        bool ret = (SSL_CTX_set_tmp_ecdh(ctx, eckey) >= 0);
        EC_KEY_free(eckey);
        if (!ret) {
            throw Exception("Couldn't set ECDH parameters");
        }
# endif
    }
#endif

    bool SetCiphers(const std::string& ciphers) {
        // TLSv1 to TLSv1.2 ciphers.
        ERR_clear_error();
        return SSL_CTX_set_cipher_list(ctx, ciphers.c_str());
    }

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    bool SetCiphersuites(const std::string& ciphers) {
        // TLSv1.3+ ciphers.
        ERR_clear_error();
        return SSL_CTX_set_ciphersuites(ctx, ciphers.c_str());
    }
#endif

    bool SetCerts(const std::string& filename) {
        ERR_clear_error();
        return SSL_CTX_use_certificate_chain_file(ctx, filename.c_str());
    }

    bool SetPrivateKey(const std::string& filename) {
        ERR_clear_error();
        return SSL_CTX_use_PrivateKey_file(ctx, filename.c_str(), SSL_FILETYPE_PEM);
    }

    bool SetCA(const std::string& filename) {
        ERR_clear_error();
        return SSL_CTX_load_verify_locations(ctx, filename.c_str(), 0);
    }

    void SetCRL(const std::string& crlfile, const std::string& crlpath,
                const std::string& crlmode) {
        if (crlfile.empty() && crlpath.empty()) {
            return;
        }

        /* Set CRL mode */
        unsigned long crlflags = X509_V_FLAG_CRL_CHECK;
        if (stdalgo::string::equalsci(crlmode, "chain")) {
            crlflags |= X509_V_FLAG_CRL_CHECK_ALL;
        } else if (!stdalgo::string::equalsci(crlmode, "leaf")) {
            throw ModuleException("Unknown mode '" + crlmode +
                                  "'; expected either 'chain' (default) or 'leaf'");
        }

        /* Load CRL files */
        X509_STORE* store = SSL_CTX_get_cert_store(ctx);
        if (!store) {
            throw ModuleException("Unable to get X509_STORE from TLS (SSL) context; this should never happen");
        }
        ERR_clear_error();
        if (!X509_STORE_load_locations(store,
                                       crlfile.empty() ? NULL : crlfile.c_str(),
                                       crlpath.empty() ? NULL : crlpath.c_str())) {
            int err = ERR_get_error();
            throw ModuleException("Unable to load CRL file '" + crlfile + "' or CRL path '"
                                  + crlpath + "': '" + (err ? ERR_error_string(err, NULL) : "unknown") + "'");
        }

        /* Set CRL mode */
        if (X509_STORE_set_flags(store, crlflags) != 1) {
            throw ModuleException("Unable to set X509 CRL flags");
        }
    }


    long GetDefaultContextOptions() const {
        return ctx_options;
    }

    long SetRawContextOptions(long setoptions, long clearoptions) {
        // Clear everything
        SSL_CTX_clear_options(ctx, SSL_CTX_get_options(ctx));

        // Set the default options and what is in the conf
        SSL_CTX_set_options(ctx, ctx_options | setoptions);
        return SSL_CTX_clear_options(ctx, clearoptions);
    }

    void SetVerifyCert() {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, OnVerify);
    }

    SSL* CreateServerSession() {
        SSL* sess = SSL_new(ctx);
        SSL_set_accept_state(sess); // Act as server
        return sess;
    }

    SSL* CreateClientSession() {
        SSL* sess = SSL_new(ctx);
        SSL_set_connect_state(sess); // Act as client
        return sess;
    }
};

class Profile {
    /** Name of this profile
     */
    const std::string name;

#ifndef INSPIRCD_OPENSSL_AUTO_DH
    /** DH parameters in use
     */
    DHParams dh;
#endif

    /** OpenSSL makes us have two contexts, one for servers and one for clients
     */
    Context ctx;
    Context clientctx;

    /** Digest to use when generating fingerprints
     */
    const EVP_MD* digest;

    /** Last error, set by error_callback()
     */
    std::string lasterr;

    /** True if renegotiations are allowed, false if not
     */
    const bool allowrenego;

    /** Rough max size of records to send
     */
    const unsigned int outrecsize;

    static int error_callback(const char* str, size_t len, void* u) {
        Profile* profile = reinterpret_cast<Profile*>(u);
        profile->lasterr = std::string(str, len - 1);
        return 0;
    }

    /** Set raw OpenSSL context (SSL_CTX) options from a config tag
     * @param ctxname Name of the context, client or server
     * @param tag Config tag defining this profile
     * @param context Context object to manipulate
     */
    void SetContextOptions(const std::string& ctxname, ConfigTag* tag,
                           Context& context) {
        long setoptions = tag->getInt(ctxname + "setoptions", 0);
        long clearoptions = tag->getInt(ctxname + "clearoptions", 0);

#ifdef SSL_OP_NO_COMPRESSION
        // Disable compression by default
        if (!tag->getBool("compression", false)) {
            setoptions |= SSL_OP_NO_COMPRESSION;
        }
#endif

        // Disable TLSv1.0 by default.
        if (!tag->getBool("tlsv1", false)) {
            setoptions |= SSL_OP_NO_TLSv1;
        }

#ifdef SSL_OP_NO_TLSv1_1
        // Enable TLSv1.1 by default.
        if (!tag->getBool("tlsv11", true)) {
            setoptions |= SSL_OP_NO_TLSv1_1;
        }
#endif

#ifdef SSL_OP_NO_TLSv1_2
        // Enable TLSv1.2 by default.
        if (!tag->getBool("tlsv12", true)) {
            setoptions |= SSL_OP_NO_TLSv1_2;
        }
#endif

#ifdef SSL_OP_NO_TLSv1_3
        // Enable TLSv1.3 by default.
        if (!tag->getBool("tlsv13", true)) {
            setoptions |= SSL_OP_NO_TLSv1_3;
        }
#endif

        if (!setoptions && !clearoptions) {
            return;    // Nothing to do
        }

        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                  "Setting %s %s context options, default: %ld set: %ld clear: %ld", name.c_str(),
                                  ctxname.c_str(), ctx.GetDefaultContextOptions(), setoptions, clearoptions);
        long final = context.SetRawContextOptions(setoptions, clearoptions);
        ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "%s %s context options: %ld",
                                  name.c_str(), ctxname.c_str(), final);
    }

  public:
    Profile(const std::string& profilename, ConfigTag* tag)
        : name(profilename)
#ifndef INSPIRCD_OPENSSL_AUTO_DH
        , dh(ServerInstance->Config->Paths.PrependConfig(tag->getString("dhfile",
                "dhparams.pem", 1)))
#endif
        , ctx(SSL_CTX_new(SSLv23_server_method()))
        , clientctx(SSL_CTX_new(SSLv23_client_method()))
        , allowrenego(tag->getBool("renegotiation")) // Disallow by default
        , outrecsize(tag->getUInt("outrecsize", 2048, 512, 16384)) {
#ifndef INSPIRCD_OPENSSL_AUTO_DH
        if ((!ctx.SetDH(dh)) || (!clientctx.SetDH(dh))) {
            throw Exception("Couldn't set DH parameters");
        }
#endif

        const std::string hash = tag->getString("hash", "md5", 1);
        digest = EVP_get_digestbyname(hash.c_str());
        if (digest == NULL) {
            throw Exception("Unknown hash type " + hash);
        }

        const std::string ciphers = tag->getString("ciphers");
        if (!ciphers.empty()) {
            if ((!ctx.SetCiphers(ciphers)) || (!clientctx.SetCiphers(ciphers))) {
                ERR_print_errors_cb(error_callback, this);
                throw Exception("Can't set cipher list to \"" + ciphers + "\" " + lasterr);
            }
        }

        const std::string ciphersuites = tag->getString("ciphersuites");
        if (!ciphersuites.empty()) {
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
            if ((!ctx.SetCiphersuites(ciphersuites))
                    || (!clientctx.SetCiphersuites(ciphersuites))) {
                ERR_print_errors_cb(error_callback, this);
                throw Exception("Can't set ciphersuite list to \"" + ciphersuites + "\" " +
                                lasterr);
            }
#else
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "You have configured <sslprofile:ciphersuites> but your version of OpenSSL does not support TLSv1.3+");
#endif
        }

#ifndef OPENSSL_NO_ECDH
        const std::string curvename = tag->getString("ecdhcurve", "prime256v1");
        if (!curvename.empty()) {
            ctx.SetECDH(curvename);
        }
#endif

        SetContextOptions("server", tag, ctx);
        SetContextOptions("client", tag, clientctx);

        /* Load our keys and certificates
         * NOTE: OpenSSL's error logging API sucks, don't blame us for this clusterfuck.
         */
        std::string filename = ServerInstance->Config->Paths.PrependConfig(
                                   tag->getString("certfile", "cert.pem", 1));
        if ((!ctx.SetCerts(filename)) || (!clientctx.SetCerts(filename))) {
            ERR_print_errors_cb(error_callback, this);
            throw Exception("Can't read certificate file: " + lasterr);
        }

        filename = ServerInstance->Config->Paths.PrependConfig(tag->getString("keyfile",
                   "key.pem", 1));
        if ((!ctx.SetPrivateKey(filename)) || (!clientctx.SetPrivateKey(filename))) {
            ERR_print_errors_cb(error_callback, this);
            throw Exception("Can't read key file: " + lasterr);
        }

        // Load the CAs we trust
        filename = ServerInstance->Config->Paths.PrependConfig(tag->getString("cafile",
                   "ca.pem", 1));
        if ((!ctx.SetCA(filename)) || (!clientctx.SetCA(filename))) {
            ERR_print_errors_cb(error_callback, this);
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                      "Can't read CA list from %s. This is only a problem if you want to verify client certificates, otherwise it's safe to ignore this message. Error: %s",
                                      filename.c_str(), lasterr.c_str());
        }

        // Load the CRLs.
        const std::string crlfile = tag->getString("crlfile");
        const std::string crlpath = tag->getString("crlpath");
        const std::string crlmode = tag->getString("crlmode", "chain", 1);
        ctx.SetCRL(crlfile, crlpath, crlmode);

        clientctx.SetVerifyCert();
        if (tag->getBool("requestclientcert", true)) {
            ctx.SetVerifyCert();
        }
    }

    const std::string& GetName() const {
        return name;
    }
    SSL* CreateServerSession() {
        return ctx.CreateServerSession();
    }
    SSL* CreateClientSession() {
        return clientctx.CreateClientSession();
    }
    const EVP_MD* GetDigest() {
        return digest;
    }
    bool AllowRenegotiation() const {
        return allowrenego;
    }
    unsigned int GetOutgoingRecordSize() const {
        return outrecsize;
    }
};

namespace BIOMethod {
static int create(BIO* bio) {
    BIO_set_init(bio, 1);
    return 1;
}

static int destroy(BIO* bio) {
    // XXX: Dummy function to avoid a memory leak in OpenSSL.
    // The memory leak happens in BIO_free() (bio_lib.c) when the destroy func of the BIO is NULL.
    // This is fixed in OpenSSL but some distros still ship the unpatched version hence we provide this workaround.
    return 1;
}

static long ctrl(BIO* bio, int cmd, long num, void* ptr) {
    if (cmd == BIO_CTRL_FLUSH) {
        return 1;
    }
    return 0;
}

static int read(BIO* bio, char* buf, int len);
static int write(BIO* bio, const char* buf, int len);

#ifdef INSPIRCD_OPENSSL_OPAQUE_BIO
static BIO_METHOD* alloc() {
    BIO_METHOD* meth = BIO_meth_new(100 | BIO_TYPE_SOURCE_SINK, "inspircd");
    BIO_meth_set_write(meth, OpenSSL::BIOMethod::write);
    BIO_meth_set_read(meth, OpenSSL::BIOMethod::read);
    BIO_meth_set_ctrl(meth, OpenSSL::BIOMethod::ctrl);
    BIO_meth_set_create(meth, OpenSSL::BIOMethod::create);
    BIO_meth_set_destroy(meth, OpenSSL::BIOMethod::destroy);
    return meth;
}
#endif
}
}

// BIO_METHOD is opaque in OpenSSL 1.1 so we can't do this.
// See OpenSSL::BIOMethod::alloc for the new method.
#ifndef INSPIRCD_OPENSSL_OPAQUE_BIO
static BIO_METHOD biomethods = {
    (100 | BIO_TYPE_SOURCE_SINK),
    "inspircd",
    OpenSSL::BIOMethod::write,
    OpenSSL::BIOMethod::read,
    NULL, // puts
    NULL, // gets
    OpenSSL::BIOMethod::ctrl,
    OpenSSL::BIOMethod::create,
    OpenSSL::BIOMethod::destroy, // destroy, does nothing, see function body for more info
    NULL // callback_ctrl
};
#else
static BIO_METHOD* biomethods;
#endif

static int OnVerify(int preverify_ok, X509_STORE_CTX *ctx) {
    /* XXX: This will allow self signed certificates.
     * In the future if we want an option to not allow this,
     * we can just return preverify_ok here, and openssl
     * will boot off self-signed and invalid peer certs.
     */
    int ve = X509_STORE_CTX_get_error(ctx);

    SelfSigned = (ve == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT);

    return 1;
}

class OpenSSLIOHook : public SSLIOHook {
  private:
    SSL* sess;
    bool data_to_write;

    // Returns 1 if handshake succeeded, 0 if it is still in progress, -1 if it failed
    int Handshake(StreamSocket* user) {
        ERR_clear_error();
        int ret = SSL_do_handshake(sess);
        if (ret < 0) {
            int err = SSL_get_error(sess, ret);

            if (err == SSL_ERROR_WANT_READ) {
                SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
                this->status = STATUS_HANDSHAKING;
                return 0;
            } else if (err == SSL_ERROR_WANT_WRITE) {
                SocketEngine::ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
                this->status = STATUS_HANDSHAKING;
                return 0;
            } else {
                CloseSession();
                return -1;
            }
        } else if (ret > 0) {
            // Handshake complete.
            VerifyCertificate();

            status = STATUS_OPEN;

            SocketEngine::ChangeEventMask(user,
                                          FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

            return 1;
        } else if (ret == 0) {
            CloseSession();
        }
        return -1;
    }

    void CloseSession() {
        if (sess) {
            SSL_shutdown(sess);
            SSL_free(sess);
        }
        sess = NULL;
        certificate = NULL;
        status = STATUS_NONE;
    }

    void VerifyCertificate() {
        X509* cert;
        ssl_cert* certinfo = new ssl_cert;
        this->certificate = certinfo;
        unsigned int n;
        unsigned char md[EVP_MAX_MD_SIZE];

        cert = SSL_get_peer_certificate(sess);

        if (!cert) {
            certinfo->error = "Could not get peer certificate: "+std::string(get_error());
            return;
        }

        certinfo->invalid = (SSL_get_verify_result(sess) != X509_V_OK);

        if (!SelfSigned) {
            certinfo->unknownsigner = false;
            certinfo->trusted = true;
        } else {
            certinfo->unknownsigner = true;
            certinfo->trusted = false;
        }

        GetDNString(X509_get_subject_name(cert), certinfo->dn);
        GetDNString(X509_get_issuer_name(cert), certinfo->issuer);

        if (!X509_digest(cert, GetProfile().GetDigest(), md, &n)) {
            certinfo->error = "Out of memory generating fingerprint";
        } else {
            certinfo->fingerprint = BinToHex(md, n);
        }

        if ((ASN1_UTCTIME_cmp_time_t(X509_getm_notAfter(cert),
                                     ServerInstance->Time()) == -1)
                || (ASN1_UTCTIME_cmp_time_t(X509_getm_notBefore(cert),
                                            ServerInstance->Time()) == 0)) {
            certinfo->error = "Not activated, or expired certificate";
        }

        X509_free(cert);
    }

    static void GetDNString(X509_NAME* x509name, std::string& out) {
        char buf[512];
        X509_NAME_oneline(x509name, buf, sizeof(buf));

        out.assign(buf);
        for (size_t pos = 0;
                ((pos = out.find_first_of("\r\n", pos)) != std::string::npos); ) {
            out[pos] = ' ';
        }
    }

    void SSLInfoCallback(int where, int rc) {
        if ((where & SSL_CB_HANDSHAKE_START) && (status == STATUS_OPEN)) {
            if (GetProfile().AllowRenegotiation()) {
                return;
            }

            // The other side is trying to renegotiate, kill the connection and change status
            // to STATUS_NONE so CheckRenego() closes the session
            status = STATUS_NONE;
            BIO* bio = SSL_get_rbio(sess);
            EventHandler* eh = static_cast<StreamSocket*>(BIO_get_data(bio));
            SocketEngine::Shutdown(eh, 2);
        }
    }

    bool CheckRenego(StreamSocket* sock) {
        if (status != STATUS_NONE) {
            return true;
        }

        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                  "Session %p killed, attempted to renegotiate", (void*)sess);
        CloseSession();
        sock->SetError("Renegotiation is not allowed");
        return false;
    }

    // Returns 1 if application I/O should proceed, 0 if it must wait for the underlying protocol to progress, -1 on fatal error
    int PrepareIO(StreamSocket* sock) {
        if (status == STATUS_OPEN) {
            return 1;
        } else if (status == STATUS_HANDSHAKING) {
            // The handshake isn't finished, try to finish it
            return Handshake(sock);
        }

        CloseSession();
        return -1;
    }

    // Calls our private SSLInfoCallback()
    friend void StaticSSLInfoCallback(const SSL* ssl, int where, int rc);

  public:
    OpenSSLIOHook(IOHookProvider* hookprov, StreamSocket* sock, SSL* session)
        : SSLIOHook(hookprov)
        , sess(session)
        , data_to_write(false) {
        // Create BIO instance and store a pointer to the socket in it which will be used by the read and write functions
#ifdef INSPIRCD_OPENSSL_OPAQUE_BIO
        BIO* bio = BIO_new(biomethods);
#else
        BIO* bio = BIO_new(&biomethods);
#endif
        BIO_set_data(bio, sock);
        SSL_set_bio(sess, bio, bio);

        SSL_set_ex_data(sess, exdataindex, this);
        sock->AddIOHook(this);
        Handshake(sock);
    }

    void OnStreamSocketClose(StreamSocket* user) CXX11_OVERRIDE {
        CloseSession();
    }

    int OnStreamSocketRead(StreamSocket* user, std::string& recvq) CXX11_OVERRIDE {
        // Finish handshake if needed
        int prepret = PrepareIO(user);
        if (prepret <= 0) {
            return prepret;
        }

        // If we resumed the handshake then this->status will be STATUS_OPEN
        {
            ERR_clear_error();
            char* buffer = ServerInstance->GetReadBuffer();
            size_t bufsiz = ServerInstance->Config->NetBufferSize;
            int ret = SSL_read(sess, buffer, bufsiz);

            if (!CheckRenego(user)) {
                return -1;
            }

            if (ret > 0) {
                recvq.append(buffer, ret);
                int mask = 0;
                // Schedule a read if there is still data in the OpenSSL buffer
                if (SSL_pending(sess) > 0) {
                    mask |= FD_ADD_TRIAL_READ;
                }
                if (data_to_write) {
                    mask |= FD_WANT_POLL_READ | FD_WANT_SINGLE_WRITE;
                }
                if (mask != 0) {
                    SocketEngine::ChangeEventMask(user, mask);
                }
                return 1;
            } else if (ret == 0) {
                // Client closed connection.
                CloseSession();
                user->SetError("Connection closed");
                return -1;
            } else { // if (ret < 0)
                int err = SSL_get_error(sess, ret);

                if (err == SSL_ERROR_WANT_READ) {
                    SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ);
                    return 0;
                } else if (err == SSL_ERROR_WANT_WRITE) {
                    SocketEngine::ChangeEventMask(user, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
                    return 0;
                } else {
                    CloseSession();
                    return -1;
                }
            }
        }
    }

    int OnStreamSocketWrite(StreamSocket* user,
                            StreamSocket::SendQueue& sendq) CXX11_OVERRIDE {
        // Finish handshake if needed
        int prepret = PrepareIO(user);
        if (prepret <= 0) {
            return prepret;
        }

        data_to_write = true;

        // Session is ready for transferring application data
        while (!sendq.empty()) {
            ERR_clear_error();
            FlattenSendQueue(sendq, GetProfile().GetOutgoingRecordSize());
            const StreamSocket::SendQueue::Element& buffer = sendq.front();
            int ret = SSL_write(sess, buffer.data(), buffer.size());

            if (!CheckRenego(user)) {
                return -1;
            }

            if (ret == (int)buffer.length()) {
                // Wrote entire record, continue sending
                sendq.pop_front();
            } else if (ret > 0) {
                sendq.erase_front(ret);
                SocketEngine::ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
                return 0;
            } else if (ret == 0) {
                CloseSession();
                return -1;
            } else { // if (ret < 0)
                int err = SSL_get_error(sess, ret);

                if (err == SSL_ERROR_WANT_WRITE) {
                    SocketEngine::ChangeEventMask(user, FD_WANT_SINGLE_WRITE);
                    return 0;
                } else if (err == SSL_ERROR_WANT_READ) {
                    SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ);
                    return 0;
                } else {
                    CloseSession();
                    return -1;
                }
            }
        }

        data_to_write = false;
        SocketEngine::ChangeEventMask(user, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
        return 1;
    }

    void GetCiphersuite(std::string& out) const CXX11_OVERRIDE {
        if (!IsHookReady()) {
            return;
        }
        out.append(SSL_get_version(sess)).push_back('-');
        out.append(SSL_get_cipher(sess));
    }

    bool GetServerName(std::string& out) const CXX11_OVERRIDE {
        const char* name = SSL_get_servername(sess, TLSEXT_NAMETYPE_host_name);
        if (!name) {
            return false;
        }

        out.append(name);
        return true;
    }

    OpenSSL::Profile& GetProfile();
};

static void StaticSSLInfoCallback(const SSL* ssl, int where, int rc) {
    OpenSSLIOHook* hook = static_cast<OpenSSLIOHook*>(SSL_get_ex_data(ssl,
                          exdataindex));
    hook->SSLInfoCallback(where, rc);
}

static int OpenSSL::BIOMethod::write(BIO* bio, const char* buffer, int size) {
    BIO_clear_retry_flags(bio);

    StreamSocket* sock = static_cast<StreamSocket*>(BIO_get_data(bio));
    if (sock->GetEventMask() & FD_WRITE_WILL_BLOCK) {
        // Writes blocked earlier, don't retry syscall
        BIO_set_retry_write(bio);
        return -1;
    }

    int ret = SocketEngine::Send(sock, buffer, size, 0);
    if ((ret < size) && ((ret > 0) || (SocketEngine::IgnoreError()))) {
        // Blocked, set retry flag for OpenSSL
        SocketEngine::ChangeEventMask(sock, FD_WRITE_WILL_BLOCK);
        BIO_set_retry_write(bio);
    }

    return ret;
}

static int OpenSSL::BIOMethod::read(BIO* bio, char* buffer, int size) {
    BIO_clear_retry_flags(bio);

    StreamSocket* sock = static_cast<StreamSocket*>(BIO_get_data(bio));
    if (sock->GetEventMask() & FD_READ_WILL_BLOCK) {
        // Reads blocked earlier, don't retry syscall
        BIO_set_retry_read(bio);
        return -1;
    }

    int ret = SocketEngine::Recv(sock, buffer, size, 0);
    if ((ret < size) && ((ret > 0) || (SocketEngine::IgnoreError()))) {
        // Blocked, set retry flag for OpenSSL
        SocketEngine::ChangeEventMask(sock, FD_READ_WILL_BLOCK);
        BIO_set_retry_read(bio);
    }

    return ret;
}

class OpenSSLIOHookProvider : public SSLIOHookProvider {
    OpenSSL::Profile profile;

  public:
    OpenSSLIOHookProvider(Module* mod, const std::string& profilename,
                          ConfigTag* tag)
        : SSLIOHookProvider(mod, profilename)
        , profile(profilename, tag) {
        ServerInstance->Modules->AddService(*this);
    }

    ~OpenSSLIOHookProvider() {
        ServerInstance->Modules->DelService(*this);
    }

    void OnAccept(StreamSocket* sock, irc::sockets::sockaddrs* client,
                  irc::sockets::sockaddrs* server) CXX11_OVERRIDE {
        new OpenSSLIOHook(this, sock, profile.CreateServerSession());
    }

    void OnConnect(StreamSocket* sock) CXX11_OVERRIDE {
        new OpenSSLIOHook(this, sock, profile.CreateClientSession());
    }

    OpenSSL::Profile& GetProfile() {
        return profile;
    }
};

OpenSSL::Profile& OpenSSLIOHook::GetProfile() {
    IOHookProvider* hookprov = prov;
    return static_cast<OpenSSLIOHookProvider*>(hookprov)->GetProfile();
}

class ModuleSSLOpenSSL : public Module {
    typedef std::vector<reference<OpenSSLIOHookProvider> > ProfileList;

    ProfileList profiles;

    void ReadProfiles() {
        ProfileList newprofiles;
        ConfigTagList tags = ServerInstance->Config->ConfTags("sslprofile");
        if (tags.first == tags.second) {
            // Create a default profile named "openssl"
            const std::string defname = "openssl";
            ConfigTag* tag = ServerInstance->Config->ConfValue(defname);
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                      "No <sslprofile> tags found, using settings from the deprecated <openssl> tag");

            try {
                newprofiles.push_back(new OpenSSLIOHookProvider(this, defname, tag));
            } catch (OpenSSL::Exception& ex) {
                throw ModuleException("Error while initializing the default TLS (SSL) profile - "
                                      + ex.GetReason());
            }
        } else {
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                      "You have defined an <sslprofile> tag; you should use this in place of \"openssl\" when configuring TLS (SSL) connections in <bind:sslprofile> or <link:sslprofile>");
            for (ConfigIter i = tags.first; i != tags.second; ++i) {
                ConfigTag* tag = i->second;
                if (!stdalgo::string::equalsci(tag->getString("provider"), "openssl")) {
                    ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                              "Ignoring non-OpenSSL <sslprofile> tag at " + tag->getTagLocation());
                    continue;
                }

                std::string name = tag->getString("name");
                if (name.empty()) {
                    ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                              "Ignoring <sslprofile> tag without name at " + tag->getTagLocation());
                    continue;
                }

                reference<OpenSSLIOHookProvider> prov;
                try {
                    prov = new OpenSSLIOHookProvider(this, name, tag);
                } catch (CoreException& ex) {
                    throw ModuleException("Error while initializing TLS (SSL) profile \"" + name +
                                          "\" at " + tag->getTagLocation() + " - " + ex.GetReason());
                }

                newprofiles.push_back(prov);
            }
        }

        for (ProfileList::iterator i = profiles.begin(); i != profiles.end(); ++i) {
            OpenSSLIOHookProvider& prov = **i;
            ServerInstance->Modules.DelService(prov);
        }

        profiles.swap(newprofiles);
    }

  public:
    ModuleSSLOpenSSL() {
        // Initialize OpenSSL
        OPENSSL_init_ssl(0, NULL);
#ifdef INSPIRCD_OPENSSL_OPAQUE_BIO
        biomethods = OpenSSL::BIOMethod::alloc();
    }

    ~ModuleSSLOpenSSL() {
        BIO_meth_free(biomethods);
#endif
    }

    void init() CXX11_OVERRIDE {
        ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "OpenSSL lib version \"%s\" module was compiled for \"" OPENSSL_VERSION_TEXT "\"", OpenSSL_version(OPENSSL_VERSION));

        // Register application specific data
        char exdatastr[] = "inspircd";
        exdataindex = SSL_get_ex_new_index(0, exdatastr, NULL, NULL, NULL);
        if (exdataindex < 0) {
            throw ModuleException("Failed to register application specific data");
        }
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("openssl");
        if (status.initial || tag->getBool("onrehash")) {
            ReadProfiles();
        }
    }

    void OnModuleRehash(User* user, const std::string &param) CXX11_OVERRIDE {
        if (!irc::equals(param, "tls") && !irc::equals(param, "ssl")) {
            return;
        }

        try {
            ReadProfiles();
            ServerInstance->SNO->WriteToSnoMask('a',
                                                "OpenSSL TLS (SSL) profiles have been reloaded.");
        } catch (ModuleException& ex) {
            ServerInstance->SNO->WriteToSnoMask('a',
                                                "Failed to reload the OpenSSL TLS (SSL) profiles. " + ex.GetReason());
        }
    }

    void OnCleanup(ExtensionItem::ExtensibleType type,
                   Extensible* item) CXX11_OVERRIDE {
        if (type == ExtensionItem::EXT_USER) {
            LocalUser* user = IS_LOCAL((User*)item);

            if ((user) && (user->eh.GetModHook(this))) {
                // User is using TLS (SSL), they're a local user, and they're using one of *our* TLS (SSL) ports.
                // Potentially there could be multiple TLS (SSL) modules loaded at once on different ports.
                ServerInstance->Users->QuitUser(user, "OpenSSL module unloading");
            }
        }
    }

    ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE {
        const OpenSSLIOHook* const iohook = static_cast<OpenSSLIOHook*>(user->eh.GetModHook(this));
        if ((iohook) && (!iohook->IsHookReady())) {
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows TLS (SSL) encrypted connections using the OpenSSL library.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleSSLOpenSSL)
