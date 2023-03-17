/* BotServ core functions
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "module.h"
#include "modules/bs_badwords.h"

struct BadWordImpl : BadWord, Serializable {
    BadWordImpl() : Serializable("BadWord") { }
    ~BadWordImpl();

    void Serialize(Serialize::Data &data) const anope_override {
        data["ci"] << this->chan;
        data["word"] << this->word;
        data.SetType("type", Serialize::Data::DT_INT);
        data["type"] << this->type;
    }

    static Serializable* Unserialize(Serializable *obj, Serialize::Data &);
};

struct BadWordsImpl : BadWords {
    Serialize::Reference<ChannelInfo> ci;
    typedef std::vector<BadWordImpl *> list;
    Serialize::Checker<list> badwords;

    BadWordsImpl(Extensible *obj) : ci(anope_dynamic_static_cast<ChannelInfo *>
                                           (obj)), badwords("BadWord") { }

    ~BadWordsImpl();

    BadWord* AddBadWord(const Anope::string &word,
                        BadWordType type) anope_override {
        BadWordImpl *bw = new BadWordImpl();
        bw->chan = ci->name;
        bw->word = word;
        bw->type = type;

        this->badwords->push_back(bw);

        FOREACH_MOD(OnBadWordAdd, (ci, bw));

        return bw;
    }

    BadWord* GetBadWord(unsigned index) const anope_override {
        if (this->badwords->empty() || index >= this->badwords->size()) {
            return NULL;
        }

        BadWordImpl *bw = (*this->badwords)[index];
        bw->QueueUpdate();
        return bw;
    }

    unsigned GetBadWordCount() const anope_override {
        return this->badwords->size();
    }

    void EraseBadWord(unsigned index) anope_override {
        if (this->badwords->empty() || index >= this->badwords->size()) {
            return;
        }

        FOREACH_MOD(OnBadWordDel, (ci, (*this->badwords)[index]));

        delete this->badwords->at(index);
    }

    void ClearBadWords() anope_override {
        while (!this->badwords->empty()) {
            delete this->badwords->back();
        }
    }

    void Check() anope_override {
        if (this->badwords->empty()) {
            ci->Shrink<BadWords>("badwords");
        }
    }
};

BadWordsImpl::~BadWordsImpl() {
    for (list::iterator it = badwords->begin(); it != badwords->end();) {
        BadWord *bw = *it;
        ++it;
        delete bw;
    }
}

BadWordImpl::~BadWordImpl() {
    ChannelInfo *ci = ChannelInfo::Find(chan);
    if (ci) {
        BadWordsImpl *badwords = ci->GetExt<BadWordsImpl>("badwords");
        if (badwords) {
            BadWordsImpl::list::iterator it = std::find(badwords->badwords->begin(),
                                              badwords->badwords->end(), this);
            if (it != badwords->badwords->end()) {
                badwords->badwords->erase(it);
            }
        }
    }
}

Serializable* BadWordImpl::Unserialize(Serializable *obj,
                                       Serialize::Data &data) {
    Anope::string sci, sword;

    data["ci"] >> sci;
    data["word"] >> sword;

    ChannelInfo *ci = ChannelInfo::Find(sci);
    if (!ci) {
        return NULL;
    }

    unsigned int n;
    data["type"] >> n;

    BadWordImpl *bw;
    if (obj) {
        bw = anope_dynamic_static_cast<BadWordImpl *>(obj);
    } else {
        bw = new BadWordImpl();
    }
    bw->chan = sci;
    bw->word = sword;
    bw->type = static_cast<BadWordType>(n);

    BadWordsImpl *bws = ci->Require<BadWordsImpl>("badwords");
    if (!obj) {
        bws->badwords->push_back(bw);
    }

    return bw;
}

class BadwordsDelCallback : public NumberList {
    CommandSource &source;
    ChannelInfo *ci;
    BadWords *bw;
    Command *c;
    unsigned deleted;
    bool override;
  public:
    BadwordsDelCallback(CommandSource &_source, ChannelInfo *_ci, Command *_c,
                        const Anope::string &list) : NumberList(list, true), source(_source), ci(_ci),
        c(_c), deleted(0), override(false) {
        if (!source.AccessFor(ci).HasPriv("BADWORDS")
                && source.HasPriv("botserv/administration")) {
            this->override = true;
        }
        bw = ci->Require<BadWords>("badwords");
    }

    ~BadwordsDelCallback() {
        if (!deleted) {
            source.Reply(_("No matching entries on %s bad words list."), ci->name.c_str());
        } else if (deleted == 1) {
            source.Reply(_("Deleted 1 entry from %s bad words list."), ci->name.c_str());
        } else {
            source.Reply(_("Deleted %d entries from %s bad words list."), deleted,
                         ci->name.c_str());
        }
    }

    void HandleNumber(unsigned Number) anope_override {
        if (!bw || !Number || Number > bw->GetBadWordCount()) {
            return;
        }

        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, c, ci) << "DEL " << bw->GetBadWord(Number - 1)->word;
        ++deleted;
        bw->EraseBadWord(Number - 1);
    }
};

class CommandBSBadwords : public Command {
  private:
    void DoList(CommandSource &source, ChannelInfo *ci, const Anope::string &word) {
        bool override = !source.AccessFor(ci).HasPriv("BADWORDS");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci) << "LIST";
        ListFormatter list(source.GetAccount());
        BadWords *bw = ci->GetExt<BadWords>("badwords");

        list.AddColumn(_("Number")).AddColumn(_("Word")).AddColumn(_("Type"));

        if (!bw || !bw->GetBadWordCount()) {
            source.Reply(_("%s bad words list is empty."), ci->name.c_str());
            return;
        } else if (!word.empty()
                   && word.find_first_not_of("1234567890,-") == Anope::string::npos) {
            class BadwordsListCallback : public NumberList {
                ListFormatter &list;
                BadWords *bw;
              public:
                BadwordsListCallback(ListFormatter &_list, BadWords *_bw,
                                     const Anope::string &numlist) : NumberList(numlist, false), list(_list),
                    bw(_bw) {
                }

                void HandleNumber(unsigned Number) anope_override {
                    if (!Number || Number > bw->GetBadWordCount()) {
                        return;
                    }

                    const BadWord *b = bw->GetBadWord(Number - 1);
                    ListFormatter::ListEntry entry;
                    entry["Number"] = stringify(Number);
                    entry["Word"] = b->word;
                    entry["Type"] = b->type == BW_SINGLE ? "(SINGLE)" : (b->type == BW_START ? "(START)" : (b->type == BW_END ? "(END)" : ""));
                    this->list.AddEntry(entry);
                }
            }
            nl_list(list, bw, word);
            nl_list.Process();
        } else {
            for (unsigned i = 0, end = bw->GetBadWordCount(); i < end; ++i) {
                const BadWord *b = bw->GetBadWord(i);

                if (!word.empty() && !Anope::Match(b->word, word)) {
                    continue;
                }

                ListFormatter::ListEntry entry;
                entry["Number"] = stringify(i + 1);
                entry["Word"] = b->word;
                entry["Type"] = b->type == BW_SINGLE ? "(SINGLE)" : (b->type == BW_START ?
                                "(START)" : (b->type == BW_END ? "(END)" : ""));
                list.AddEntry(entry);
            }
        }

        if (list.IsEmpty()) {
            source.Reply(_("No matching entries on %s bad words list."), ci->name.c_str());
        } else {
            std::vector<Anope::string> replies;
            list.Process(replies);

            source.Reply(_("Bad words list for %s:"), ci->name.c_str());

            for (unsigned i = 0; i < replies.size(); ++i) {
                source.Reply(replies[i]);
            }

            source.Reply(_("End of bad words list."));
        }
    }

    void DoAdd(CommandSource &source, ChannelInfo *ci, const Anope::string &word) {
        size_t pos = word.rfind(' ');
        BadWordType bwtype = BW_ANY;
        Anope::string realword = word;
        BadWords *badwords = ci->Require<BadWords>("badwords");

        if (pos != Anope::string::npos) {
            Anope::string opt = word.substr(pos + 1);
            if (!opt.empty()) {
                if (opt.equals_ci("SINGLE")) {
                    bwtype = BW_SINGLE;
                } else if (opt.equals_ci("START")) {
                    bwtype = BW_START;
                } else if (opt.equals_ci("END")) {
                    bwtype = BW_END;
                }
            }
            realword = word.substr(0, pos);
        }

        unsigned badwordsmax = Config->GetModule(
                                   this->module)->Get<unsigned>("badwordsmax");
        if (badwords->GetBadWordCount() >= badwordsmax) {
            source.Reply(_("Sorry, you can only have %d bad words entries on a channel."),
                         badwordsmax);
            return;
        }

        bool casesensitive = Config->GetModule(
                                 this->module)->Get<bool>("casesensitive");

        for (unsigned i = 0, end = badwords->GetBadWordCount(); i < end; ++i) {
            const BadWord *bw = badwords->GetBadWord(i);

            if ((casesensitive && realword.equals_cs(bw->word)) || (!casesensitive
                    && realword.equals_ci(bw->word))) {
                source.Reply(_("\002%s\002 already exists in %s bad words list."),
                             bw->word.c_str(), ci->name.c_str());
                return;
            }
        }

        bool override = !source.AccessFor(ci).HasPriv("BADWORDS");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
            ci) << "ADD " << realword;
        badwords->AddBadWord(realword, bwtype);

        source.Reply(_("\002%s\002 added to %s bad words list."), realword.c_str(),
                     ci->name.c_str());
    }

    void DoDelete(CommandSource &source, ChannelInfo *ci,
                  const Anope::string &word) {
        BadWords *badwords = ci->GetExt<BadWords>("badwords");

        if (!badwords || !badwords->GetBadWordCount()) {
            source.Reply(_("%s bad words list is empty."), ci->name.c_str());
            return;
        }

        /* Special case: is it a number/list?  Only do search if it isn't. */
        if (!word.empty() && isdigit(word[0])
                && word.find_first_not_of("1234567890,-") == Anope::string::npos) {
            BadwordsDelCallback list(source, ci, this, word);
            list.Process();
        } else {
            unsigned i, end;
            const BadWord *badword;

            for (i = 0, end = badwords->GetBadWordCount(); i < end; ++i) {
                badword = badwords->GetBadWord(i);

                if (word.equals_ci(badword->word)) {
                    break;
                }
            }

            if (i == end) {
                source.Reply(_("\002%s\002 not found on %s bad words list."), word.c_str(),
                             ci->name.c_str());
                return;
            }

            bool override = !source.AccessFor(ci).HasPriv("BADWORDS");
            Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                ci) << "DEL " << badword->word;

            source.Reply(_("\002%s\002 deleted from %s bad words list."),
                         badword->word.c_str(), ci->name.c_str());

            badwords->EraseBadWord(i);
        }

        badwords->Check();
    }

    void DoClear(CommandSource &source, ChannelInfo *ci) {
        bool override = !source.AccessFor(ci).HasPriv("BADWORDS");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci) << "CLEAR";

        BadWords *badwords = ci->GetExt<BadWords>("badwords");
        if (badwords) {
            badwords->ClearBadWords();
        }
        source.Reply(_("Bad words list is now empty."));
    }

  public:
    CommandBSBadwords(Module *creator) : Command(creator, "botserv/badwords", 2,
                3) {
        this->SetDesc(_("Maintains the bad words list"));
        this->SetSyntax(
            _("\037channel\037 ADD \037word\037 [\037SINGLE\037 | \037START\037 | \037END\037]"));
        this->SetSyntax(
            _("\037channel\037 DEL {\037word\037 | \037entry-num\037 | \037list\037}"));
        this->SetSyntax(_("\037channel\037 LIST [\037mask\037 | \037list\037]"));
        this->SetSyntax(_("\037channel\037 CLEAR"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &cmd = params[1];
        const Anope::string &word = params.size() > 2 ? params[2] : "";
        bool need_args = cmd.equals_ci("LIST") || cmd.equals_ci("CLEAR");

        if (!need_args && word.empty()) {
            this->OnSyntaxError(source, cmd);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        if (!source.AccessFor(ci).HasPriv("BADWORDS") && !source.HasPriv("botserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, bad words list modification is temporarily disabled."));
            return;
        }

        if (cmd.equals_ci("ADD")) {
            return this->DoAdd(source, ci, word);
        } else if (cmd.equals_ci("DEL")) {
            return this->DoDelete(source, ci, word);
        } else if (cmd.equals_ci("LIST")) {
            return this->DoList(source, ci, word);
        } else if (cmd.equals_ci("CLEAR")) {
            return this->DoClear(source, ci);
        } else {
            this->OnSyntaxError(source, "");
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Maintains the \002bad words list\002 for a channel. The bad\n"
                       "words list determines which words are to be kicked\n"
                       "when the bad words kicker is enabled. For more information,\n"
                       "type \002%s%s HELP KICK %s\002.\n"
                       " \n"
                       "The \002ADD\002 command adds the given word to the\n"
                       "bad words list. If SINGLE is specified, a kick will be\n"
                       "done only if a user says the entire word. If START is\n"
                       "specified, a kick will be done if a user says a word\n"
                       "that starts with \037word\037. If END is specified, a kick\n"
                       "will be done if a user says a word that ends with\n"
                       "\037word\037. If you don't specify anything, a kick will\n"
                       "be issued every time \037word\037 is said by a user.\n"
                       " \n"), Config->StrictPrivmsg.c_str(), source.service->nick.c_str(), source.command.c_str());
        source.Reply(_("The \002DEL\002 command removes the given word from the\n"
                       "bad words list.  If a list of entry numbers is given, those\n"
                       "entries are deleted.  (See the example for LIST below.)\n"
                       " \n"
                       "The \002LIST\002 command displays the bad words list.  If\n"
                       "a wildcard mask is given, only those entries matching the\n"
                       "mask are displayed.  If a list of entry numbers is given,\n"
                       "only those entries are shown; for example:\n"
                       "   \002#channel LIST 2-5,7-9\002\n"
                       "      Lists bad words entries numbered 2 through 5 and\n"
                       "      7 through 9.\n"
                       " \n"
                       "The \002CLEAR\002 command clears all entries from the\n"
                       "bad words list."));
        return true;
    }
};

class BSBadwords : public Module {
    CommandBSBadwords commandbsbadwords;
    ExtensibleItem<BadWordsImpl> badwords;
    Serialize::Type badword_type;

  public:
    BSBadwords(const Anope::string &modname,
               const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandbsbadwords(this), badwords(this, "badwords"), badword_type("BadWord",
                BadWordImpl::Unserialize) {
    }
};

MODULE_INIT(BSBadwords)
