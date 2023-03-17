/* MemoServ functions.
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
#include "service.h"
#include "memo.h"
#include "users.h"
#include "account.h"
#include "regchannel.h"

Memo::Memo() : Serializable("Memo") {
    mi = NULL;
    unread = receipt = false;
}

Memo::~Memo() {
    if (mi) {
        std::vector<Memo *>::iterator it = std::find(mi->memos->begin(),
                                           mi->memos->end(), this);

        if (it != mi->memos->end()) {
            mi->memos->erase(it);
        }
    }
}

void Memo::Serialize(Serialize::Data &data) const {
    data["owner"] << this->owner;
    data.SetType("time", Serialize::Data::DT_INT);
    data["time"] << this->time;
    data["sender"] << this->sender;
    data["text"] << this->text;
    data["unread"] << this->unread;
    data["receipt"] << this->receipt;
}

Serializable* Memo::Unserialize(Serializable *obj, Serialize::Data &data) {
    Anope::string owner;

    data["owner"] >> owner;

    bool ischan;
    MemoInfo *mi = MemoInfo::GetMemoInfo(owner, ischan);
    if (!mi) {
        return NULL;
    }

    Memo *m;
    if (obj) {
        m = anope_dynamic_static_cast<Memo *>(obj);
    } else {
        m = new Memo();
        m->mi = mi;
    }

    m->owner = owner;
    data["time"] >> m->time;
    data["sender"] >> m->sender;
    data["text"] >> m->text;
    data["unread"] >> m->unread;
    data["receipt"] >> m->receipt;

    if (obj == NULL) {
        mi->memos->push_back(m);
    }
    return m;
}

MemoInfo::MemoInfo() : memomax(0), memos("Memo") {
}

Memo *MemoInfo::GetMemo(unsigned index) const {
    if (index >= this->memos->size()) {
        return NULL;
    }
    Memo *m = (*memos)[index];
    m->QueueUpdate();
    return m;
}

unsigned MemoInfo::GetIndex(Memo *m) const {
    for (unsigned i = 0; i < this->memos->size(); ++i)
        if (this->GetMemo(i) == m) {
            return i;
        }
    return -1;
}

void MemoInfo::Del(unsigned index) {
    if (index >= this->memos->size()) {
        return;
    }

    Memo *m = this->GetMemo(index);

    std::vector<Memo *>::iterator it = std::find(memos->begin(), memos->end(), m);
    if (it != memos->end()) {
        memos->erase(it);
    }

    delete m;
}

bool MemoInfo::HasIgnore(User *u) {
    for (unsigned i = 0; i < this->ignores.size(); ++i)
        if (u->nick.equals_ci(this->ignores[i]) || (u->Account()
                && u->Account()->display.equals_ci(this->ignores[i]))
                || Anope::Match(u->GetMask(), Anope::string(this->ignores[i]))) {
            return true;
        }
    return false;
}

MemoInfo *MemoInfo::GetMemoInfo(const Anope::string &target, bool &ischan) {
    if (!target.empty() && target[0] == '#') {
        ischan = true;
        ChannelInfo *ci = ChannelInfo::Find(target);
        if (ci != NULL) {
            return &ci->memos;
        }
    } else {
        ischan = false;
        NickAlias *na = NickAlias::Find(target);
        if (na != NULL) {
            return &na->nc->memos;
        }
    }

    return NULL;
}
