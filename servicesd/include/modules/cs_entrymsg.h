/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

struct EntryMsg {
    Anope::string chan;
    Anope::string creator;
    Anope::string message;
    time_t when;

    virtual ~EntryMsg() { }
  protected:
    EntryMsg() { }
};

struct EntryMessageList : Serialize::Checker<std::vector<EntryMsg *> > {
  protected:
    EntryMessageList() : Serialize::Checker<std::vector<EntryMsg *> >("EntryMsg") { }

  public:
    virtual ~EntryMessageList() {
        for (unsigned i = (*this)->size(); i > 0; --i) {
            delete (*this)->at(i - 1);
        }
    }

    virtual EntryMsg* Create() = 0;
};
