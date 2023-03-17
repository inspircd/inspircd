/* Mode support
 *
 * (C) 2008-2011 Adam <Adam@anope.org>
 * (C) 2008-2023 Anope Team <team@anope.org>
 *
 * Please read COPYING and README for further details.
 */

#include "services.h"
#include "modules.h"
#include "config.h"
#include "sockets.h"
#include "protocol.h"
#include "channels.h"
#include "uplink.h"

struct StackerInfo;

/* List of pairs of user/channels and their stacker info */
static std::map<User *, StackerInfo *> UserStackerObjects;
static std::map<Channel *, StackerInfo *> ChannelStackerObjects;

/* Array of all modes Anope knows about.*/
static std::vector<ChannelMode *> ChannelModes;
static std::vector<UserMode *> UserModes;

/* Modes are in this array are at position
 * modechar. Additionally, status modes are in this array (again) at statuschar.
 */
static std::vector<ChannelMode *> ChannelModesIdx;
static std::vector<UserMode *> UserModesIdx;

static std::map<Anope::string, ChannelMode *> ChannelModesByName;
static std::map<Anope::string, UserMode *> UserModesByName;

/* Sorted by status */
static std::vector<ChannelModeStatus *> ChannelModesByStatus;

/* Number of generic modes we support */
unsigned ModeManager::GenericChannelModes = 0,
                      ModeManager::GenericUserModes = 0;

struct StackerInfo {
    /* Modes to be added */
    std::list<std::pair<Mode *, Anope::string> > AddModes;
    /* Modes to be deleted */
    std::list<std::pair<Mode *, Anope::string> > DelModes;
    /* Bot this is sent from */
    BotInfo *bi;

    StackerInfo() : bi(NULL) { }

    /** Add a mode to this object
     * @param mode The mode
     * @param set true if setting, false if unsetting
     * @param param The param for the mode
     */
    void AddMode(Mode *mode, bool set, const Anope::string &param);
};

ChannelStatus::ChannelStatus() {
}

ChannelStatus::ChannelStatus(const Anope::string &m) : modes(m) {
}

void ChannelStatus::AddMode(char c) {
    if (modes.find(c) == Anope::string::npos) {
        modes.append(c);
    }
}

void ChannelStatus::DelMode(char c) {
    modes = modes.replace_all_cs(c, "");
}

bool ChannelStatus::HasMode(char c) const {
    return modes.find(c) != Anope::string::npos;
}

bool ChannelStatus::Empty() const {
    return modes.empty();
}

void ChannelStatus::Clear() {
    modes.clear();
}

const Anope::string &ChannelStatus::Modes() const {
    return modes;
}

Anope::string ChannelStatus::BuildModePrefixList() const {
    Anope::string ret;

    for (size_t i = 0; i < modes.length(); ++i) {
        ChannelMode *cm = ModeManager::FindChannelModeByChar(modes[i]);
        if (cm != NULL && cm->type == MODE_STATUS) {
            ChannelModeStatus *cms = anope_dynamic_static_cast<ChannelModeStatus *>(cm);
            ret += cms->symbol;
        }
    }

    return ret;
}

Mode::Mode(const Anope::string &mname, ModeClass mcl, char mch,
           ModeType mt) : name(mname), mclass(mcl), mchar(mch), type(mt) {
}

Mode::~Mode() {
}

bool Mode::CanSet(User *u) const {
    return true;
}

UserMode::UserMode(const Anope::string &un, char mch) : Mode(un, MC_USER, mch,
            MODE_REGULAR) {
}

UserModeParam::UserModeParam(const Anope::string &un, char mch) : UserMode(un,
            mch) {
    this->type = MODE_PARAM;
}

ChannelMode::ChannelMode(const Anope::string &cm, char mch) : Mode(cm,
            MC_CHANNEL, mch, MODE_REGULAR) {
}

bool ChannelMode::CanSet(User *u) const {
    EventReturn MOD_RESULT;
    FOREACH_RESULT(OnCanSet, MOD_RESULT, (u, this));
    return MOD_RESULT != EVENT_STOP;
}

ChannelMode *ChannelMode::Wrap(Anope::string &param) {
    return this;
}

ChannelMode *ChannelMode::Unwrap(Anope::string &param) {
    for (unsigned i = 0; i < listeners.size(); ++i) {
        ChannelMode *cm = listeners[i]->Unwrap(this, param);
        if (cm != this) {
            return cm;
        }
    }

    return this;
}

ChannelMode *ChannelMode::Unwrap(ChannelMode *, Anope::string &param) {
    throw CoreException("Unwrap in channel mode");
}

ChannelModeList::ChannelModeList(const Anope::string &cm,
                                 char mch) : ChannelMode(cm, mch) {
    this->type = MODE_LIST;
}

bool ChannelModeList::IsValid(Anope::string &mask) const {
    if (name == "BAN" || name == "EXCEPT" || name == "INVITEOVERRIDE") {
        mask = IRCD->NormalizeMask(mask);
    }
    return true;
}

ChannelModeParam::ChannelModeParam(const Anope::string &cm, char mch,
                                   bool ma) : ChannelMode(cm, mch), minus_no_arg(ma) {
    this->type = MODE_PARAM;
}

ChannelModeStatus::ChannelModeStatus(const Anope::string &mname, char modeChar,
                                     char msymbol, unsigned mlevel) : ChannelMode(mname, modeChar), symbol(msymbol),
    level(mlevel) {
    this->type = MODE_STATUS;
}

template<typename T>
ChannelModeVirtual<T>::ChannelModeVirtual(const Anope::string &mname,
        const Anope::string &basename) : T(mname, 0)
    , base(basename) {
    basech = ModeManager::FindChannelModeByName(base);
    if (basech) {
        basech->listeners.push_back(this);
    }
}

template<typename T>
ChannelModeVirtual<T>::~ChannelModeVirtual() {
    if (basech) {
        std::vector<ChannelMode *>::iterator it = std::find(basech->listeners.begin(),
                basech->listeners.end(), this);
        if (it != basech->listeners.end()) {
            basech->listeners.erase(it);
        }
    }
}

template<typename T>
void ChannelModeVirtual<T>::Check() {
    if (basech == NULL) {
        basech = ModeManager::FindChannelModeByName(base);
        if (basech) {
            basech->listeners.push_back(this);
        }
    }
}

template<typename T>
ChannelMode *ChannelModeVirtual<T>::Wrap(Anope::string &param) {
    return basech;
}

template class ChannelModeVirtual<ChannelMode>;
template class ChannelModeVirtual<ChannelModeList>;

bool UserModeOperOnly::CanSet(User *u) const {
    return u && u->HasMode("OPER");
}

bool UserModeNoone::CanSet(User *u) const {
    return false;
}

bool ChannelModeKey::IsValid(Anope::string &value) const {
    if (!value.empty() && value.find(':') == Anope::string::npos
            && value.find(',') == Anope::string::npos) {
        return true;
    }

    return false;
}

bool ChannelModeOperOnly::CanSet(User *u) const {
    return u && u->HasMode("OPER");
}

bool ChannelModeNoone::CanSet(User *u) const {
    return false;
}

void StackerInfo::AddMode(Mode *mode, bool set, const Anope::string &param) {
    bool is_param = mode->type == MODE_PARAM;

    std::list<std::pair<Mode *, Anope::string> > *list, *otherlist;
    if (set) {
        list = &AddModes;
        otherlist = &DelModes;
    } else {
        list = &DelModes;
        otherlist = &AddModes;
    }

    /* Loop through the list and find if this mode is already on here */
    std::list<std::pair<Mode *, Anope::string > >::iterator it, it_end;
    for (it = list->begin(), it_end = list->end(); it != it_end; ++it) {
        /* The param must match too (can have multiple status or list modes), but
         * if it is a param mode it can match no matter what the param is
         */
        if (it->first == mode && (is_param || param.equals_cs(it->second))) {
            list->erase(it);
            /* It can only be on this list once */
            break;
        }
    }
    /* If the mode is on the other list, remove it from there (eg, we don't want +o-o Adam Adam) */
    for (it = otherlist->begin(), it_end = otherlist->end(); it != it_end; ++it) {
        /* The param must match too (can have multiple status or list modes), but
         * if it is a param mode it can match no matter what the param is
         */
        if (it->first == mode && (is_param || param.equals_cs(it->second))) {
            otherlist->erase(it);
            return;
            /* Note that we return here because this is like setting + and - on the same mode within the same
             * cycle, no change is made. This causes no problems with something like - + and -, because after the
             * second mode change the list is empty, and the third mode change starts fresh.
             */
        }
    }

    /* Add this mode and its param to our list */
    list->push_back(std::make_pair(mode, param));
}

static class ModePipe : public Pipe {
  public:
    void OnNotify() {
        ModeManager::ProcessModes();
    }
} *modePipe;

/** Get the stacker info for an item, if one doesn't exist it is created
 * @param Item The user/channel etc
 * @return The stacker info
 */
template<typename List, typename Object>
static StackerInfo *GetInfo(List &l, Object *o) {
    typename List::const_iterator it = l.find(o);
    if (it != l.end()) {
        return it->second;
    }

    StackerInfo *s = new StackerInfo();
    l[o] = s;
    return s;
}

/** Build a list of mode strings to send to the IRCd from the mode stacker
 * @param info The stacker info for a channel or user
 * @return a list of strings
 */
static std::list<Anope::string> BuildModeStrings(StackerInfo *info) {
    std::list<Anope::string> ret;
    std::list<std::pair<Mode *, Anope::string> >::iterator it, it_end;
    Anope::string buf = "+", parambuf;
    unsigned NModes = 0;

    for (it = info->AddModes.begin(), it_end = info->AddModes.end(); it != it_end;
            ++it) {
        if (++NModes > IRCD->MaxModes
                || (buf.length() + parambuf.length() > IRCD->MaxLine -
                    100)) { // Leave room for command, channel, etc
            ret.push_back(buf + parambuf);
            buf = "+";
            parambuf.clear();
            NModes = 1;
        }

        buf += it->first->mchar;

        if (!it->second.empty()) {
            parambuf += " " + it->second;
        }
    }

    if (buf[buf.length() - 1] == '+') {
        buf.erase(buf.length() - 1);
    }

    buf += "-";
    for (it = info->DelModes.begin(), it_end = info->DelModes.end(); it != it_end;
            ++it) {
        if (++NModes > IRCD->MaxModes
                || (buf.length() + parambuf.length() > IRCD->MaxLine -
                    100)) { // Leave room for command, channel, etc
            ret.push_back(buf + parambuf);
            buf = "-";
            parambuf.clear();
            NModes = 1;
        }

        buf += it->first->mchar;

        if (!it->second.empty()) {
            parambuf += " " + it->second;
        }
    }

    if (buf[buf.length() - 1] == '-') {
        buf.erase(buf.length() - 1);
    }

    if (!buf.empty()) {
        ret.push_back(buf + parambuf);
    }

    return ret;
}

bool ModeManager::AddUserMode(UserMode *um) {
    if (ModeManager::FindUserModeByChar(um->mchar) != NULL) {
        return false;
    }
    if (ModeManager::FindUserModeByName(um->name) != NULL) {
        return false;
    }

    if (um->name.empty()) {
        um->name = stringify(++GenericUserModes);
        Log() << "ModeManager: Added generic support for user mode " << um->mchar;
    }

    unsigned want = um->mchar;
    if (want >= UserModesIdx.size()) {
        UserModesIdx.resize(want + 1);
    }
    UserModesIdx[want] = um;

    UserModesByName[um->name] = um;

    UserModes.push_back(um);

    FOREACH_MOD(OnUserModeAdd, (um));

    return true;
}

bool ModeManager::AddChannelMode(ChannelMode *cm) {
    if (cm->mchar && ModeManager::FindChannelModeByChar(cm->mchar) != NULL) {
        return false;
    }
    if (ModeManager::FindChannelModeByName(cm->name) != NULL) {
        return false;
    }

    if (cm->name.empty()) {
        cm->name = stringify(++GenericChannelModes);
        Log() << "ModeManager: Added generic support for channel mode " << cm->mchar;
    }

    if (cm->mchar) {
        unsigned want = cm->mchar;
        if (want >= ChannelModesIdx.size()) {
            ChannelModesIdx.resize(want + 1);
        }
        ChannelModesIdx[want] = cm;
    }

    if (cm->type == MODE_STATUS) {
        ChannelModeStatus *cms = anope_dynamic_static_cast<ChannelModeStatus *>(cm);
        unsigned want = cms->symbol;
        if (want >= ChannelModesIdx.size()) {
            ChannelModesIdx.resize(want + 1);
        }
        ChannelModesIdx[want] = cms;

        RebuildStatusModes();
    }

    ChannelModesByName[cm->name] = cm;

    ChannelModes.push_back(cm);

    FOREACH_MOD(OnChannelModeAdd, (cm));

    for (unsigned int i = 0; i < ChannelModes.size(); ++i) {
        ChannelModes[i]->Check();
    }

    return true;
}

void ModeManager::RemoveUserMode(UserMode *um) {
    if (!um) {
        return;
    }

    unsigned want = um->mchar;
    if (want >= UserModesIdx.size()) {
        return;
    }

    if (UserModesIdx[want] != um) {
        return;
    }

    UserModesIdx[want] = NULL;

    UserModesByName.erase(um->name);

    std::vector<UserMode *>::iterator it = std::find(UserModes.begin(),
                                           UserModes.end(), um);
    if (it != UserModes.end()) {
        UserModes.erase(it);
    }

    StackerDel(um);
}

void ModeManager::RemoveChannelMode(ChannelMode *cm) {
    if (!cm) {
        return;
    }

    if (cm->mchar) {
        unsigned want = cm->mchar;
        if (want >= ChannelModesIdx.size()) {
            return;
        }

        if (ChannelModesIdx[want] != cm) {
            return;
        }

        ChannelModesIdx[want] = NULL;
    }

    if (cm->type == MODE_STATUS) {
        ChannelModeStatus *cms = anope_dynamic_static_cast<ChannelModeStatus *>(cm);
        unsigned want = cms->symbol;

        if (want >= ChannelModesIdx.size()) {
            return;
        }

        if (ChannelModesIdx[want] != cm) {
            return;
        }

        ChannelModesIdx[want] = NULL;

        RebuildStatusModes();
    }

    ChannelModesByName.erase(cm->name);

    std::vector<ChannelMode *>::iterator it = std::find(ChannelModes.begin(),
            ChannelModes.end(), cm);
    if (it != ChannelModes.end()) {
        ChannelModes.erase(it);
    }

    StackerDel(cm);
}

ChannelMode *ModeManager::FindChannelModeByChar(char mode) {
    unsigned want = mode;
    if (want >= ChannelModesIdx.size()) {
        return NULL;
    }

    return ChannelModesIdx[want];
}

UserMode *ModeManager::FindUserModeByChar(char mode) {
    unsigned want = mode;
    if (want >= UserModesIdx.size()) {
        return NULL;
    }

    return UserModesIdx[want];
}

ChannelMode *ModeManager::FindChannelModeByName(const Anope::string &name) {
    std::map<Anope::string, ChannelMode *>::iterator it = ChannelModesByName.find(
                name);
    if (it != ChannelModesByName.end()) {
        return it->second;
    }
    return NULL;
}

UserMode *ModeManager::FindUserModeByName(const Anope::string &name) {
    std::map<Anope::string, UserMode *>::iterator it = UserModesByName.find(name);
    if (it != UserModesByName.end()) {
        return it->second;
    }
    return NULL;
}

char ModeManager::GetStatusChar(char value) {
    unsigned want = value;
    if (want >= ChannelModesIdx.size()) {
        return 0;
    }

    ChannelMode *cm = ChannelModesIdx[want];
    if (cm == NULL || cm->type != MODE_STATUS || cm->mchar == value) {
        return 0;
    }

    return cm->mchar;
}

const std::vector<ChannelMode *> &ModeManager::GetChannelModes() {
    return ChannelModes;
}

const std::vector<UserMode *> &ModeManager::GetUserModes() {
    return UserModes;
}

const std::vector<ChannelModeStatus *>
&ModeManager::GetStatusChannelModesByRank() {
    return ChannelModesByStatus;
}

static struct StatusSort {
    bool operator()(ChannelModeStatus *cm1, ChannelModeStatus *cm2) const {
        return cm1->level > cm2->level;
    }
} statuscmp;

void ModeManager::RebuildStatusModes() {
    ChannelModesByStatus.clear();
    for (unsigned j = 0; j < ChannelModesIdx.size(); ++j) {
        ChannelMode *cm = ChannelModesIdx[j];

        if (cm && cm->type == MODE_STATUS
                && std::find(ChannelModesByStatus.begin(), ChannelModesByStatus.end(),
                             cm) == ChannelModesByStatus.end()) {
            ChannelModesByStatus.push_back(anope_dynamic_static_cast<ChannelModeStatus *>
                                           (cm));
        }
    }
    std::sort(ChannelModesByStatus.begin(), ChannelModesByStatus.end(), statuscmp);
}

void ModeManager::StackerAdd(BotInfo *bi, Channel *c, ChannelMode *cm, bool Set,
                             const Anope::string &Param) {
    StackerInfo *s = GetInfo(ChannelStackerObjects, c);
    s->AddMode(cm, Set, Param);
    if (bi) {
        s->bi = bi;
    } else {
        s->bi = c->ci->WhoSends();
    }

    if (!modePipe) {
        modePipe = new ModePipe();
    }
    modePipe->Notify();
}

void ModeManager::StackerAdd(BotInfo *bi, User *u, UserMode *um, bool Set,
                             const Anope::string &Param) {
    StackerInfo *s = GetInfo(UserStackerObjects, u);
    s->AddMode(um, Set, Param);
    if (bi) {
        s->bi = bi;
    }

    if (!modePipe) {
        modePipe = new ModePipe();
    }
    modePipe->Notify();
}

void ModeManager::ProcessModes() {
    if (!UserStackerObjects.empty()) {
        for (std::map<User *, StackerInfo *>::const_iterator it =
                    UserStackerObjects.begin(), it_end = UserStackerObjects.end(); it != it_end;
                ++it) {
            User *u = it->first;
            StackerInfo *s = it->second;

            std::list<Anope::string> ModeStrings = BuildModeStrings(s);
            for (std::list<Anope::string>::iterator lit = ModeStrings.begin(),
                    lit_end = ModeStrings.end(); lit != lit_end; ++lit) {
                IRCD->SendMode(s->bi, u, "%s", lit->c_str());
            }
            delete it->second;
        }
        UserStackerObjects.clear();
    }

    if (!ChannelStackerObjects.empty()) {
        for (std::map<Channel *, StackerInfo *>::const_iterator it =
                    ChannelStackerObjects.begin(), it_end = ChannelStackerObjects.end();
                it != it_end; ++it) {
            Channel *c = it->first;
            StackerInfo *s = it->second;

            std::list<Anope::string> ModeStrings = BuildModeStrings(s);
            for (std::list<Anope::string>::iterator lit = ModeStrings.begin(),
                    lit_end = ModeStrings.end(); lit != lit_end; ++lit) {
                IRCD->SendMode(s->bi, c, "%s", lit->c_str());
            }
            delete it->second;
        }
        ChannelStackerObjects.clear();
    }
}

template<typename T>
static void StackerDel(std::map<T *, StackerInfo *> &map, T *obj) {
    typename std::map<T *, StackerInfo *>::iterator it = map.find(obj);
    if (it != map.end()) {
        StackerInfo *si = it->second;
        std::list<Anope::string> ModeStrings = BuildModeStrings(si);
        for (std::list<Anope::string>::iterator lit = ModeStrings.begin(),
                lit_end = ModeStrings.end(); lit != lit_end; ++lit) {
            IRCD->SendMode(si->bi, obj, "%s", lit->c_str());
        }

        delete si;
        map.erase(it);
    }
}

void ModeManager::StackerDel(User *u) {
    ::StackerDel(UserStackerObjects, u);
}

void ModeManager::StackerDel(Channel *c) {
    ::StackerDel(ChannelStackerObjects, c);
}

void ModeManager::StackerDel(Mode *m) {
    for (std::map<User *, StackerInfo *>::const_iterator it =
                UserStackerObjects.begin(), it_end = UserStackerObjects.end(); it != it_end;) {
        StackerInfo *si = it->second;
        ++it;

        for (std::list<std::pair<Mode *, Anope::string> >::iterator it2 =
                    si->AddModes.begin(), it2_end = si->AddModes.end(); it2 != it2_end;) {
            if (it2->first == m) {
                it2 = si->AddModes.erase(it2);
            } else {
                ++it2;
            }
        }

        for (std::list<std::pair<Mode *, Anope::string> >::iterator it2 =
                    si->DelModes.begin(), it2_end = si->DelModes.end(); it2 != it2_end;) {
            if (it2->first == m) {
                it2 = si->DelModes.erase(it2);
            } else {
                ++it2;
            }
        }
    }

    for (std::map<Channel *, StackerInfo *>::const_iterator it =
                ChannelStackerObjects.begin(), it_end = ChannelStackerObjects.end();
            it != it_end;) {
        StackerInfo *si = it->second;
        ++it;

        for (std::list<std::pair<Mode *, Anope::string> >::iterator it2 =
                    si->AddModes.begin(), it2_end = si->AddModes.end(); it2 != it2_end;) {
            if (it2->first == m) {
                it2 = si->AddModes.erase(it2);
            } else {
                ++it2;
            }
        }

        for (std::list<std::pair<Mode *, Anope::string> >::iterator it2 =
                    si->DelModes.begin(), it2_end = si->DelModes.end(); it2 != it2_end;) {
            if (it2->first == m) {
                it2 = si->DelModes.erase(it2);
            } else {
                ++it2;
            }
        }
    }
}

Entry::Entry(const Anope::string &m, const Anope::string &fh) : name(m),
    mask(fh), cidr_len(0), family(0) {
    Anope::string n, u, h;

    size_t at = fh.find('@');
    if (at != Anope::string::npos) {
        this->host = fh.substr(at + 1);

        const Anope::string &nu = fh.substr(0, at);

        size_t ex = nu.find('!');
        if (ex != Anope::string::npos) {
            this->user = nu.substr(ex + 1);
            this->nick = nu.substr(0, ex);
        } else {
            this->user = nu;
        }
    } else {
        if (fh.find('.') != Anope::string::npos
                || fh.find(':') != Anope::string::npos) {
            this->host = fh;
        } else {
            this->nick = fh;
        }
    }

    at = this->host.find('#');
    if (at != Anope::string::npos) {
        this->real = this->host.substr(at + 1);
        this->host = this->host.substr(0, at);
    }

    /* If the mask is all *'s it will match anything, so just clear it */
    if (this->nick.find_first_not_of("*") == Anope::string::npos) {
        this->nick.clear();
    }

    if (this->user.find_first_not_of("*") == Anope::string::npos) {
        this->user.clear();
    }

    if (this->host.find_first_not_of("*") == Anope::string::npos) {
        this->host.clear();
    } else {
        /* Host might be a CIDR range */
        size_t sl = this->host.find_last_of('/');
        if (sl != Anope::string::npos) {
            const Anope::string &cidr_ip = this->host.substr(0, sl),
                                 &cidr_range = this->host.substr(sl + 1);

            sockaddrs addr(cidr_ip);

            try {
                if (addr.valid() && cidr_range.is_pos_number_only()) {
                    this->cidr_len = convertTo<unsigned short>(cidr_range);

                    /* If we got here, cidr_len is a valid number. */

                    this->host = cidr_ip;
                    this->family = addr.family();

                    Log(LOG_DEBUG) << "Ban " << mask << " has cidr " << this->cidr_len;
                }
            } catch (const ConvertException &) { }
        }
    }

    if (this->real.find_first_not_of("*") == Anope::string::npos) {
        this->real.clear();
    }
}

const Anope::string Entry::GetMask() const {
    return this->mask;
}

const Anope::string Entry::GetNUHMask() const {
    Anope::string n = nick.empty() ? "*" : nick,
                  u = user.empty() ? "*" : user,
                  h = host.empty() ? "*" : host,
                  r = real.empty() ? "" : "#" + real,
                  c;
    switch (family) {
    case AF_INET:
        if (cidr_len <= 32) {
            c = "/" + stringify(cidr_len);
        }
        break;
    case AF_INET6:
        if (cidr_len <= 128) {
            c = "/" + stringify(cidr_len);
        }
        break;
    }

    return n + "!" + u + "@" + h + c + r;
}

bool Entry::Matches(User *u, bool full) const {
    /* First check if this mode has defined any matches (usually for extbans). */
    if (IRCD->IsExtbanValid(this->mask)) {
        ChannelMode *cm = ModeManager::FindChannelModeByName(this->name);
        if (cm != NULL && cm->type == MODE_LIST) {
            ChannelModeList *cml = anope_dynamic_static_cast<ChannelModeList *>(cm);
            if (cml->Matches(u, this)) {
                return true;
            }
        }
    }

    /* If the user's displayed host is their real host, then we can do a full match without
     * having to worry about exposing a user's IP
     */
    full |= u->GetDisplayedHost() == u->host;

    bool ret = true;

    if (!this->nick.empty() && !Anope::Match(u->nick, this->nick)) {
        ret = false;
    }

    if (!this->user.empty() && !Anope::Match(u->GetVIdent(), this->user) && (!full
            || !Anope::Match(u->GetIdent(), this->user))) {
        ret = false;
    }

    if (this->cidr_len && full) {
        try {
            if (!cidr(this->host, this->cidr_len).match(u->ip)) {
                ret = false;
            }
        } catch (const SocketException &) {
            ret = false;
        }
    } else if (!this->host.empty()
               && !Anope::Match(u->GetDisplayedHost(), this->host)
               && !Anope::Match(u->GetCloakedHost(), this->host) &&
               (!full || (!Anope::Match(u->host, this->host)
                          && !Anope::Match(u->ip.addr(), this->host)))) {
        ret = false;
    }

    if (!this->real.empty() && !Anope::Match(u->realname, this->real)) {
        ret = false;
    }

    return ret;
}
