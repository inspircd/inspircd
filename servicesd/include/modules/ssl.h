/*
 *
 * (C) 2010-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

class SSLService : public Service {
  public:
    SSLService(Module *o, const Anope::string &n) : Service(o, "SSLService", n) { }

    virtual void Init(Socket *s) = 0;
};
