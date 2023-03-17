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
#include "modules/set_misc.h"

static Module *me;

static Anope::map<Anope::string> descriptions;

struct CSMiscData;
static Anope::map<ExtensibleItem<CSMiscData> *> items;

static ExtensibleItem<CSMiscData> *GetItem(const Anope::string &name) {
    ExtensibleItem<CSMiscData>* &it = items[name];
    if (!it)
        try {
            it = new ExtensibleItem<CSMiscData>(me, name);
        } catch (const ModuleException &) { }
    return it;
}

struct CSMiscData : MiscData, Serializable {
    CSMiscData(Extensible *obj) : Serializable("CSMiscData") { }

    CSMiscData(ChannelInfo *c, const Anope::string &n,
               const Anope::string &d) : Serializable("CSMiscData") {
        object = c->name;
        name = n;
        data = d;
    }

    void Serialize(Serialize::Data &sdata) const anope_override {
        sdata["ci"] << this->object;
        sdata["name"] << this->name;
        sdata["data"] << this->data;
    }

    static Serializable* Unserialize(Serializable *obj, Serialize::Data &data) {
        Anope::string sci, sname, sdata;

        data["ci"] >> sci;
        data["name"] >> sname;
        data["data"] >> sdata;

        ChannelInfo *ci = ChannelInfo::Find(sci);
        if (ci == NULL) {
            return NULL;
        }

        CSMiscData *d = NULL;
        if (obj) {
            d = anope_dynamic_static_cast<CSMiscData *>(obj);
            d->object = ci->name;
            data["name"] >> d->name;
            data["data"] >> d->data;
        } else {
            ExtensibleItem<CSMiscData> *item = GetItem(sname);
            if (item) {
                d = item->Set(ci, CSMiscData(ci, sname, sdata));
            }
        }

        return d;
    }
};

static Anope::string GetAttribute(const Anope::string &command) {
    size_t sp = command.rfind(' ');
    if (sp != Anope::string::npos) {
        return command.substr(sp + 1);
    }
    return command;
}

class CommandCSSetMisc : public Command {
  public:
    CommandCSSetMisc(Module *creator,
                     const Anope::string &cname = "chanserv/set/misc") : Command(creator, cname, 1,
                                 2) {
        this->SetSyntax(_("\037channel\037 [\037parameters\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        const Anope::string &param = params.size() > 1 ? params[1] : "";
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        Anope::string scommand = GetAttribute(source.command);
        Anope::string key = "cs_set_misc:" + scommand;
        ExtensibleItem<CSMiscData> *item = GetItem(key);
        if (item == NULL) {
            return;
        }

        if (!param.empty()) {
            item->Set(ci, CSMiscData(ci, key, param));
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to change it to " << param;
            source.Reply(CHAN_SETTING_CHANGED, scommand.c_str(), ci->name.c_str(),
                         params[1].c_str());
        } else {
            item->Unset(ci);
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to unset it";
            source.Reply(CHAN_SETTING_UNSET, scommand.c_str(), ci->name.c_str());
        }
    }

    void OnServHelp(CommandSource &source) anope_override {
        if (descriptions.count(source.command)) {
            this->SetDesc(descriptions[source.command]);
            Command::OnServHelp(source);
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        if (descriptions.count(source.command)) {
            this->SendSyntax(source);
            source.Reply("%s", Language::Translate(source.nc,
                                                   descriptions[source.command].c_str()));
            return true;
        }
        return false;
    }
};

class CSSetMisc : public Module {
    CommandCSSetMisc commandcssetmisc;
    Serialize::Type csmiscdata_type;

  public:
    CSSetMisc(const Anope::string &modname,
              const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcssetmisc(this), csmiscdata_type("CSMiscData", CSMiscData::Unserialize) {
        me = this;
    }

    ~CSSetMisc() {
        for (Anope::map<ExtensibleItem<CSMiscData> *>::iterator it = items.begin();
                it != items.end(); ++it) {
            delete it->second;
        }
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        descriptions.clear();

        for (int i = 0; i < conf->CountBlock("command"); ++i) {
            Configuration::Block *block = conf->GetBlock("command", i);

            if (block->Get<const Anope::string>("command") != "chanserv/set/misc") {
                continue;
            }

            Anope::string cname = block->Get<const Anope::string>("name");
            Anope::string desc = block->Get<const Anope::string>("misc_description");

            if (cname.empty() || desc.empty()) {
                continue;
            }

            descriptions[cname] = desc;
        }
    }

    void OnChanInfo(CommandSource &source, ChannelInfo *ci, InfoFormatter &info,
                    bool) anope_override {
        for (Anope::map<ExtensibleItem<CSMiscData> *>::iterator it = items.begin(); it != items.end(); ++it) {
            ExtensibleItem<CSMiscData> *e = it->second;
            MiscData *data = e->Get(ci);

            if (data != NULL) {
                info[e->name.substr(12).replace_all_cs("_", " ")] = data->data;
            }
        }
    }
};

MODULE_INIT(CSSetMisc)
