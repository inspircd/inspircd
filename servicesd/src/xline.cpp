/* XLine functions.
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
#include "modules.h"
#include "xline.h"
#include "users.h"
#include "sockets.h"
#include "regexpr.h"
#include "config.h"
#include "commands.h"
#include "servers.h"

/* List of XLine managers we check users against in XLineManager::CheckAll */
std::list<XLineManager *> XLineManager::XLineManagers;
Serialize::Checker<std::multimap<Anope::string, XLine *, ci::less> >
XLineManager::XLinesByUID("XLine");

void XLine::Init() {
    if (this->mask.length() >= 2 && this->mask[0] == '/'
            && this->mask[this->mask.length() - 1] == '/'
            && !Config->GetBlock("options")->Get<const Anope::string>("regexengine").empty()) {
        Anope::string stripped_mask = this->mask.substr(1, this->mask.length() - 2);

        ServiceReference<RegexProvider> provider("Regex",
                Config->GetBlock("options")->Get<const Anope::string>("regexengine"));
        if (provider) {
            try {
                this->regex = provider->Compile(stripped_mask);
            } catch (const RegexException &ex) {
                Log(LOG_DEBUG) << ex.GetReason();
            }
        }
    }

    size_t nick_t = this->mask.find('!');
    if (nick_t != Anope::string::npos) {
        nick = this->mask.substr(0, nick_t);
    }

    size_t user_t = this->mask.find('!'), host_t = this->mask.find('@');
    if (host_t != Anope::string::npos) {
        if (user_t != Anope::string::npos && host_t > user_t) {
            user = this->mask.substr(user_t + 1, host_t - user_t - 1);
        } else {
            user = this->mask.substr(0, host_t);
        }
    }

    size_t real_t = this->mask.find('#');
    if (host_t != Anope::string::npos) {
        if (real_t != Anope::string::npos && real_t > host_t) {
            host = this->mask.substr(host_t + 1, real_t - host_t - 1);
        } else {
            host = this->mask.substr(host_t + 1);
        }
    } else {
        if (real_t != Anope::string::npos) {
            host = this->mask.substr(0, real_t);
        } else {
            host = this->mask;
        }
    }

    if (real_t != Anope::string::npos) {
        real = this->mask.substr(real_t + 1);
    }

    if (host.find('/') != Anope::string::npos) {
        c = new cidr(host);
        if (!c->valid()) {
            delete c;
            c = NULL;
        }
    }
}

XLine::XLine(const Anope::string &ma, const Anope::string &r,
             const Anope::string &uid) : Serializable("XLine"), mask(ma), by(Me->GetName()),
    created(0), expires(0), reason(r), id(uid) {
    regex = NULL;
    manager = NULL;
    c = NULL;

    this->Init();
}

XLine::XLine(const Anope::string &ma, const Anope::string &b, const time_t ex,
             const Anope::string &r, const Anope::string &uid) : Serializable("XLine"),
    mask(ma), by(b), created(Anope::CurTime), expires(ex), reason(r), id(uid) {
    regex = NULL;
    manager = NULL;
    c = NULL;

    this->Init();
}

XLine::~XLine() {
    if (manager) {
        manager->RemoveXLine(this);
    }

    delete regex;
    delete c;
}

const Anope::string &XLine::GetNick() const {
    return nick;
}

const Anope::string &XLine::GetUser() const {
    return user;
}

const Anope::string &XLine::GetHost() const {
    return host;
}

const Anope::string &XLine::GetReal() const {
    return real;
}

Anope::string XLine::GetReason() const {
    Anope::string r = this->reason;
    if (!this->id.empty()) {
        r += " (ID: " + this->id + ")";
    }
    return r;
}

bool XLine::HasNickOrReal() const {
    return !this->GetNick().empty() || !this->GetReal().empty();
}

bool XLine::IsRegex() const {
    return !this->mask.empty() && this->mask[0] == '/'
           && this->mask[this->mask.length() - 1] == '/';
}

void XLine::Serialize(Serialize::Data &data) const {
    data["mask"] << this->mask;
    data["by"] << this->by;
    data["created"] << this->created;
    data["expires"] << this->expires;
    data["reason"] << this->reason;
    data["uid"] << this->id;
    if (this->manager) {
        data["manager"] << this->manager->name;
    }
}

Serializable* XLine::Unserialize(Serializable *obj, Serialize::Data &data) {
    Anope::string smanager;

    data["manager"] >> smanager;

    ServiceReference<XLineManager> xlm("XLineManager", smanager);
    if (!xlm) {
        return NULL;
    }

    XLine *xl;
    if (obj) {
        xl = anope_dynamic_static_cast<XLine *>(obj);
        data["mask"] >> xl->mask;
        data["by"] >> xl->by;
        data["reason"] >> xl->reason;
        data["uid"] >> xl->id;

        if (xlm != xl->manager) {
            xl->manager->DelXLine(xl);
            xlm->AddXLine(xl);
        }
    } else {
        Anope::string smask, sby, sreason, suid;
        time_t expires;

        data["mask"] >> smask;
        data["by"] >> sby;
        data["reason"] >> sreason;
        data["uid"] >> suid;
        data["expires"] >> expires;

        xl = new XLine(smask, sby, expires, sreason, suid);
        xlm->AddXLine(xl);
    }

    data["created"] >> xl->created;
    xl->manager = xlm;

    return xl;
}

void XLineManager::RegisterXLineManager(XLineManager *xlm) {
    XLineManagers.push_back(xlm);
}

void XLineManager::UnregisterXLineManager(XLineManager *xlm) {
    std::list<XLineManager *>::iterator it = std::find(XLineManagers.begin(),
            XLineManagers.end(), xlm);

    if (it != XLineManagers.end()) {
        XLineManagers.erase(it);
    }
}

void XLineManager::CheckAll(User *u) {
    for (std::list<XLineManager *>::iterator it = XLineManagers.begin(),
            it_end = XLineManagers.end(); it != it_end; ++it) {
        XLineManager *xlm = *it;

        if (xlm->CheckAllXLines(u)) {
            break;
        }
    }
}

Anope::string XLineManager::GenerateUID() {
    Anope::string id;
    int count = 0;
    do {
        id.clear();

        if (++count > 10) {
            Log(LOG_DEBUG) << "Unable to generate XLine UID";
            break;
        }

        for (int i = 0; i < 10; ++i) {
            char c;
            do {
                c = (rand() % 75) + 48;
            } while (!isupper(c) && !isdigit(c));
            id += c;
        }
    } while (XLinesByUID->count(id) > 0);

    return id;
}

XLineManager::XLineManager(Module *creator, const Anope::string &xname,
                           char t) : Service(creator, "XLineManager", xname), type(t), xlines("XLine") {
}

XLineManager::~XLineManager() {
    this->Clear();
}

const char &XLineManager::Type() {
    return this->type;
}

size_t XLineManager::GetCount() const {
    return this->xlines->size();
}

const std::vector<XLine *> &XLineManager::GetList() const {
    return this->xlines;
}

void XLineManager::AddXLine(XLine *x) {
    if (!x->id.empty()) {
        XLinesByUID->insert(std::make_pair(x->id, x));
    }
    this->xlines->push_back(x);
    x->manager = this;
}

void XLineManager::RemoveXLine(XLine *x) {
    /* called from the destructor */

    std::vector<XLine *>::iterator it = std::find(this->xlines->begin(),
                                        this->xlines->end(), x);

    if (!x->id.empty()) {
        std::multimap<Anope::string, XLine *, ci::less>::iterator it2 =
            XLinesByUID->find(x->id), it3 = XLinesByUID->upper_bound(x->id);
        for (; it2 != XLinesByUID->end() && it2 != it3; ++it2)
            if (it2->second == x) {
                XLinesByUID->erase(it2);
                break;
            }
    }

    if (it != this->xlines->end()) {
        this->SendDel(x);
        this->xlines->erase(it);
    }
}

bool XLineManager::DelXLine(XLine *x) {
    std::vector<XLine *>::iterator it = std::find(this->xlines->begin(),
                                        this->xlines->end(), x);

    if (!x->id.empty()) {
        std::multimap<Anope::string, XLine *, ci::less>::iterator it2 =
            XLinesByUID->find(x->id), it3 = XLinesByUID->upper_bound(x->id);
        for (; it2 != XLinesByUID->end() && it2 != it3; ++it2)
            if (it2->second == x) {
                XLinesByUID->erase(it2);
                break;
            }
    }

    if (it != this->xlines->end()) {
        this->SendDel(x);

        x->manager = NULL; // Don't call remove
        delete x;
        this->xlines->erase(it);

        return true;
    }

    return false;
}

XLine* XLineManager::GetEntry(unsigned index) {
    if (index >= this->xlines->size()) {
        return NULL;
    }

    XLine *x = this->xlines->at(index);
    x->QueueUpdate();
    return x;
}

void XLineManager::Clear() {
    std::vector<XLine *> xl;
    this->xlines->swap(xl);

    for (unsigned i = 0; i < xl.size(); ++i) {
        XLine *x = xl[i];
        if (!x->id.empty()) {
            XLinesByUID->erase(x->id);
        }
        delete x;
    }
}

bool XLineManager::CanAdd(CommandSource &source, const Anope::string &mask,
                          time_t expires, const Anope::string &reason) {
    for (unsigned i = this->GetCount(); i > 0; --i) {
        XLine *x = this->GetEntry(i - 1);

        if (x->mask.equals_ci(mask)) {
            if (!x->expires || x->expires >= expires) {
                if (x->reason != reason) {
                    x->reason = reason;
                    source.Reply(_("Reason for %s updated."), x->mask.c_str());
                } else {
                    source.Reply(_("%s already exists."), mask.c_str());
                }
            } else {
                x->expires = expires;
                if (x->reason != reason) {
                    x->reason = reason;
                    source.Reply(_("Expiry and reason updated for %s."), x->mask.c_str());
                } else {
                    source.Reply(_("Expiry for %s updated."), x->mask.c_str());
                }
            }

            return false;
        } else if (Anope::Match(mask, x->mask) && (!x->expires
                   || x->expires >= expires)) {
            source.Reply(_("%s is already covered by %s."), mask.c_str(), x->mask.c_str());
            return false;
        } else if (Anope::Match(x->mask, mask) && (!expires || x->expires <= expires)) {
            source.Reply(_("Removing %s because %s covers it."), x->mask.c_str(),
                         mask.c_str());
            this->DelXLine(x);
        }
    }

    return true;
}

XLine* XLineManager::HasEntry(const Anope::string &mask) {
    std::multimap<Anope::string, XLine *, ci::less>::iterator it =
        XLinesByUID->find(mask);
    if (it != XLinesByUID->end())
        for (std::multimap<Anope::string, XLine *, ci::less>::iterator it2 =
                    XLinesByUID->upper_bound(mask); it != it2; ++it)
            if (it->second->manager == NULL || it->second->manager == this) {
                it->second->QueueUpdate();
                return it->second;
            }
    for (unsigned i = 0, end = this->xlines->size(); i < end; ++i) {
        XLine *x = this->xlines->at(i);

        if (x->mask.equals_ci(mask)) {
            x->QueueUpdate();
            return x;
        }
    }

    return NULL;
}

XLine *XLineManager::CheckAllXLines(User *u) {
    for (unsigned i = this->xlines->size(); i > 0; --i) {
        XLine *x = this->xlines->at(i - 1);

        if (x->expires && x->expires < Anope::CurTime) {
            this->OnExpire(x);
            this->DelXLine(x);
            continue;
        }

        if (this->Check(u, x)) {
            this->OnMatch(u, x);
            return x;
        }
    }

    return NULL;
}

void XLineManager::OnExpire(const XLine *x) {
}
