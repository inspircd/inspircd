/*
 *
 * (C) 2011-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#ifndef NICKSERV_H
#define NICKSERV_H

class NickServService : public Service {
  public:
    NickServService(Module *m) : Service(m, "NickServService", "NickServ") {
    }

    virtual void Validate(User *u) = 0;
    virtual void Collide(User *u, NickAlias *na) = 0;
    virtual void Release(NickAlias *na) = 0;
};

#endif // NICKSERV_H
