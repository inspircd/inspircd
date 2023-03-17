/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

namespace Encryption {
typedef std::pair<const unsigned char *, size_t> Hash;
typedef std::pair<const uint32_t *, size_t> IV;

class Context {
  public:
    virtual ~Context() { }
    virtual void Update(const unsigned char *data, size_t len) = 0;
    virtual void Finalize() = 0;
    virtual Hash GetFinalizedHash() = 0;
};

class Provider : public Service {
  public:
    Provider(Module *creator, const Anope::string &sname) : Service(creator,
                "Encryption::Provider", sname) { }
    virtual ~Provider() { }

    virtual Context *CreateContext(IV * = NULL) = 0;
    virtual IV GetDefaultIV() = 0;
};
}
