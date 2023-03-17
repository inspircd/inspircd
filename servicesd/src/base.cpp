/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "services.h"
#include "anope.h"
#include "service.h"

std::map<Anope::string, std::map<Anope::string, Service *> > Service::Services;
std::map<Anope::string, std::map<Anope::string, Anope::string> >
Service::Aliases;

Base::Base() : references(NULL) {
}

Base::~Base() {
    if (this->references != NULL) {
        for (std::set<ReferenceBase *>::iterator it = this->references->begin(),
                it_end = this->references->end(); it != it_end; ++it) {
            (*it)->Invalidate();
        }
        delete this->references;
    }
}

void Base::AddReference(ReferenceBase *r) {
    if (this->references == NULL) {
        this->references = new std::set<ReferenceBase *>();
    }
    this->references->insert(r);
}

void Base::DelReference(ReferenceBase *r) {
    if (this->references != NULL) {
        this->references->erase(r);
        if (this->references->empty()) {
            delete this->references;
            this->references = NULL;
        }
    }
}
