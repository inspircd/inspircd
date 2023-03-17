/* Configuration file handling.
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
#include "config.h"
#include "bots.h"
#include "access.h"
#include "opertype.h"
#include "channels.h"
#include "hashcomp.h"

using namespace Configuration;

File ServicesConf("services.conf", false); // Services configuration file name
Conf *Config = NULL;

Block::Block(const Anope::string &n) : name(n), linenum(-1) {
}

const Anope::string &Block::GetName() const {
    return name;
}

int Block::CountBlock(const Anope::string &bname) {
    if (!this) {
        return 0;
    }

    return blocks.count(bname);
}

Block* Block::GetBlock(const Anope::string &bname, int num) {
    if (!this) {
        return NULL;
    }

    std::pair<block_map::iterator, block_map::iterator> it = blocks.equal_range(
                bname);

    for (int i = 0; it.first != it.second; ++it.first, ++i)
        if (i == num) {
            return &it.first->second;
        }
    return NULL;
}

bool Block::Set(const Anope::string &tag, const Anope::string &value) {
    if (!this) {
        return false;
    }

    items[tag] = value;
    return true;
}

const Block::item_map* Block::GetItems() const {
    if (this) {
        return &items;
    } else {
        return NULL;
    }
}

template<> const Anope::string Block::Get(const Anope::string &tag,
        const Anope::string& def) const {
    if (!this) {
        return def;
    }

    Anope::map<Anope::string>::const_iterator it = items.find(tag);
    if (it != items.end()) {
        return it->second;
    }

    return def;
}

template<> time_t Block::Get(const Anope::string &tag,
                             const Anope::string &def) const {
    return Anope::DoTime(Get<const Anope::string>(tag, def));
}

template<> bool Block::Get(const Anope::string &tag,
                           const Anope::string &def) const {
    const Anope::string &str = Get<const Anope::string>(tag, def);
    return !str.empty() && !str.equals_ci("no") && !str.equals_ci("off")
           && !str.equals_ci("false") && !str.equals_ci("0");
}

static void ValidateNotEmpty(const Anope::string &block,
                             const Anope::string &name, const Anope::string &value) {
    if (value.empty()) {
        throw ConfigException("The value for <" + block + ":" + name + "> (" + value +
                              ") cannot be empty!");
    }
}

static void ValidateNoSpaces(const Anope::string &block,
                             const Anope::string &name, const Anope::string &value) {
    if (value.find(' ') != Anope::string::npos) {
        throw ConfigException("The value for <" + block + ":" + name + "> (" + value +
                              ") may not contain spaces!");
    }
}

static void ValidateNotEmptyOrSpaces(const Anope::string &block,
                                     const Anope::string &name, const Anope::string &value) {
    ValidateNotEmpty(block, name, value);
    ValidateNoSpaces(block, name, value);
}

template<typename T> static void ValidateNotZero(const Anope::string &block,
        const Anope::string &name, T value) {
    if (!value) {
        throw ConfigException("The value for <" + block + ":" + name +
                              "> cannot be zero!");
    }
}

Conf::Conf() : Block("") {
    ReadTimeout = 0;
    UsePrivmsg = DefPrivmsg = false;

    this->LoadConf(ServicesConf);

    for (int i = 0; i < this->CountBlock("include"); ++i) {
        Block *include = this->GetBlock("include", i);

        const Anope::string &type = include->Get<const Anope::string>("type"),
                             &file = include->Get<const Anope::string>("name");

        File f(file, type == "executable");
        this->LoadConf(f);
    }

    FOREACH_MOD(OnReload, (this));

    /* Check for modified values that aren't allowed to be modified */
    if (Config) {
        struct {
            Anope::string block;
            Anope::string name;
        } noreload[] = {
            {"serverinfo", "name"},
            {"serverinfo", "description"},
            {"serverinfo", "localhost"},
            {"serverinfo", "id"},
            {"serverinfo", "pid"},
            {"networkinfo", "nicklen"},
            {"networkinfo", "userlen"},
            {"networkinfo", "hostlen"},
            {"networkinfo", "chanlen"},
        };

        for (unsigned i = 0; i < sizeof(noreload) / sizeof(noreload[0]); ++i)
            if (this->GetBlock(noreload[i].block)->Get<const Anope::string>
                    (noreload[i].name) != Config->GetBlock(
                        noreload[i].block)->Get<const Anope::string>(noreload[i].name)) {
                throw ConfigException("<" + noreload[i].block + ":" + noreload[i].name +
                                      "> can not be modified once set");
            }
    }

    Block *serverinfo = this->GetBlock("serverinfo"),
           *options = this->GetBlock("options"),
            *mail = this->GetBlock("mail"), *networkinfo = this->GetBlock("networkinfo");

    const Anope::string &servername = serverinfo->Get<Anope::string>("name");

    ValidateNotEmptyOrSpaces("serverinfo", "name", servername);

    if (servername.find(' ') != Anope::string::npos
            || servername.find('.') == Anope::string::npos) {
        throw ConfigException("serverinfo:name is not a valid server name");
    }

    ValidateNotEmpty("serverinfo", "description",
                     serverinfo->Get<const Anope::string>("description"));
    ValidateNotEmpty("serverinfo", "pid",
                     serverinfo->Get<const Anope::string>("pid"));
    ValidateNotEmpty("serverinfo", "motd",
                     serverinfo->Get<const Anope::string>("motd"));

    ValidateNotZero("options", "readtimeout", options->Get<time_t>("readtimeout"));
    ValidateNotZero("options", "warningtimeout",
                    options->Get<time_t>("warningtimeout"));

    ValidateNotZero("networkinfo", "nicklen",
                    networkinfo->Get<unsigned>("nicklen"));
    ValidateNotZero("networkinfo", "userlen",
                    networkinfo->Get<unsigned>("userlen"));
    ValidateNotZero("networkinfo", "hostlen",
                    networkinfo->Get<unsigned>("hostlen"));
    ValidateNotZero("networkinfo", "chanlen",
                    networkinfo->Get<unsigned>("chanlen"));

    spacesepstream(options->Get<const Anope::string>("ulineservers")).GetTokens(
        this->Ulines);

    if (mail->Get<bool>("usemail")) {
        Anope::string check[] = { "sendmailpath", "sendfrom", "registration_subject", "registration_message", "emailchange_subject", "emailchange_message", "memo_subject", "memo_message" };
        for (unsigned i = 0; i < sizeof(check) / sizeof(Anope::string); ++i) {
            ValidateNotEmpty("mail", check[i], mail->Get<const Anope::string>(check[i]));
        }
    }

    this->ReadTimeout = options->Get<time_t>("readtimeout");
    this->UsePrivmsg = options->Get<bool>("useprivmsg");
    this->UseStrictPrivmsg = options->Get<bool>("usestrictprivmsg");
    this->StrictPrivmsg = !UseStrictPrivmsg ? "/msg " : "/";
    {
        std::vector<Anope::string> defaults;
        spacesepstream(
            this->GetModule("nickserv")->Get<const Anope::string>("defaults")).GetTokens(
                defaults);
        this->DefPrivmsg = std::find(defaults.begin(), defaults.end(),
                                     "msg") != defaults.end();
    }
    this->DefLanguage = options->Get<const Anope::string>("defaultlanguage");
    this->TimeoutCheck = options->Get<time_t>("timeoutcheck");
    this->NickChars = networkinfo->Get<Anope::string>("nick_chars");

    for (int i = 0; i < this->CountBlock("uplink"); ++i) {
        Block *uplink = this->GetBlock("uplink", i);

        const Anope::string &host = uplink->Get<const Anope::string>("host");
        bool ipv6 = uplink->Get<bool>("ipv6");
        int port = uplink->Get<int>("port");
        const Anope::string &password = uplink->Get<const Anope::string>("password");

        ValidateNotEmptyOrSpaces("uplink", "host", host);
        ValidateNotZero("uplink", "port", port);
        ValidateNotEmptyOrSpaces("uplink", "password", password);

        if (password.find(' ') != Anope::string::npos || password[0] == ':') {
            throw ConfigException("uplink:password is not valid");
        }

        this->Uplinks.push_back(Uplink(host, port, password, ipv6));
    }

    for (int i = 0; i < this->CountBlock("module"); ++i) {
        Block *module = this->GetBlock("module", i);

        const Anope::string &modname = module->Get<const Anope::string>("name");

        ValidateNotEmptyOrSpaces("module", "name", modname);

        this->ModulesAutoLoad.push_back(modname);
    }

    for (int i = 0; i < this->CountBlock("opertype"); ++i) {
        Block *opertype = this->GetBlock("opertype", i);

        const Anope::string &oname = opertype->Get<const Anope::string>("name"),
                             &modes = opertype->Get<const Anope::string>("modes"),
                              &inherits = opertype->Get<const Anope::string>("inherits"),
                               &commands = opertype->Get<const Anope::string>("commands"),
                                &privs = opertype->Get<const Anope::string>("privs");

        ValidateNotEmpty("opertype", "name", oname);

        OperType *ot = new OperType(oname);
        ot->modes = modes;

        spacesepstream cmdstr(commands);
        for (Anope::string str; cmdstr.GetToken(str);) {
            ot->AddCommand(str);
        }

        spacesepstream privstr(privs);
        for (Anope::string str; privstr.GetToken(str);) {
            ot->AddPriv(str);
        }

        commasepstream inheritstr(inherits);
        for (Anope::string str; inheritstr.GetToken(str);) {
            /* Strip leading ' ' after , */
            if (str.length() > 1 && str[0] == ' ') {
                str.erase(str.begin());
            }
            for (unsigned j = 0; j < this->MyOperTypes.size(); ++j) {
                OperType *ot2 = this->MyOperTypes[j];

                if (ot2->GetName().equals_ci(str)) {
                    Log() << "Inheriting commands and privs from " << ot2->GetName() << " to " <<
                          ot->GetName();
                    ot->Inherits(ot2);
                    break;
                }
            }
        }

        this->MyOperTypes.push_back(ot);
    }

    for (int i = 0; i < this->CountBlock("oper"); ++i) {
        Block *oper = this->GetBlock("oper", i);

        const Anope::string &nname = oper->Get<const Anope::string>("name"),
                             &type = oper->Get<const Anope::string>("type"),
                              &password = oper->Get<const Anope::string>("password"),
                               &certfp = oper->Get<const Anope::string>("certfp"),
                                &host = oper->Get<const Anope::string>("host"),
                                 &vhost = oper->Get<const Anope::string>("vhost");
        bool require_oper = oper->Get<bool>("require_oper");

        ValidateNotEmptyOrSpaces("oper", "name", nname);
        ValidateNotEmpty("oper", "type", type);

        OperType *ot = NULL;
        for (unsigned j = 0; j < this->MyOperTypes.size(); ++j)
            if (this->MyOperTypes[j]->GetName() == type) {
                ot = this->MyOperTypes[j];
            }
        if (ot == NULL) {
            throw ConfigException("Oper block for " + nname + " has invalid oper type " +
                                  type);
        }

        Oper *o = new Oper(nname, ot);
        o->require_oper = require_oper;
        o->password = password;
        o->certfp = certfp;
        spacesepstream(host).GetTokens(o->hosts);
        o->vhost = vhost;

        this->Opers.push_back(o);
    }

    for (botinfo_map::const_iterator it = BotListByNick->begin(),
            it_end = BotListByNick->end(); it != it_end; ++it) {
        it->second->conf = false;
    }
    for (int i = 0; i < this->CountBlock("service"); ++i) {
        Block *service = this->GetBlock("service", i);

        const Anope::string &nick = service->Get<const Anope::string>("nick"),
                             &user = service->Get<const Anope::string>("user"),
                              &host = service->Get<const Anope::string>("host"),
                               &gecos = service->Get<const Anope::string>("gecos"),
                                &modes = service->Get<const Anope::string>("modes"),
                                 &channels = service->Get<const Anope::string>("channels");

        ValidateNotEmptyOrSpaces("service", "nick", nick);
        ValidateNotEmptyOrSpaces("service", "user", user);
        ValidateNotEmptyOrSpaces("service", "host", host);
        ValidateNotEmpty("service", "gecos", gecos);
        ValidateNoSpaces("service", "channels", channels);

        BotInfo *bi = BotInfo::Find(nick, true);
        if (!bi) {
            bi = new BotInfo(nick, user, host, gecos, modes);
        }
        bi->conf = true;

        std::vector<Anope::string> oldchannels = bi->botchannels;
        bi->botchannels.clear();
        commasepstream sep(channels);
        for (Anope::string token; sep.GetToken(token);) {
            bi->botchannels.push_back(token);
            size_t ch = token.find('#');
            Anope::string chname, want_modes;
            if (ch == Anope::string::npos) {
                chname = token;
            } else {
                want_modes = token.substr(0, ch);
                chname = token.substr(ch);
            }
            bi->Join(chname);
            Channel *c = Channel::Find(chname);
            if (!c) {
                continue;    // Can't happen
            }

            c->botchannel = true;

            /* Remove all existing modes */
            ChanUserContainer *cu = c->FindUser(bi);
            if (cu != NULL)
                for (size_t j = 0; j < cu->status.Modes().length(); ++j) {
                    c->RemoveMode(bi, ModeManager::FindChannelModeByChar(cu->status.Modes()[j]),
                                  bi->GetUID());
                }
            /* Set the new modes */
            for (unsigned j = 0; j < want_modes.length(); ++j) {
                ChannelMode *cm = ModeManager::FindChannelModeByChar(want_modes[j]);
                if (cm == NULL) {
                    cm = ModeManager::FindChannelModeByChar(ModeManager::GetStatusChar(
                            want_modes[j]));
                }
                if (cm && cm->type == MODE_STATUS) {
                    c->SetMode(bi, cm, bi->GetUID());
                }
            }
        }
        for (unsigned k = 0; k < oldchannels.size(); ++k) {
            size_t ch = oldchannels[k].find('#');
            Anope::string chname = oldchannels[k].substr(ch != Anope::string::npos ? ch :
                                   0);

            bool found = false;
            for (unsigned j = 0; j < bi->botchannels.size(); ++j) {
                ch = bi->botchannels[j].find('#');
                Anope::string ochname = bi->botchannels[j].substr(ch != Anope::string::npos ?
                                        ch : 0);

                if (chname.equals_ci(ochname)) {
                    found = true;
                }
            }

            if (found) {
                continue;
            }

            Channel *c = Channel::Find(chname);
            if (c) {
                c->botchannel = false;
                bi->Part(c);
            }
        }
    }

    for (int i = 0; i < this->CountBlock("log"); ++i) {
        Block *log = this->GetBlock("log", i);

        int logage = log->Get<int>("logage");
        bool rawio = log->Get<bool>("rawio");
        bool debug = log->Get<bool>("debug");

        LogInfo l(logage, rawio, debug);

        l.bot = BotInfo::Find(log->Get<const Anope::string>("bot", "Global"), true);
        spacesepstream(log->Get<const Anope::string>("target")).GetTokens(l.targets);
        spacesepstream(log->Get<const Anope::string>("source")).GetTokens(l.sources);
        spacesepstream(log->Get<const Anope::string>("admin")).GetTokens(l.admin);
        spacesepstream(log->Get<const Anope::string>("override")).GetTokens(l.override);
        spacesepstream(log->Get<const Anope::string>("commands")).GetTokens(l.commands);
        spacesepstream(log->Get<const Anope::string>("servers")).GetTokens(l.servers);
        spacesepstream(log->Get<const Anope::string>("channels")).GetTokens(l.channels);
        spacesepstream(log->Get<const Anope::string>("users")).GetTokens(l.users);
        spacesepstream(log->Get<const Anope::string>("other")).GetTokens(l.normal);

        this->LogInfos.push_back(l);
    }

    for (botinfo_map::const_iterator it = BotListByNick->begin(),
            it_end = BotListByNick->end(); it != it_end; ++it) {
        it->second->commands.clear();
    }
    for (int i = 0; i < this->CountBlock("command"); ++i) {
        Block *command = this->GetBlock("command", i);

        const Anope::string &service = command->Get<const Anope::string>("service"),
                             &nname = command->Get<const Anope::string>("name"),
                              &cmd = command->Get<const Anope::string>("command"),
                               &permission = command->Get<const Anope::string>("permission"),
                                &group = command->Get<const Anope::string>("group");
        bool hide = command->Get<bool>("hide");

        ValidateNotEmptyOrSpaces("command", "service", service);
        ValidateNotEmpty("command", "name", nname);
        ValidateNotEmptyOrSpaces("command", "command", cmd);

        BotInfo *bi = this->GetClient(service);
        if (!bi) {
            continue;
        }

        CommandInfo &ci = bi->SetCommand(nname, cmd, permission);
        ci.group = group;
        ci.hide = hide;
    }

    PrivilegeManager::ClearPrivileges();
    for (int i = 0; i < this->CountBlock("privilege"); ++i) {
        Block *privilege = this->GetBlock("privilege", i);

        const Anope::string &nname = privilege->Get<const Anope::string>("name"),
                             &desc = privilege->Get<const Anope::string>("desc");
        int rank = privilege->Get<int>("rank");

        PrivilegeManager::AddPrivilege(Privilege(nname, desc, rank));
    }

    for (int i = 0; i < this->CountBlock("fantasy"); ++i) {
        Block *fantasy = this->GetBlock("fantasy", i);

        const Anope::string &nname = fantasy->Get<const Anope::string>("name"),
                             &service = fantasy->Get<const Anope::string>("command"),
                              &permission = fantasy->Get<const Anope::string>("permission"),
                               &group = fantasy->Get<const Anope::string>("group");
        bool hide = fantasy->Get<bool>("hide"),
             prepend_channel = fantasy->Get<bool>("prepend_channel", "yes");

        ValidateNotEmpty("fantasy", "name", nname);
        ValidateNotEmptyOrSpaces("fantasy", "command", service);

        CommandInfo &c = this->Fantasy[nname];
        c.name = service;
        c.permission = permission;
        c.group = group;
        c.hide = hide;
        c.prepend_channel = prepend_channel;
    }

    for (int i = 0; i < this->CountBlock("command_group"); ++i) {
        Block *command_group = this->GetBlock("command_group", i);

        const Anope::string &nname = command_group->Get<const Anope::string>("name"),
                             &description = command_group->Get<const Anope::string>("description");

        CommandGroup gr;
        gr.name = nname;
        gr.description = description;

        this->CommandGroups.push_back(gr);
    }

    /* Below here can't throw */

    if (Config)
        /* Clear existing conf opers */
        for (nickcore_map::const_iterator it = NickCoreList->begin(),
                it_end = NickCoreList->end(); it != it_end; ++it) {
            NickCore *nc = it->second;
            if (nc->o && std::find(Config->Opers.begin(), Config->Opers.end(),
                                   nc->o) != Config->Opers.end()) {
                nc->o = NULL;
            }
        }
    /* Apply new opers */
    for (unsigned i = 0; i < this->Opers.size(); ++i) {
        Oper *o = this->Opers[i];

        NickAlias *na = NickAlias::Find(o->name);
        if (!na) {
            continue;
        }

        if (!na->nc || na->nc->o) {
            // If the account is already an oper it might mean two oper blocks for the same nick, or
            // something else has configured them as an oper (like a module)
            continue;
        }

        na->nc->o = o;
        Log() << "Tied oper " << na->nc->display << " to type " << o->ot->GetName();
    }

    if (options->Get<const Anope::string>("casemap", "ascii") == "ascii") {
        Anope::casemap = std::locale(std::locale(), new Anope::ascii_ctype<char>());
    } else if (options->Get<const Anope::string>("casemap") == "rfc1459") {
        Anope::casemap = std::locale(std::locale(), new Anope::rfc1459_ctype<char>());
    } else {
        try {
            Anope::casemap = std::locale(
                                 options->Get<const Anope::string>("casemap").c_str());
        } catch (const std::runtime_error &) {
            Log() << "Unknown casemap " << options->Get<const Anope::string>("casemap") <<
                  " - casemap not changed";
        }
    }
    Anope::CaseMapRebuild();

    /* Check the user keys */
    if (!options->Get<unsigned>("seed")) {
        Log() << "Configuration option options:seed should be set. It's for YOUR safety! Remember that!";
    }
}

Conf::~Conf() {
    for (unsigned i = 0; i < MyOperTypes.size(); ++i) {
        delete MyOperTypes[i];
    }
    for (unsigned i = 0; i < Opers.size(); ++i) {
        delete Opers[i];
    }
}

void Conf::Post(Conf *old) {
    /* Apply module changes */
    for (unsigned i = 0; i < old->ModulesAutoLoad.size(); ++i)
        if (std::find(this->ModulesAutoLoad.begin(), this->ModulesAutoLoad.end(),
                      old->ModulesAutoLoad[i]) == this->ModulesAutoLoad.end()) {
            ModuleManager::UnloadModule(ModuleManager::FindModule(old->ModulesAutoLoad[i]),
                                        NULL);
        }
    for (unsigned i = 0; i < this->ModulesAutoLoad.size(); ++i)
        if (std::find(old->ModulesAutoLoad.begin(), old->ModulesAutoLoad.end(),
                      this->ModulesAutoLoad[i]) == old->ModulesAutoLoad.end()) {
            ModuleManager::LoadModule(this->ModulesAutoLoad[i], NULL);
        }

    /* Apply opertype changes, as non-conf opers still point to the old oper types */
    for (unsigned i = Oper::opers.size(); i > 0; --i) {
        Oper *o = Oper::opers[i - 1];

        /* Oper's type is in the old config, so update it */
        if (std::find(old->MyOperTypes.begin(), old->MyOperTypes.end(),
                      o->ot) != old->MyOperTypes.end()) {
            OperType *ot = o->ot;
            o->ot = NULL;

            for (unsigned j = 0; j < MyOperTypes.size(); ++j)
                if (ot->GetName() == MyOperTypes[j]->GetName()) {
                    o->ot = MyOperTypes[j];
                }

            if (o->ot == NULL) {
                /* Oper block has lost type */
                std::vector<Oper *>::iterator it = std::find(old->Opers.begin(),
                                                   old->Opers.end(), o);
                if (it != old->Opers.end()) {
                    old->Opers.erase(it);
                }

                it = std::find(this->Opers.begin(), this->Opers.end(), o);
                if (it != this->Opers.end()) {
                    this->Opers.erase(it);
                }

                delete o;
            }
        }
    }
}

Block *Conf::GetModule(Module *m) {
    if (!m) {
        return NULL;
    }

    return GetModule(m->name);
}

Block *Conf::GetModule(const Anope::string &mname) {
    std::map<Anope::string, Block *>::iterator it = modules.find(mname);
    if (it != modules.end()) {
        return it->second;
    }

    Block* &block = modules[mname];

    /* Search for the block */
    for (std::pair<block_map::iterator, block_map::iterator> iters =
                blocks.equal_range("module"); iters.first != iters.second; ++iters.first) {
        Block *b = &iters.first->second;

        if (b->Get<const Anope::string>("name") == mname) {
            block = b;
            break;
        }
    }

    return GetModule(mname);
}

BotInfo *Conf::GetClient(const Anope::string &cname) {
    Anope::map<Anope::string>::iterator it = bots.find(cname);
    if (it != bots.end()) {
        return BotInfo::Find(!it->second.empty() ? it->second : cname, true);
    }

    Block *block = GetModule(cname.lower());
    const Anope::string &client = block->Get<const Anope::string>("client");
    bots[cname] = client;
    return GetClient(cname);
}

Block *Conf::GetCommand(CommandSource &source) {
    const Anope::string &block_name = source.c ? "fantasy" : "command";

    for (std::pair<block_map::iterator, block_map::iterator> iters =
                blocks.equal_range(block_name); iters.first != iters.second; ++iters.first) {
        Block *b = &iters.first->second;

        if (b->Get<Anope::string>("name") == source.command) {
            return b;
        }
    }

    return NULL;
}

File::File(const Anope::string &n, bool e) : name(n), executable(e), fp(NULL) {
}

File::~File() {
    this->Close();
}

const Anope::string &File::GetName() const {
    return this->name;
}

Anope::string File::GetPath() const {
    return (this->executable ? "" : Anope::ConfigDir + "/") + this->name;
}

bool File::IsOpen() const {
    return this->fp != NULL;
}

bool File::Open() {
    this->Close();
    this->fp = (this->executable ? popen(this->name.c_str(),
                                         "r") : fopen((Anope::ConfigDir + "/" + this->name).c_str(), "r"));
    return this->fp != NULL;
}

void File::Close() {
    if (this->fp != NULL) {
        if (this->executable) {
            pclose(this->fp);
        } else {
            fclose(this->fp);
        }
        this->fp = NULL;
    }
}

bool File::End() const {
    return !this->IsOpen() || feof(this->fp);
}

Anope::string File::Read() {
    Anope::string ret;
    char buf[BUFSIZE];
    while (fgets(buf, sizeof(buf), this->fp) != NULL) {
        char *nl = strchr(buf, '\n');
        if (nl != NULL) {
            *nl = 0;
        } else if (!this->End()) {
            ret += buf;
            continue;
        }

        ret = buf;
        break;
    }

    return ret;
}

void Conf::LoadConf(File &file) {
    if (file.GetName().empty()) {
        return;
    }

    if (!file.Open()) {
        throw ConfigException("File " + file.GetPath() + " could not be opened.");
    }

    Anope::string itemname, wordbuffer;
    std::stack<Block *> block_stack;
    int linenumber = 0;
    bool in_word = false, in_quote = false, in_comment = false;

    Log(LOG_DEBUG) << "Start to read conf " << file.GetPath();
    // Start reading characters...
    while (!file.End()) {
        Anope::string line = file.Read();
        ++linenumber;

        /* If this line is completely empty and we are in a quote, just append a newline */
        if (line.empty() && in_quote) {
            wordbuffer += "\n";
        }

        for (unsigned c = 0, len = line.length(); c < len; ++c) {
            char ch = line[c];
            if (in_quote) {
                /* Strip leading white spaces from multi line quotes */
                if (c == 0) {
                    while (c < len && isspace(line[c])) {
                        ++c;
                    }
                    ch = line[c];
                }

                /* Allow \" in quotes */
                if (ch == '\\' && c + 1 < len && line[c + 1] == '"') {
                    wordbuffer += line[++c];
                } else if (ch == '"') {
                    in_quote = in_word = false;
                } else if (ch) {
                    wordbuffer += ch;
                }
            } else if (in_comment) {
                if (ch == '*' && c + 1 < len && line[c + 1] == '/') {
                    in_comment = false;
                    ++c;
                    // We might be at an eol, so continue on and process it
                } else {
                    continue;
                }
            } else if (ch == '#' || (ch == '/' && c + 1 < len && line[c + 1] == '/')) {
                c = len - 1;    // Line comment, ignore the rest of the line (much like this one!)
            } else if (ch == '/' && c + 1 < len && line[c + 1] == '*') {
                // Multiline (or less than one line) comment
                in_comment = true;
                ++c;
                continue;
            } else if (!in_word && (ch == '(' || ch == '_' || ch == ')'))
                ;
            else if (ch == '"') {
                // Quotes are valid only in the value position
                if (block_stack.empty() || itemname.empty()) {
                    file.Close();
                    throw ConfigException("Unexpected quoted string: " + file.GetName() + ":" +
                                          stringify(linenumber));
                }
                if (in_word || !wordbuffer.empty()) {
                    file.Close();
                    throw ConfigException("Unexpected quoted string (prior unhandled words): " +
                                          file.GetName() + ":" + stringify(linenumber));
                }
                in_quote = in_word = true;
            } else if (ch == '=') {
                if (block_stack.empty()) {
                    file.Close();
                    throw ConfigException("Config item outside of section (or stray '='): " +
                                          file.GetName() + ":" + stringify(linenumber));
                }

                if (!itemname.empty() || wordbuffer.empty()) {
                    file.Close();
                    throw ConfigException("Stray '=' sign or item without value: " + file.GetName()
                                          + ":" + stringify(linenumber));
                }

                in_word = false;
                itemname = wordbuffer;
                wordbuffer.clear();
            } else if (ch == '{') {
                if (wordbuffer.empty()) {
                    block_stack.push(NULL);
                    // Commented or unnamed section
                    continue;
                }

                if (!block_stack.empty() && !block_stack.top()) {
                    // Named block inside of a commented block
                    in_word = false;
                    wordbuffer.clear();
                    block_stack.push(NULL);
                    continue;
                }

                Block *b = block_stack.empty() ? this : block_stack.top();
                block_map::iterator it = b->blocks.insert(std::make_pair(wordbuffer,
                                         Configuration::Block(wordbuffer)));
                b = &it->second;
                b->linenum = linenumber;
                block_stack.push(b);

                in_word = false;
                wordbuffer.clear();
                continue;
            } else if (ch == ' ' || ch == '\r' || ch == '\t') {
                // Terminate word
                in_word = false;
            } else if (ch == ';' || ch == '}')
                ;
            else {
                if (!in_word && !wordbuffer.empty()) {
                    file.Close();
                    throw ConfigException("Unexpected word: " + file.GetName() + ":" + stringify(
                                              linenumber));
                }
                wordbuffer += ch;
                in_word = true;
            }

            if (ch == ';' || ch == '}' || c + 1 >= len) {
                bool eol = c + 1 >= len;

                if (!eol && in_quote)
                    // Allow ; and } in quoted strings
                {
                    continue;
                }

                if (in_quote) {
                    // Quotes can span multiple lines; all we need to do is go to the next line without clearing things
                    wordbuffer += "\n";
                    continue;
                }

                in_word = false;
                if (!itemname.empty()) {
                    if (block_stack.empty()) {
                        file.Close();
                        throw ConfigException("Stray ';' outside of block: " + file.GetName() + ":" +
                                              stringify(linenumber));
                    }

                    Block *b = block_stack.top();

                    if (b) {
                        Log(LOG_DEBUG) << "ln " << linenumber << " EOL: s='" << b->name << "' '" <<
                                       itemname << "' set to '" << wordbuffer << "'";
                    }

                    /* Check defines */
                    for (int i = 0; i < this->CountBlock("define"); ++i) {
                        Block *define = this->GetBlock("define", i);

                        const Anope::string &dname = define->Get<const Anope::string>("name");

                        if (dname == wordbuffer && define != b) {
                            wordbuffer = define->Get<const Anope::string>("value");
                        }
                    }

                    if (b) {
                        b->items[itemname] = wordbuffer;
                    }

                    wordbuffer.clear();
                    itemname.clear();
                }

                if (ch == '}') {
                    if (block_stack.empty()) {
                        file.Close();
                        throw ConfigException("Stray '}': " + file.GetName() + ":" + stringify(
                                                  linenumber));
                    }

                    block_stack.pop();
                }
            }
        }
    }

    file.Close();

    if (in_comment) {
        throw ConfigException("Unterminated multiline comment at end of file: " +
                              file.GetName());
    }
    if (in_quote) {
        throw ConfigException("Unterminated quote at end of file: " + file.GetName());
    }
    if (!itemname.empty() || !wordbuffer.empty()) {
        throw ConfigException("Unexpected garbage at end of file: " + file.GetName());
    }
    if (!block_stack.empty()) {
        if (block_stack.top()) {
            throw ConfigException("Unterminated block at end of file: " + file.GetName() +
                                  ". Block was opened on line " + stringify(block_stack.top()->linenum));
        } else {
            throw ConfigException("Unterminated commented block at end of file: " +
                                  file.GetName());
        }
    }
}
