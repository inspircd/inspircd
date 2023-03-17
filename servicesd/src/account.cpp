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

#include "services.h"
#include "account.h"
#include "modules.h"
#include "users.h"
#include "protocol.h"
#include "regchannel.h"

std::set<IdentifyRequest *> IdentifyRequest::Requests;

IdentifyRequest::IdentifyRequest(Module *o, const Anope::string &acc,
                                 const Anope::string &pass) : owner(o), account(acc), password(pass),
    dispatched(false), success(false) {
    Requests.insert(this);
}

IdentifyRequest::~IdentifyRequest() {
    Requests.erase(this);
}

void IdentifyRequest::Hold(Module *m) {
    holds.insert(m);
}

void IdentifyRequest::Release(Module *m) {
    holds.erase(m);
    if (holds.empty() && dispatched) {
        if (!success) {
            this->OnFail();
        }
        delete this;
    }
}

void IdentifyRequest::Success(Module *m) {
    if (!success) {
        this->OnSuccess();
        success = true;
    }
}

void IdentifyRequest::Dispatch() {
    if (holds.empty()) {
        if (!success) {
            this->OnFail();
        }
        delete this;
    } else {
        dispatched = true;
    }
}

void IdentifyRequest::ModuleUnload(Module *m) {
    for (std::set<IdentifyRequest *>::iterator it = Requests.begin(),
            it_end = Requests.end(); it != it_end;) {
        IdentifyRequest *ir = *it;
        ++it;

        ir->holds.erase(m);
        if (ir->holds.empty() && ir->dispatched) {
            if (!ir->success) {
                ir->OnFail();
            }
            delete ir;
            continue;
        }

        if (ir->owner == m) {
            if (!ir->success) {
                ir->OnFail();
            }
            delete ir;
        }
    }
}
