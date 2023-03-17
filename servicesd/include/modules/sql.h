/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

namespace SQL {

class Data : public Serialize::Data {
  public:
    typedef std::map<Anope::string, std::stringstream *> Map;
    Map data;
    std::map<Anope::string, Type> types;

    ~Data() {
        Clear();
    }

    std::iostream& operator[](const Anope::string &key) anope_override {
        std::stringstream *&ss = data[key];
        if (!ss) {
            ss = new std::stringstream();
        }
        return *ss;
    }

    std::set<Anope::string> KeySet() const anope_override {
        std::set<Anope::string> keys;
        for (Map::const_iterator it = this->data.begin(), it_end = this->data.end();
                it != it_end; ++it) {
            keys.insert(it->first);
        }
        return keys;
    }

    size_t Hash() const anope_override {
        size_t hash = 0;
        for (Map::const_iterator it = this->data.begin(), it_end = this->data.end();
                it != it_end; ++it)
            if (!it->second->str().empty()) {
                hash ^= Anope::hash_cs()(it->second->str());
            }
        return hash;
    }

    std::map<Anope::string, std::iostream *> GetData() const {
        std::map<Anope::string, std::iostream *> d;
        for (Map::const_iterator it = this->data.begin(), it_end = this->data.end();
                it != it_end; ++it) {
            d[it->first] = it->second;
        }
        return d;
    }

    void Clear() {
        for (Map::const_iterator it = this->data.begin(), it_end = this->data.end();
                it != it_end; ++it) {
            delete it->second;
        }
        this->data.clear();
    }

    void SetType(const Anope::string &key, Type t) anope_override {
        this->types[key] = t;
    }

    Type GetType(const Anope::string &key) const anope_override {
        std::map<Anope::string, Type>::const_iterator it = this->types.find(key);
        if (it != this->types.end()) {
            return it->second;
        }
        return DT_TEXT;
    }
};

/** A SQL exception, can be thrown at various points
 */
class Exception : public ModuleException {
  public:
    Exception(const Anope::string &reason) : ModuleException(reason) { }

    virtual ~Exception() throw() { }
};

/** A SQL query
 */

struct QueryData {
    Anope::string data;
    bool escape;
};

struct Query {
    Anope::string query;
    std::map<Anope::string, QueryData> parameters;

    Query() { }
    Query(const Anope::string &q) : query(q) { }

    Query& operator=(const Anope::string &q) {
        this->query = q;
        this->parameters.clear();
        return *this;
    }

    bool operator==(const Query &other) const {
        return this->query == other.query;
    }

    inline bool operator!=(const Query &other) const {
        return !(*this == other);
    }

    template<typename T> void SetValue(const Anope::string &key, const T& value,
                                       bool escape = true) {
        try {
            Anope::string string_value = stringify(value);
            this->parameters[key].data = string_value;
            this->parameters[key].escape = escape;
        } catch (const ConvertException &ex) { }
    }
};

/** A result from a SQL query
 */
class Result {
  protected:
    /* Rows, column, item */
    std::vector<std::map<Anope::string, Anope::string> > entries;
    Query query;
    Anope::string error;
  public:
    unsigned int id;
    Anope::string finished_query;

    Result() : id(0) { }
    Result(unsigned int i, const Query &q, const Anope::string &fq,
           const Anope::string &err = "") : query(q), error(err), id(i),
        finished_query(fq) { }

    inline operator bool() const {
        return this->error.empty();
    }

    inline unsigned int GetID() const {
        return this->id;
    }
    inline const Query &GetQuery() const {
        return this->query;
    }
    inline const Anope::string &GetError() const {
        return this->error;
    }

    int Rows() const {
        return this->entries.size();
    }

    const std::map<Anope::string, Anope::string> &Row(size_t index) const {
        try {
            return this->entries.at(index);
        } catch (const std::out_of_range &) {
            throw Exception("Out of bounds access to SQLResult");
        }
    }

    const Anope::string Get(size_t index, const Anope::string &col) const {
        const std::map<Anope::string, Anope::string> rows = this->Row(index);

        std::map<Anope::string, Anope::string>::const_iterator it = rows.find(col);
        if (it == rows.end()) {
            throw Exception("Unknown column name in SQLResult: " + col);
        }

        return it->second;
    }
};

/* An interface used by modules to retrieve the results
 */
class Interface {
  public:
    Module *owner;

    Interface(Module *m) : owner(m) { }
    virtual ~Interface() { }

    virtual void OnResult(const Result &r) = 0;
    virtual void OnError(const Result &r) = 0;
};

/** Class providing the SQL service, modules call this to execute queries
 */
class Provider : public Service {
  public:
    Provider(Module *c, const Anope::string &n) : Service(c, "SQL::Provider", n) { }

    virtual void Run(Interface *i, const Query &query) = 0;

    virtual Result RunQuery(const Query &query) = 0;

    virtual std::vector<Query> CreateTable(const Anope::string &table,
                                           const Data &data) = 0;

    virtual Query BuildInsert(const Anope::string &table, unsigned int id,
                              Data &data) = 0;

    virtual Query GetTables(const Anope::string &prefix) = 0;

    virtual Anope::string FromUnixtime(time_t) = 0;
};

}
