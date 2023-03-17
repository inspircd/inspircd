/* Registered channel functions
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
#include "regchannel.h"
#include "account.h"
#include "access.h"
#include "channels.h"
#include "config.h"
#include "bots.h"
#include "servers.h"

Serialize::Checker<registered_channel_map> RegisteredChannelList("ChannelInfo");

AutoKick::AutoKick() : Serializable("AutoKick") {
}

AutoKick::~AutoKick() {
    if (this->ci) {
        std::vector<AutoKick *>::iterator it = std::find(this->ci->akick->begin(),
                                               this->ci->akick->end(), this);
        if (it != this->ci->akick->end()) {
            this->ci->akick->erase(it);
        }

        if (nc) {
            nc->RemoveChannelReference(this->ci);
        }
    }
}

void AutoKick::Serialize(Serialize::Data &data) const {
    data["ci"] << this->ci->name;
    if (this->nc) {
        data["nc"] << this->nc->display;
    } else {
        data["mask"] << this->mask;
    }
    data["reason"] << this->reason;
    data["creator"] << this->creator;
    data.SetType("addtime", Serialize::Data::DT_INT);
    data["addtime"] << this->addtime;
    data.SetType("last_used", Serialize::Data::DT_INT);
    data["last_used"] << this->last_used;
}

Serializable* AutoKick::Unserialize(Serializable *obj, Serialize::Data &data) {
    Anope::string sci, snc;

    data["ci"] >> sci;
    data["nc"] >> snc;

    ChannelInfo *ci = ChannelInfo::Find(sci);
    if (!ci) {
        return NULL;
    }

    AutoKick *ak;
    NickCore *nc = NickCore::Find(snc);
    if (obj) {
        ak = anope_dynamic_static_cast<AutoKick *>(obj);
        data["creator"] >> ak->creator;
        data["reason"] >> ak->reason;
        ak->nc = NickCore::Find(snc);
        data["mask"] >> ak->mask;
        data["addtime"] >> ak->addtime;
        data["last_used"] >> ak->last_used;
    } else {
        time_t addtime, lastused;
        data["addtime"] >> addtime;
        data["last_used"] >> lastused;

        Anope::string screator, sreason, smask;

        data["creator"] >> screator;
        data["reason"] >> sreason;
        data["mask"] >> smask;

        if (nc) {
            ak = ci->AddAkick(screator, nc, sreason, addtime, lastused);
        } else {
            ak = ci->AddAkick(screator, smask, sreason, addtime, lastused);
        }
    }

    return ak;
}

ChannelInfo::ChannelInfo(const Anope::string &chname) :
    Serializable("ChannelInfo"),
    access("ChanAccess"), akick("AutoKick") {
    if (chname.empty()) {
        throw CoreException("Empty channel passed to ChannelInfo constructor");
    }

    this->founder = NULL;
    this->successor = NULL;
    this->c = Channel::Find(chname);
    if (this->c) {
        this->c->ci = this;
    }
    this->banexpire = 0;
    this->bi = NULL;
    this->last_topic_time = 0;

    this->name = chname;

    this->bantype = 2;
    this->memos.memomax = 0;
    this->last_used = this->time_registered = Anope::CurTime;

    size_t old = RegisteredChannelList->size();
    (*RegisteredChannelList)[this->name] = this;
    if (old == RegisteredChannelList->size()) {
        Log(LOG_DEBUG) << "Duplicate channel " << this->name <<
                       " in registered channel table?";
    }

    FOREACH_MOD(OnCreateChan, (this));
}

ChannelInfo::ChannelInfo(const ChannelInfo &ci) : Serializable("ChannelInfo"),
    access("ChanAccess"), akick("AutoKick") {
    *this = ci;

    if (this->founder) {
        ++this->founder->channelcount;
    }

    this->access->clear();
    this->akick->clear();

    FOREACH_MOD(OnCreateChan, (this));
}

ChannelInfo::~ChannelInfo() {
    FOREACH_MOD(OnDelChan, (this));

    UnsetExtensibles();

    Log(LOG_DEBUG) << "Deleting channel " << this->name;

    if (this->c) {
        if (this->bi && this->c->FindUser(this->bi)) {
            this->bi->Part(this->c);
        }

        /* Parting the service bot can cause the channel to go away */

        if (this->c) {
            if (this->c && this->c->CheckDelete()) {
                this->c->QueueForDeletion();
            }

            this->c = NULL;
        }
    }

    RegisteredChannelList->erase(this->name);

    this->SetFounder(NULL);
    this->SetSuccessor(NULL);

    this->ClearAccess();
    this->ClearAkick();

    if (!this->memos.memos->empty()) {
        for (unsigned i = 0, end = this->memos.memos->size(); i < end; ++i) {
            delete this->memos.GetMemo(i);
        }
        this->memos.memos->clear();
    }
}

void ChannelInfo::Serialize(Serialize::Data &data) const {
    data["name"] << this->name;
    if (this->founder) {
        data["founder"] << this->founder->display;
    }
    if (this->successor) {
        data["successor"] << this->successor->display;
    }
    data["description"] << this->desc;
    data.SetType("time_registered", Serialize::Data::DT_INT);
    data["time_registered"] << this->time_registered;
    data.SetType("last_used", Serialize::Data::DT_INT);
    data["last_used"] << this->last_used;
    data["last_topic"] << this->last_topic;
    data["last_topic_setter"] << this->last_topic_setter;
    data.SetType("last_topic_time", Serialize::Data::DT_INT);
    data["last_topic_time"] << this->last_topic_time;
    data.SetType("bantype", Serialize::Data::DT_INT);
    data["bantype"] << this->bantype;
    {
        Anope::string levels_buffer;
        for (Anope::map<int16_t>::const_iterator it = this->levels.begin(),
                it_end = this->levels.end(); it != it_end; ++it) {
            levels_buffer += it->first + " " + stringify(it->second) + " ";
        }
        data["levels"] << levels_buffer;
    }
    if (this->bi) {
        data["bi"] << this->bi->nick;
    }
    data.SetType("banexpire", Serialize::Data::DT_INT);
    data["banexpire"] << this->banexpire;
    data["memomax"] << this->memos.memomax;
    for (unsigned i = 0; i < this->memos.ignores.size(); ++i) {
        data["memoignores"] << this->memos.ignores[i] << " ";
    }

    Extensible::ExtensibleSerialize(this, this, data);
}

Serializable* ChannelInfo::Unserialize(Serializable *obj,
                                       Serialize::Data &data) {
    Anope::string sname, sfounder, ssuccessor, slevels, sbi;

    data["name"] >> sname;
    data["founder"] >> sfounder;
    data["successor"] >> ssuccessor;
    data["levels"] >> slevels;
    data["bi"] >> sbi;

    ChannelInfo *ci;
    if (obj) {
        ci = anope_dynamic_static_cast<ChannelInfo *>(obj);
    } else {
        ci = new ChannelInfo(sname);
    }

    ci->SetFounder(NickCore::Find(sfounder));
    ci->SetSuccessor(NickCore::Find(ssuccessor));

    data["description"] >> ci->desc;
    data["time_registered"] >> ci->time_registered;
    data["last_used"] >> ci->last_used;
    data["last_topic"] >> ci->last_topic;
    data["last_topic_setter"] >> ci->last_topic_setter;
    data["last_topic_time"] >> ci->last_topic_time;
    data["bantype"] >> ci->bantype;
    {
        std::vector<Anope::string> v;
        spacesepstream(slevels).GetTokens(v);
        for (unsigned i = 0; i + 1 < v.size(); i += 2)
            try {
                ci->levels[v[i]] = convertTo<int16_t>(v[i + 1]);
            } catch (const ConvertException &) { }
    }
    BotInfo *bi = BotInfo::Find(sbi, true);
    if (*ci->bi != bi) {
        if (bi) {
            bi->Assign(NULL, ci);
        } else if (ci->bi) {
            ci->bi->UnAssign(NULL, ci);
        }
    }
    data["banexpire"] >> ci->banexpire;
    data["memomax"] >> ci->memos.memomax;
    {
        Anope::string buf;
        data["memoignores"] >> buf;
        spacesepstream sep(buf);
        ci->memos.ignores.clear();
        while (sep.GetToken(buf)) {
            ci->memos.ignores.push_back(buf);
        }
    }

    Extensible::ExtensibleUnserialize(ci, ci, data);

    /* compat */
    bool b;
    b = false;
    data["extensible:SECURE"] >> b;
    if (b) {
        ci->Extend<bool>("CS_SECURE");
    }
    b = false;
    data["extensible:PRIVATE"] >> b;
    if (b) {
        ci->Extend<bool>("CS_PRIVATE");
    }
    b = false;
    data["extensible:NO_EXPIRE"] >> b;
    if (b) {
        ci->Extend<bool>("CS_NO_EXPIRE");
    }
    b = false;
    data["extensible:FANTASY"] >> b;
    if (b) {
        ci->Extend<bool>("BS_FANTASY");
    }
    b = false;
    data["extensible:GREET"] >> b;
    if (b) {
        ci->Extend<bool>("BS_GREET");
    }
    b = false;
    data["extensible:PEACE"] >> b;
    if (b) {
        ci->Extend<bool>("PEACE");
    }
    b = false;
    data["extensible:SECUREFOUNDER"] >> b;
    if (b) {
        ci->Extend<bool>("SECUREFOUNDER");
    }
    b = false;
    data["extensible:RESTRICTED"] >> b;
    if (b) {
        ci->Extend<bool>("RESTRICTED");
    }
    b = false;
    data["extensible:KEEPTOPIC"] >> b;
    if (b) {
        ci->Extend<bool>("KEEPTOPIC");
    }
    b = false;
    data["extensible:SIGNKICK"] >> b;
    if (b) {
        ci->Extend<bool>("SIGNKICK");
    }
    b = false;
    data["extensible:SIGNKICK_LEVEL"] >> b;
    if (b) {
        ci->Extend<bool>("SIGNKICK_LEVEL");
    }
    /* end compat */

    return ci;
}


void ChannelInfo::SetFounder(NickCore *nc) {
    if (this->founder) {
        --this->founder->channelcount;
        this->founder->RemoveChannelReference(this);
    }

    this->founder = nc;

    if (this->founder) {
        ++this->founder->channelcount;
        this->founder->AddChannelReference(this);
    }
}

NickCore *ChannelInfo::GetFounder() const {
    return this->founder;
}

void ChannelInfo::SetSuccessor(NickCore *nc) {
    if (this->successor) {
        this->successor->RemoveChannelReference(this);
    }
    this->successor = nc;
    if (this->successor) {
        this->successor->AddChannelReference(this);
    }
}

NickCore *ChannelInfo::GetSuccessor() const {
    return this->successor;
}

BotInfo *ChannelInfo::WhoSends() const {
    if (this && this->bi) {
        return this->bi;
    }

    BotInfo *ChanServ = Config->GetClient("ChanServ");
    if (ChanServ) {
        return ChanServ;
    }

    if (!BotListByNick->empty()) {
        return BotListByNick->begin()->second;
    }

    return NULL;
}

void ChannelInfo::AddAccess(ChanAccess *taccess) {
    this->access->push_back(taccess);
}

ChanAccess *ChannelInfo::GetAccess(unsigned index) const {
    if (this->access->empty() || index >= this->access->size()) {
        return NULL;
    }

    ChanAccess *acc = (*this->access)[index];
    acc->QueueUpdate();
    return acc;
}

static void FindMatchesRecurse(ChannelInfo *ci, const User *u,
                               const NickCore *account, unsigned int depth,
                               std::vector<ChanAccess::Path> &paths, ChanAccess::Path &path) {
    if (depth > ChanAccess::MAX_DEPTH) {
        return;
    }

    for (unsigned int i = 0; i < ci->GetAccessCount(); ++i) {
        ChanAccess *a = ci->GetAccess(i);
        ChannelInfo *next = NULL;

        if (a->Matches(u, account, next)) {
            ChanAccess::Path next_path = path;
            next_path.push_back(a);

            paths.push_back(next_path);
        } else if (next) {
            ChanAccess::Path next_path = path;
            next_path.push_back(a);

            FindMatchesRecurse(next, u, account, depth + 1, paths, next_path);
        }
    }
}

static void FindMatches(AccessGroup &group, ChannelInfo *ci, const User *u,
                        const NickCore *account) {
    ChanAccess::Path path;
    FindMatchesRecurse(ci, u, account, 0, group.paths, path);
}

AccessGroup ChannelInfo::AccessFor(const User *u, bool updateLastUsed) {
    AccessGroup group;

    if (u == NULL) {
        return group;
    }

    const NickCore *nc = u->Account();
    if (nc == NULL && !this->HasExt("NS_SECURE") && u->IsRecognized()) {
        const NickAlias *na = NickAlias::Find(u->nick);
        if (na != NULL) {
            nc = na->nc;
        }
    }

    group.super_admin = u->super_admin;
    group.founder = IsFounder(u, this);
    group.ci = this;
    group.nc = nc;

    FindMatches(group, this, u, u->Account());

    if (group.founder || !group.paths.empty()) {
        if (updateLastUsed) {
            this->last_used = Anope::CurTime;
        }

        for (unsigned i = 0; i < group.paths.size(); ++i) {
            ChanAccess::Path &p = group.paths[i];

            for (unsigned int j = 0; j < p.size(); ++j) {
                p[j]->last_seen = Anope::CurTime;
            }
        }
    }

    return group;
}

AccessGroup ChannelInfo::AccessFor(const NickCore *nc, bool updateLastUsed) {
    AccessGroup group;

    group.founder = (this->founder && this->founder == nc);
    group.ci = this;
    group.nc = nc;

    FindMatches(group, this, NULL, nc);

    if (group.founder || !group.paths.empty())
        if (updateLastUsed) {
            this->last_used = Anope::CurTime;
        }

    /* don't update access last seen here, this isn't the user requesting access */

    return group;
}

unsigned ChannelInfo::GetAccessCount() const {
    return this->access->size();
}

static unsigned int GetDeepAccessCount(const ChannelInfo *ci,
                                       std::set<const ChannelInfo *> &seen, unsigned int depth) {
    if (depth > ChanAccess::MAX_DEPTH || seen.count(ci)) {
        return 0;
    }
    seen.insert(ci);

    unsigned int total = 0;

    for (unsigned int i = 0; i < ci->GetAccessCount(); ++i) {
        ChanAccess::Path path;
        ChanAccess *a = ci->GetAccess(i);
        ChannelInfo *next = NULL;

        a->Matches(NULL, NULL, next);
        ++total;

        if (next) {
            total += GetDeepAccessCount(ci, seen, depth + 1);
        }
    }

    return total;
}

unsigned ChannelInfo::GetDeepAccessCount() const {
    std::set<const ChannelInfo *> seen;
    return ::GetDeepAccessCount(this, seen, 0);
}

ChanAccess *ChannelInfo::EraseAccess(unsigned index) {
    if (this->access->empty() || index >= this->access->size()) {
        return NULL;
    }

    ChanAccess *ca = this->access->at(index);
    this->access->erase(this->access->begin() + index);
    return ca;
}

void ChannelInfo::ClearAccess() {
    for (unsigned i = this->access->size(); i > 0; --i) {
        delete this->GetAccess(i - 1);
    }
}

AutoKick *ChannelInfo::AddAkick(const Anope::string &user, NickCore *akicknc,
                                const Anope::string &reason, time_t t, time_t lu) {
    AutoKick *autokick = new AutoKick();
    autokick->ci = this;
    autokick->nc = akicknc;
    autokick->reason = reason;
    autokick->creator = user;
    autokick->addtime = t;
    autokick->last_used = lu;

    this->akick->push_back(autokick);

    akicknc->AddChannelReference(this);

    return autokick;
}

AutoKick *ChannelInfo::AddAkick(const Anope::string &user,
                                const Anope::string &mask, const Anope::string &reason, time_t t, time_t lu) {
    AutoKick *autokick = new AutoKick();
    autokick->ci = this;
    autokick->mask = mask;
    autokick->nc = NULL;
    autokick->reason = reason;
    autokick->creator = user;
    autokick->addtime = t;
    autokick->last_used = lu;

    this->akick->push_back(autokick);

    return autokick;
}

AutoKick *ChannelInfo::GetAkick(unsigned index) const {
    if (this->akick->empty() || index >= this->akick->size()) {
        return NULL;
    }

    AutoKick *ak = (*this->akick)[index];
    ak->QueueUpdate();
    return ak;
}

unsigned ChannelInfo::GetAkickCount() const {
    return this->akick->size();
}

void ChannelInfo::EraseAkick(unsigned index) {
    if (this->akick->empty() || index >= this->akick->size()) {
        return;
    }

    delete this->GetAkick(index);
}

void ChannelInfo::ClearAkick() {
    while (!this->akick->empty()) {
        delete this->akick->back();
    }
}

const Anope::map<int16_t> &ChannelInfo::GetLevelEntries() {
    return this->levels;
}

int16_t ChannelInfo::GetLevel(const Anope::string &priv) const {
    if (PrivilegeManager::FindPrivilege(priv) == NULL) {
        Log(LOG_DEBUG) << "Unknown privilege " + priv;
        return ACCESS_INVALID;
    }

    Anope::map<int16_t>::const_iterator it = this->levels.find(priv);
    if (it == this->levels.end()) {
        return 0;
    }
    return it->second;
}

void ChannelInfo::SetLevel(const Anope::string &priv, int16_t level) {
    if (PrivilegeManager::FindPrivilege(priv) == NULL) {
        Log(LOG_DEBUG) << "Unknown privilege " + priv;
        return;
    }

    this->levels[priv] = level;
}

void ChannelInfo::RemoveLevel(const Anope::string &priv) {
    this->levels.erase(priv);
}

void ChannelInfo::ClearLevels() {
    this->levels.clear();
}

Anope::string ChannelInfo::GetIdealBan(User *u) const {
    int bt = this ? this->bantype : -1;
    switch (bt) {
    case 0:
        return "*!" + u->GetVIdent() + "@" + u->GetDisplayedHost();
    case 1:
        if (u->GetVIdent()[0] == '~') {
            return "*!*" + u->GetVIdent() + "@" + u->GetDisplayedHost();
        } else {
            return "*!" + u->GetVIdent() + "@" + u->GetDisplayedHost();
        }
    case 3:
        return "*!" + u->Mask();
    case 2:
    default:
        return "*!*@" + u->GetDisplayedHost();
    }
}

ChannelInfo* ChannelInfo::Find(const Anope::string &name) {
    registered_channel_map::const_iterator it = RegisteredChannelList->find(name);
    if (it != RegisteredChannelList->end()) {
        it->second->QueueUpdate();
        return it->second;
    }

    return NULL;
}

bool IsFounder(const User *user, const ChannelInfo *ci) {
    if (!user || !ci) {
        return false;
    }

    if (user->super_admin) {
        return true;
    }

    if (user->Account() && user->Account() == ci->GetFounder()) {
        return true;
    }

    return false;
}


void ChannelInfo::AddChannelReference(const Anope::string &what) {
    ++references[what];
}

void ChannelInfo::RemoveChannelReference(const Anope::string &what) {
    int &i = references[what];
    if (--i <= 0) {
        references.erase(what);
    }
}

void ChannelInfo::GetChannelReferences(std::deque<Anope::string> &chans) {
    chans.clear();
    for (Anope::map<int>::iterator it = references.begin(); it != references.end();
            ++it) {
        chans.push_back(it->first);
    }
}
