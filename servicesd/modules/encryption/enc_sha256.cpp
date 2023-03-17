/* This module generates and compares password hashes using SHA256 algorithms.
 *
 * If an intruder gets access to your system or uses a brute force attack,
 * salt will not provide much value.
 * IMPORTANT: DATA HASHES CANNOT BE "DECRYPTED" BACK TO PLAIN TEXT.
 *
 * Modified for Anope.
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Taken from InspIRCd (https://www.inspircd.org/),
 * see https://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 * the file COPYING for details.
 */

/* FIPS 180-2 SHA-224/256/384/512 implementation
 * Last update: 05/23/2005
 * Issue date:  04/30/2005
 *
 * Copyright (C) 2005 Olivier Gay <olivier.gay@a3.epfl.ch>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "module.h"
#include "modules/encryption.h"

static const unsigned SHA256_DIGEST_SIZE = 256 / 8;
static const unsigned SHA256_BLOCK_SIZE = 512 / 8;

inline static uint32_t SHFR(uint32_t x, uint32_t n) {
    return x >> n;
}
inline static uint32_t ROTR(uint32_t x, uint32_t n) {
    return (x >> n) | (x << ((sizeof(x) << 3) - n));
}
inline static uint32_t CH(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}
inline static uint32_t MAJ(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline static uint32_t SHA256_F1(uint32_t x) {
    return ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22);
}
inline static uint32_t SHA256_F2(uint32_t x) {
    return ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25);
}
inline static uint32_t SHA256_F3(uint32_t x) {
    return ROTR(x, 7) ^ ROTR(x, 18) ^ SHFR(x, 3);
}
inline static uint32_t SHA256_F4(uint32_t x) {
    return ROTR(x, 17) ^ ROTR(x, 19) ^ SHFR(x, 10);
}

inline static void UNPACK32(unsigned x, unsigned char *str) {
    str[3] = static_cast<uint8_t>(x);
    str[2] = static_cast<uint8_t>(x >> 8);
    str[1] = static_cast<uint8_t>(x >> 16);
    str[0] = static_cast<uint8_t>(x >> 24);
}

inline static void PACK32(unsigned char *str, uint32_t &x) {
    x = static_cast<uint32_t>(str[3]) | static_cast<uint32_t>
        (str[2]) << 8 | static_cast<uint32_t>(str[1]) << 16 | static_cast<uint32_t>
        (str[0]) << 24;
}

/* Macros used for loops unrolling */

inline static void SHA256_SCR(uint32_t w[64], int i) {
    w[i] = SHA256_F4(w[i - 2]) + w[i - 7] + SHA256_F3(w[i - 15]) + w[i - 16];
}

static const uint32_t sha256_h0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/** An sha256 context
 */
class SHA256Context : public Encryption::Context {
    void Transform(unsigned char *message, unsigned block_nb) {
        uint32_t w[64], wv[8];
        unsigned char *sub_block;
        for (unsigned i = 1; i <= block_nb; ++i) {
            int j;
            sub_block = message + ((i - 1) << 6);

            for (j = 0; j < 16; ++j) {
                PACK32(&sub_block[j << 2], w[j]);
            }
            for (j = 16; j < 64; ++j) {
                SHA256_SCR(w, j);
            }
            for (j = 0; j < 8; ++j) {
                wv[j] = this->h[j];
            }
            for (j = 0; j < 64; ++j) {
                uint32_t t1 = wv[7] + SHA256_F2(wv[4]) + CH(wv[4], wv[5],
                              wv[6]) + sha256_k[j] + w[j];
                uint32_t t2 = SHA256_F1(wv[0]) + MAJ(wv[0], wv[1], wv[2]);
                wv[7] = wv[6];
                wv[6] = wv[5];
                wv[5] = wv[4];
                wv[4] = wv[3] + t1;
                wv[3] = wv[2];
                wv[2] = wv[1];
                wv[1] = wv[0];
                wv[0] = t1 + t2;
            }
            for (j = 0; j < 8; ++j) {
                this->h[j] += wv[j];
            }
        }
    }

    unsigned tot_len;
    unsigned len;
    unsigned char block[2 * SHA256_BLOCK_SIZE];
    uint32_t h[8];
    unsigned char digest[SHA256_DIGEST_SIZE];

  public:
    SHA256Context(Encryption::IV *iv) {
        if (iv != NULL) {
            if (iv->second != 8) {
                throw CoreException("Invalid IV size");
            }
            for (int i = 0; i < 8; ++i) {
                this->h[i] = iv->first[i];
            }
        } else
            for (int i = 0; i < 8; ++i) {
                this->h[i] = sha256_h0[i];
            }

        this->tot_len = 0;
        this->len = 0;
        memset(this->block, 0, sizeof(this->block));
        memset(this->digest, 0, sizeof(this->digest));
    }

    void Update(const unsigned char *message, size_t mlen) anope_override {
        unsigned tmp_len = SHA256_BLOCK_SIZE - this->len, rem_len = mlen < tmp_len ? mlen : tmp_len;

        memcpy(&this->block[this->len], message, rem_len);
        if (this->len + mlen < SHA256_BLOCK_SIZE) {
            this->len += mlen;
            return;
        }
        unsigned new_len = mlen - rem_len, block_nb = new_len / SHA256_BLOCK_SIZE;
        unsigned char *shifted_message = new unsigned char[mlen - rem_len];
        memcpy(shifted_message, message + rem_len, mlen - rem_len);
        this->Transform(this->block, 1);
        this->Transform(shifted_message, block_nb);
        rem_len = new_len % SHA256_BLOCK_SIZE;
        memcpy(this->block, &shifted_message[block_nb << 6], rem_len);
        delete [] shifted_message;
        this->len = rem_len;
        this->tot_len += (block_nb + 1) << 6;
    }

    void Finalize() anope_override {
        unsigned block_nb = 1 + ((SHA256_BLOCK_SIZE - 9) < (this->len % SHA256_BLOCK_SIZE));
        unsigned len_b = (this->tot_len + this->len) << 3;
        unsigned pm_len = block_nb << 6;
        memset(this->block + this->len, 0, pm_len - this->len);
        this->block[this->len] = 0x80;
        UNPACK32(len_b, this->block + pm_len - 4);
        this->Transform(this->block, block_nb);
        for (int i = 0 ; i < 8; ++i) {
            UNPACK32(this->h[i], &this->digest[i << 2]);
        }
    }

    Encryption::Hash GetFinalizedHash() anope_override {
        Encryption::Hash hash;
        hash.first = this->digest;
        hash.second = SHA256_DIGEST_SIZE;
        return hash;
    }
};

class SHA256Provider : public Encryption::Provider {
  public:
    SHA256Provider(Module *creator) : Encryption::Provider(creator, "sha256") { }

    Encryption::Context *CreateContext(Encryption::IV *iv) anope_override {
        return new SHA256Context(iv);
    }

    Encryption::IV GetDefaultIV() anope_override {
        Encryption::IV iv;
        iv.first = sha256_h0;
        iv.second = sizeof(sha256_h0) / sizeof(uint32_t);
        return iv;
    }
};

class ESHA256 : public Module {
    SHA256Provider sha256provider;

    unsigned iv[8];
    bool use_iv;

    /* initializes the IV with a new random value */
    void NewRandomIV() {
        for (int i = 0; i < 8; ++i) {
            iv[i] = static_cast<uint32_t>(rand());
        }
    }

    /* returns the IV as base64-encrypted string */
    Anope::string GetIVString() {
        char buf[33];
        for (int i = 0; i < 8; ++i) {
            UNPACK32(iv[i], reinterpret_cast<unsigned char *>(&buf[i << 2]));
        }
        buf[32] = '\0';
        return Anope::Hex(buf, 32);
    }

    /* splits the appended IV from the password string so it can be used for the next encryption */
    /* password format:  <hashmethod>:<password_b64>:<iv_b64> */
    void GetIVFromPass(const Anope::string &password) {
        size_t pos = password.find(':');
        Anope::string buf = password.substr(password.find(':', pos + 1) + 1,
                                            password.length());
        char buf2[33];
        Anope::Unhex(buf, buf2, sizeof(buf2));
        for (int i = 0 ; i < 8; ++i) {
            PACK32(reinterpret_cast<unsigned char *>(&buf2[i << 2]), iv[i]);
        }
    }

  public:
    ESHA256(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, ENCRYPTION | VENDOR),
        sha256provider(this) {


        use_iv = false;
    }

    EventReturn OnEncrypt(const Anope::string &src,
                          Anope::string &dest) anope_override {
        if (!use_iv) {
            NewRandomIV();
        } else {
            use_iv = false;
        }

        Encryption::IV initialization(this->iv, 8);
        SHA256Context ctx(&initialization);
        ctx.Update(reinterpret_cast<const unsigned char *>(src.c_str()), src.length());
        ctx.Finalize();

        Encryption::Hash hash = ctx.GetFinalizedHash();

        std::stringstream buf;
        buf << "sha256:" << Anope::Hex(reinterpret_cast<const char *>(hash.first), hash.second) << ":" << GetIVString();
        Log(LOG_DEBUG_2) << "(enc_sha256) hashed password from [" << src << "] to [" << buf.str() << " ]";
        dest = buf.str();
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
        if (!hash_method.equals_cs("sha256")) {
            return;
        }

        GetIVFromPass(nc->pass);
        use_iv = true;
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

MODULE_INIT(ESHA256)
