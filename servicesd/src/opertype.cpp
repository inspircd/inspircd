/*
 *
 * (C) 2008-2011 Robin Burchell <w00t@inspircd.org>
 * (C) 2008-2023 Anope Team <team@anope.org>
 *
 * Please read COPYING and README for further details.
 */

#include "services.h"
#include "anope.h"
#include "opertype.h"
#include "config.h"

std::vector<Oper *> Oper::opers;

Oper::Oper(const Anope::string &n, OperType *o) : name(n), ot(o),
    require_oper(true) {
    opers.push_back(this);
}

Oper::~Oper() {
    std::vector<Oper *>::iterator it = std::find(opers.begin(), opers.end(), this);
    if (it != opers.end()) {
        opers.erase(it);
    }
}

Oper *Oper::Find(const Anope::string &name) {
    for (unsigned i = 0; i < opers.size(); ++i) {
        Oper *o = opers[i];

        if (o->name.equals_ci(name)) {
            return o;
        }
    }

    return NULL;
}

OperType *OperType::Find(const Anope::string &name) {
    for (unsigned i = 0; i < Config->MyOperTypes.size(); ++i) {
        OperType *ot = Config->MyOperTypes[i];

        if (ot->GetName() == name) {
            return ot;
        }
    }

    return NULL;
}

OperType::OperType(const Anope::string &nname) : name(nname) {
}

bool OperType::HasCommand(const Anope::string &cmdstr) const {
    for (std::list<Anope::string>::const_iterator it = this->commands.begin(),
            it_end = this->commands.end(); it != it_end; ++it) {
        const Anope::string &s = *it;

        if (!s.find('~') && Anope::Match(cmdstr, s.substr(1))) {
            return false;
        } else if (Anope::Match(cmdstr, s)) {
            return true;
        }
    }
    for (std::set<OperType *>::const_iterator iit = this->inheritances.begin(),
            iit_end = this->inheritances.end(); iit != iit_end; ++iit) {
        OperType *ot = *iit;

        if (ot->HasCommand(cmdstr)) {
            return true;
        }
    }

    return false;
}

bool OperType::HasPriv(const Anope::string &privstr) const {
    for (std::list<Anope::string>::const_iterator it = this->privs.begin(),
            it_end = this->privs.end(); it != it_end; ++it) {
        const Anope::string &s = *it;

        if (!s.find('~') && Anope::Match(privstr, s.substr(1))) {
            return false;
        } else if (Anope::Match(privstr, s)) {
            return true;
        }
    }
    for (std::set<OperType *>::const_iterator iit = this->inheritances.begin(),
            iit_end = this->inheritances.end(); iit != iit_end; ++iit) {
        OperType *ot = *iit;

        if (ot->HasPriv(privstr)) {
            return true;
        }
    }

    return false;
}

void OperType::AddCommand(const Anope::string &cmdstr) {
    this->commands.push_back(cmdstr);
}

void OperType::AddPriv(const Anope::string &privstr) {
    this->privs.push_back(privstr);
}

const Anope::string &OperType::GetName() const {
    return this->name;
}

void OperType::Inherits(OperType *ot) {
    if (ot != this) {
        this->inheritances.insert(ot);
    }
}

const std::list<Anope::string> OperType::GetCommands() const {
    std::list<Anope::string> cmd_list = this->commands;
    for (std::set<OperType *>::const_iterator it = this->inheritances.begin(),
            it_end = this->inheritances.end(); it != it_end; ++it) {
        OperType *ot = *it;
        std::list<Anope::string> cmds = ot->GetCommands();
        for (std::list<Anope::string>::const_iterator it2 = cmds.begin(),
                it2_end = cmds.end(); it2 != it2_end; ++it2) {
            cmd_list.push_back(*it2);
        }
    }
    return cmd_list;
}

const std::list<Anope::string> OperType::GetPrivs() const {
    std::list<Anope::string> priv_list = this->privs;
    for (std::set<OperType *>::const_iterator it = this->inheritances.begin(),
            it_end = this->inheritances.end(); it != it_end; ++it) {
        OperType *ot = *it;
        std::list<Anope::string> priv = ot->GetPrivs();
        for (std::list<Anope::string>::const_iterator it2 = priv.begin(),
                it2_end = priv.end(); it2 != it2_end; ++it2) {
            priv_list.push_back(*it2);
        }
    }
    return priv_list;
}
