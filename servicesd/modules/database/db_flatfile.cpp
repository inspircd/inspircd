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

#include "module.h"

#ifndef _WIN32
#include <sys/wait.h>
#endif

class SaveData : public Serialize::Data {
  public:
    Anope::string last;
    std::fstream *fs;

    SaveData() : fs(NULL) { }

    std::iostream& operator[](const Anope::string &key) anope_override {
        if (key != last) {
            *fs << "\nDATA " << key << " ";
            last = key;
        }

        return *fs;
    }
};

class LoadData : public Serialize::Data {
  public:
    std::fstream *fs;
    unsigned int id;
    std::map<Anope::string, Anope::string> data;
    std::stringstream ss;
    bool read;

    LoadData() : fs(NULL), id(0), read(false) { }

    std::iostream& operator[](const Anope::string &key) anope_override {
        if (!read) {
            for (Anope::string token; std::getline(*this->fs, token.str());) {
                if (token.find("ID ") == 0) {
                    try {
                        this->id = convertTo<unsigned int>(token.substr(3));
                    } catch (const ConvertException &) { }

                    continue;
                } else if (token.find("DATA ") != 0) {
                    break;
                }

                size_t sp = token.find(' ', 5); // Skip DATA
                if (sp != Anope::string::npos) {
                    data[token.substr(5, sp - 5)] = token.substr(sp + 1);
                }
            }

            read = true;
        }

        ss.clear();
        this->ss << this->data[key];
        return this->ss;
    }

    std::set<Anope::string> KeySet() const anope_override {
        std::set<Anope::string> keys;
        for (std::map<Anope::string, Anope::string>::const_iterator it =
                    this->data.begin(), it_end = this->data.end(); it != it_end; ++it) {
            keys.insert(it->first);
        }
        return keys;
    }

    size_t Hash() const anope_override {
        size_t hash = 0;
        for (std::map<Anope::string, Anope::string>::const_iterator it =
                    this->data.begin(), it_end = this->data.end(); it != it_end; ++it)
            if (!it->second.empty()) {
                hash ^= Anope::hash_cs()(it->second);
            }
        return hash;
    }

    void Reset() {
        id = 0;
        read = false;
        data.clear();
    }
};

class DBFlatFile : public Module, public Pipe {
    /* Day the last backup was on */
    int last_day;
    /* Backup file names */
    std::map<Anope::string, std::list<Anope::string> > backups;
    bool loaded;

    int child_pid;

    void BackupDatabase() {
        tm *tm = localtime(&Anope::CurTime);

        if (tm->tm_mday != last_day) {
            last_day = tm->tm_mday;

            const std::vector<Anope::string> &type_order = Serialize::Type::GetTypeOrder();

            std::set<Anope::string> dbs;
            dbs.insert(Config->GetModule(this)->Get<const Anope::string>("database",
                       "anope.db"));

            for (unsigned i = 0; i < type_order.size(); ++i) {
                Serialize::Type *stype = Serialize::Type::Find(type_order[i]);

                if (stype && stype->GetOwner()) {
                    dbs.insert("module_" + stype->GetOwner()->name + ".db");
                }
            }


            for (std::set<Anope::string>::const_iterator it = dbs.begin(),
                    it_end = dbs.end(); it != it_end; ++it) {
                const Anope::string &oldname = Anope::DataDir + "/" + *it;
                Anope::string newname = Anope::DataDir + "/backups/" + *it + "-" + stringify(
                                            tm->tm_year + 1900) + Anope::printf("-%02i-",
                                                    tm->tm_mon + 1) + Anope::printf("%02i", tm->tm_mday);

                /* Backup already exists or no database to backup */
                if (Anope::IsFile(newname) || !Anope::IsFile(oldname)) {
                    continue;
                }

                Log(LOG_DEBUG) << "db_flatfile: Attempting to rename " << *it << " to " <<
                               newname;
                if (rename(oldname.c_str(), newname.c_str())) {
                    Anope::string err = Anope::LastError();
                    Log(this) << "Unable to back up database " << *it << " (" << err << ")!";

                    if (!Config->GetModule(this)->Get<bool>("nobackupokay")) {
                        Anope::Quitting = true;
                        Anope::QuitReason = "Unable to back up database " + *it + " (" + err + ")";
                    }

                    continue;
                }

                backups[*it].push_back(newname);

                unsigned keepbackups = Config->GetModule(this)->Get<unsigned>("keepbackups");
                if (keepbackups > 0 && backups[*it].size() > keepbackups) {
                    unlink(backups[*it].front().c_str());
                    backups[*it].pop_front();
                }
            }
        }
    }

  public:
    DBFlatFile(const Anope::string &modname,
               const Anope::string &creator) : Module(modname, creator, DATABASE | VENDOR),
        last_day(0), loaded(false), child_pid(-1) {

    }

#ifndef _WIN32
    void OnRestart() anope_override {
        OnShutdown();
    }

    void OnShutdown() anope_override {
        if (child_pid > -1) {
            Log(this) << "Waiting for child to exit...";

            int status;
            waitpid(child_pid, &status, 0);

            Log(this) << "Done";
        }
    }
#endif

    void OnNotify() anope_override {
        char buf[512];
        int i = this->Read(buf, sizeof(buf) - 1);
        if (i <= 0) {
            return;
        }
        buf[i] = 0;

        child_pid = -1;

        if (!*buf) {
            Log(this) << "Finished saving databases";
            return;
        }

        Log(this) << "Error saving databases: " << buf;

        if (!Config->GetModule(this)->Get<bool>("nobackupokay")) {
            Anope::Quitting = true;
        }
    }

    EventReturn OnLoadDatabase() anope_override {
        const std::vector<Anope::string> &type_order = Serialize::Type::GetTypeOrder();
        std::set<Anope::string> tried_dbs;

        const Anope::string &db_name = Anope::DataDir + "/" + Config->GetModule(this)->Get<const Anope::string>("database", "anope.db");

        std::fstream fd(db_name.c_str(), std::ios_base::in | std::ios_base::binary);
        if (!fd.is_open()) {
            Log(this) << "Unable to open " << db_name << " for reading!";
            return EVENT_STOP;
        }

        std::map<Anope::string, std::vector<std::streampos> > positions;

        for (Anope::string buf; std::getline(fd, buf.str());)
            if (buf.find("OBJECT ") == 0) {
                positions[buf.substr(7)].push_back(fd.tellg());
            }

        LoadData ld;
        ld.fs = &fd;

        for (unsigned i = 0; i < type_order.size(); ++i) {
            Serialize::Type *stype = Serialize::Type::Find(type_order[i]);
            if (!stype || stype->GetOwner()) {
                continue;
            }

            std::vector<std::streampos> &pos = positions[stype->GetName()];

            for (unsigned j = 0; j < pos.size(); ++j) {
                fd.clear();
                fd.seekg(pos[j]);

                Serializable *obj = stype->Unserialize(NULL, ld);
                if (obj != NULL) {
                    obj->id = ld.id;
                }
                ld.Reset();
            }
        }

        fd.close();

        loaded = true;
        return EVENT_STOP;
    }


    void OnSaveDatabase() anope_override {
        if (child_pid > -1) {
            Log(this) << "Database save is already in progress!";
            return;
        }

        BackupDatabase();

        int i = -1;
#ifndef _WIN32
        if (!Anope::Quitting && Config->GetModule(this)->Get<bool>("fork")) {
            i = fork();
            if (i > 0) {
                child_pid = i;
                return;
            } else if (i < 0) {
                Log(this) << "Unable to fork for database save";
            }
        }
#endif

        try {
            std::map<Module *, std::fstream *> databases;

            /* First open the databases of all of the registered types. This way, if we have a type with 0 objects, that database will be properly cleared */
            for (std::map<Anope::string, Serialize::Type *>::const_iterator it =
                        Serialize::Type::GetTypes().begin(), it_end = Serialize::Type::GetTypes().end();
                    it != it_end; ++it) {
                Serialize::Type *s_type = it->second;

                if (databases[s_type->GetOwner()]) {
                    continue;
                }

                Anope::string db_name;
                if (s_type->GetOwner()) {
                    db_name = Anope::DataDir + "/module_" + s_type->GetOwner()->name + ".db";
                } else {
                    db_name = Anope::DataDir + "/" + Config->GetModule(
                                  this)->Get<const Anope::string>("database", "anope.db");
                }

                std::fstream *fs = databases[s_type->GetOwner()] = new std::fstream((
                            db_name + ".tmp").c_str(),
                        std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);

                if (!fs->is_open()) {
                    Log(this) << "Unable to open " << db_name << " for writing";
                }
            }

            SaveData data;
            const std::list<Serializable *> &items = Serializable::GetItems();
            for (std::list<Serializable *>::const_iterator it = items.begin(),
                    it_end = items.end(); it != it_end; ++it) {
                Serializable *base = *it;
                Serialize::Type *s_type = base->GetSerializableType();

                data.fs = databases[s_type->GetOwner()];
                if (!data.fs || !data.fs->is_open()) {
                    continue;
                }

                *data.fs << "OBJECT " << s_type->GetName();
                if (base->id) {
                    *data.fs << "\nID " << base->id;
                }
                base->Serialize(data);
                *data.fs << "\nEND\n";
            }

            for (std::map<Module *, std::fstream *>::iterator it = databases.begin(),
                    it_end = databases.end(); it != it_end; ++it) {
                std::fstream *f = it->second;
                const Anope::string &db_name = Anope::DataDir + "/" + (it->first ?
                                               (it->first->name + ".db") : Config->GetModule(
                                                   this)->Get<const Anope::string>("database", "anope.db"));

                if (!f->is_open() || !f->good()) {
                    this->Write("Unable to write database " + db_name);

                    f->close();
                } else {
                    f->close();
#ifdef _WIN32
                    /* Windows rename() fails if the file already exists. */
                    remove(db_name.c_str());
#endif
                    rename((db_name + ".tmp").c_str(), db_name.c_str());
                }

                delete f;
            }
        } catch (...) {
            if (i) {
                throw;
            }
        }

        if (!i) {
            this->Notify();
            exit(0);
        }
    }

    /* Load just one type. Done if a module is reloaded during runtime */
    void OnSerializeTypeCreate(Serialize::Type *stype) anope_override {
        if (!loaded) {
            return;
        }

        Anope::string db_name;
        if (stype->GetOwner()) {
            db_name = Anope::DataDir + "/module_" + stype->GetOwner()->name + ".db";
        } else {
            db_name = Anope::DataDir + "/" + Config->GetModule(
                this)->Get<const Anope::string>("database", "anope.db");
        }

        std::fstream fd(db_name.c_str(), std::ios_base::in | std::ios_base::binary);
        if (!fd.is_open()) {
            Log(this) << "Unable to open " << db_name << " for reading!";
            return;
        }

        LoadData ld;
        ld.fs = &fd;

        for (Anope::string buf; std::getline(fd, buf.str());) {
            if (buf == "OBJECT " + stype->GetName()) {
                stype->Unserialize(NULL, ld);
                ld.Reset();
            }
        }

        fd.close();
    }
};

MODULE_INIT(DBFlatFile)
