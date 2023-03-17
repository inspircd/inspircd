/* Include file for high-level encryption routines.
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "module.h"
#include "modules/encryption.h"

static ServiceReference<Encryption::Provider> md5("Encryption::Provider",
        "md5");

class OldMD5Provider : public Encryption::Provider {
  public:
    OldMD5Provider(Module *creator) : Encryption::Provider(creator, "oldmd5") { }

    Encryption::Context *CreateContext(Encryption::IV *iv) anope_override {
        if (md5) {
            return md5->CreateContext(iv);
        }
        return NULL;
    }

    Encryption::IV GetDefaultIV() anope_override {
        if (md5) {
            return md5->GetDefaultIV();
        }
        return Encryption::IV(static_cast<const uint32_t *>(NULL), 0);
    }
};

class EOld : public Module {
    OldMD5Provider oldmd5provider;

    inline static char XTOI(char c) {
        return c > 9 ? c - 'A' + 10 : c - '0';
    }

  public:
    EOld(const Anope::string &modname,
         const Anope::string &creator) : Module(modname, creator, ENCRYPTION | VENDOR),
        oldmd5provider(this) {

        ModuleManager::LoadModule("enc_md5", User::Find(creator, true));
        if (!md5) {
            throw ModuleException("Unable to find md5 reference");
        }

    }

    EventReturn OnEncrypt(const Anope::string &src,
                          Anope::string &dest) anope_override {
        if (!md5) {
            return EVENT_CONTINUE;
        }

        Encryption::Context *context = md5->CreateContext();
        context->Update(reinterpret_cast<const unsigned char *>(src.c_str()), src.length());
        context->Finalize();

        Encryption::Hash hash = context->GetFinalizedHash();

        char digest[32], digest2[16];
        memset(digest, 0, sizeof(digest));
        if (hash.second > sizeof(digest)) {
            throw CoreException("Hash too large");
        }
        memcpy(digest, hash.first, hash.second);

        for (int i = 0; i < 32; i += 2) {
            digest2[i / 2] = XTOI(digest[i]) << 4 | XTOI(digest[i + 1]);
        }

        Anope::string buf = "oldmd5:" + Anope::Hex(digest2, sizeof(digest2));

        Log(LOG_DEBUG_2) << "(enc_old) hashed password from [" << src << "] to [" << buf << "]";
        dest = buf;
        delete context;
        return EVENT_ALLOW;
    }

    void OnCheckAuthentication(User *, IdentifyRequest *req) anope_override {
        const NickAlias *na = NickAlias::Find(req->GetAccount());
        if (na == NULL) {
            return;
        }
        NickCore *nc = na->nc;

        size_t pos = nc->pass.find(':');
        if (pos == Anope::string::npos) {
            return;
        }
        Anope::string hash_method(nc->pass.begin(), nc->pass.begin() + pos);
        if (!hash_method.equals_cs("oldmd5")) {
            return;
        }

        Anope::string buf;
        this->OnEncrypt(req->GetPassword(), buf);
        if (nc->pass.equals_cs(buf)) {
            /* if we are NOT the first module in the list,
             * we want to re-encrypt the pass with the new encryption
             */
            if (ModuleManager::FindFirstOf(ENCRYPTION) != this) {
                Anope::Encrypt(req->GetPassword(), nc->pass);
            }
            req->Success(this);
        }
    }
};

MODULE_INIT(EOld)
