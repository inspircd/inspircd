/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013-2014, 2016-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Justin Crawford <Justasic@Gmail.com>
 *   Copyright (C) 2012 DjSlash <djslash@djslash.org>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2011 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007-2010 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "xline.h"
#include "listmode.h"
#include "exitcodes.h"
#include "configparser.h"
#include <iostream>

ServerLimits::ServerLimits(ConfigTag* tag)
    : MaxLine(tag->getUInt("maxline", 512, 512))
    , NickMax(tag->getUInt("maxnick", 30, 1, MaxLine))
    , ChanMax(tag->getUInt("maxchan", 64, 1, MaxLine))
    , MaxModes(tag->getUInt("maxmodes", 20, 1))
    , IdentMax(tag->getUInt("maxident", 10, 1))
    , MaxQuit(tag->getUInt("maxquit", 255, 0, MaxLine))
    , MaxTopic(tag->getUInt("maxtopic", 307, 1, MaxLine))
    , MaxKick(tag->getUInt("maxkick", 255, 1, MaxLine))
    , MaxReal(tag->getUInt("maxreal", tag->getUInt("maxgecos", 128), 1, MaxLine))
    , MaxAway(tag->getUInt("maxaway", 200, 1, MaxLine))
    , MaxHost(tag->getUInt("maxhost", 64, 1, MaxLine)) {
}

ServerConfig::ServerPaths::ServerPaths(ConfigTag* tag)
    : Config(tag->getString("configdir", INSPIRCD_CONFIG_PATH, 1))
    , Data(tag->getString("datadir", INSPIRCD_DATA_PATH, 1))
    , Log(tag->getString("logdir", INSPIRCD_LOG_PATH, 1))
    , Module(tag->getString("moduledir", INSPIRCD_MODULE_PATH, 1))
    , Runtime(tag->getString("runtimedir", INSPIRCD_RUNTIME_PATH, 1)) {
}

static ConfigTag* CreateEmptyTag() {
    ConfigItems* items;
    return ConfigTag::create("empty", "<auto>", 0, items);
}

ServerConfig::ServerConfig()
    : EmptyTag(CreateEmptyTag())
    , Limits(EmptyTag)
    , Paths(EmptyTag)
    , RawLog(false)
    , NoSnoticeStack(false) {
}

ServerConfig::~ServerConfig() {
    delete EmptyTag;
}

static void ReadXLine(ServerConfig* conf, const std::string& tag,
                      const std::string& key, XLineFactory* make) {
    insp::flat_set<std::string> configlines;

    ConfigTagList tags = conf->ConfTags(tag);
    for(ConfigIter i = tags.first; i != tags.second; ++i) {
        ConfigTag* ctag = i->second;

        const std::string mask = ctag->getString(key);
        if (mask.empty()) {
            throw CoreException("<" + tag + ":" + key + "> missing at " +
                                ctag->getTagLocation());
        }

        const std::string reason = ctag->getString("reason");
        if (reason.empty()) {
            throw CoreException("<" + tag + ":reason> missing at " +
                                ctag->getTagLocation());
        }

        XLine* xl = make->Generate(ServerInstance->Time(), 0,
                                   ServerInstance->Config->ServerName, reason, mask);
        xl->from_config = true;
        configlines.insert(xl->Displayable());
        if (!ServerInstance->XLines->AddLine(xl, NULL)) {
            delete xl;
        }
    }

    ServerInstance->XLines->ExpireRemovedConfigLines(make->GetType(), configlines);
}

typedef std::map<std::string, ConfigTag*> LocalIndex;
void ServerConfig::CrossCheckOperClassType() {
    LocalIndex operclass;
    ConfigTagList tags = ConfTags("class");
    for(ConfigIter i = tags.first; i != tags.second; ++i) {
        ConfigTag* tag = i->second;
        std::string name = tag->getString("name");
        if (name.empty()) {
            throw CoreException("<class:name> missing from tag at " +
                                tag->getTagLocation());
        }
        if (operclass.find(name) != operclass.end()) {
            throw CoreException("Duplicate class block with name " + name + " at " +
                                tag->getTagLocation());
        }
        operclass[name] = tag;
    }
    tags = ConfTags("type");
    for(ConfigIter i = tags.first; i != tags.second; ++i) {
        ConfigTag* tag = i->second;
        std::string name = tag->getString("name");
        if (name.empty()) {
            throw CoreException("<type:name> is missing from tag at " +
                                tag->getTagLocation());
        }
        if (OperTypes.find(name) != OperTypes.end()) {
            throw CoreException("Duplicate type block with name " + name + " at " +
                                tag->getTagLocation());
        }

        OperInfo* ifo = new OperInfo(name);
        OperTypes[name] = ifo;
        ifo->type_block = tag;

        std::string classname;
        irc::spacesepstream str(tag->getString("classes"));
        while (str.GetToken(classname)) {
            LocalIndex::iterator cls = operclass.find(classname);
            if (cls == operclass.end()) {
                throw CoreException("Oper type " + name + " has missing class " + classname);
            }
            ifo->class_blocks.push_back(cls->second);
        }
    }

    tags = ConfTags("oper");
    for(ConfigIter i = tags.first; i != tags.second; ++i) {
        ConfigTag* tag = i->second;

        std::string name = tag->getString("name");
        if (name.empty()) {
            throw CoreException("<oper:name> missing from tag at " +
                                tag->getTagLocation());
        }

        std::string type = tag->getString("type");
        OperIndex::iterator tblk = OperTypes.find(type);
        if (tblk == OperTypes.end()) {
            throw CoreException("Oper block " + name + " has missing type " + type);
        }
        if (oper_blocks.find(name) != oper_blocks.end()) {
            throw CoreException("Duplicate oper block with name " + name + " at " +
                                tag->getTagLocation());
        }

        OperInfo* ifo = new OperInfo(type);
        ifo->oper_block = tag;
        ifo->type_block = tblk->second->type_block;
        ifo->class_blocks.assign(tblk->second->class_blocks.begin(),
                                 tblk->second->class_blocks.end());
        oper_blocks[name] = ifo;
    }
}

void ServerConfig::CrossCheckConnectBlocks(ServerConfig* current) {
    typedef std::map<std::pair<std::string, char>, ConnectClass*> ClassMap;
    ClassMap oldBlocksByMask;
    if (current) {
        for(ClassVector::iterator i = current->Classes.begin();
                i != current->Classes.end(); ++i) {
            ConnectClass* c = *i;
            switch (c->type) {
            case CC_ALLOW:
            case CC_DENY:
                oldBlocksByMask[std::make_pair(c->host, c->type)] = c;
                break;

            case CC_NAMED:
                oldBlocksByMask[std::make_pair(c->name, c->type)] = c;
                break;
            }
        }
    }

    size_t blk_count = config_data.count("connect");
    if (blk_count == 0) {
        // No connect blocks found; make a trivial default block
        ConfigItems* items;
        ConfigTag* tag = ConfigTag::create("connect", "<auto>", 0, items);
        (*items)["allow"] = "*";
        config_data.insert(std::make_pair("connect", tag));
        blk_count = 1;
    }

    Classes.resize(blk_count);
    std::map<std::string, size_t> names;

    bool try_again = true;
    for(size_t tries = 0; try_again; tries++) {
        try_again = false;
        ConfigTagList tags = ConfTags("connect");
        size_t i = 0;
        for(ConfigIter it = tags.first; it != tags.second; ++it, ++i) {
            ConfigTag* tag = it->second;
            if (Classes[i]) {
                continue;
            }

            ConnectClass* parent = NULL;
            std::string parentName = tag->getString("parent");
            if (!parentName.empty()) {
                std::map<std::string, size_t>::const_iterator parentIter = names.find(
                            parentName);
                if (parentIter == names.end()) {
                    try_again = true;
                    // couldn't find parent this time. If it's the last time, we'll never find it.
                    if (tries >= blk_count) {
                        throw CoreException("Could not find parent connect class \"" + parentName +
                                            "\" for connect block at " + tag->getTagLocation());
                    }
                    continue;
                }
                parent = Classes[parentIter->second];
            }

            std::string name = tag->getString("name");
            std::string mask;
            char type;

            if (tag->readString("allow", mask, false) && !mask.empty()) {
                type = CC_ALLOW;
            } else if (tag->readString("deny", mask, false) && !mask.empty()) {
                type = CC_DENY;
            } else if (!name.empty()) {
                type = CC_NAMED;
                mask = name;
            } else {
                throw CoreException("Connect class must have allow, deny, or name specified at "
                                    + tag->getTagLocation());
            }

            if (name.empty()) {
                name = "unnamed-" + ConvToStr(i);
            }

            if (names.find(name) != names.end()) {
                throw CoreException("Two connect classes with name \"" + name + "\" defined!");
            }
            names[name] = i;

            ConnectClass* me = parent ?
                               new ConnectClass(tag, type, mask, *parent) :
                               new ConnectClass(tag, type, mask);

            me->name = name;

            me->registration_timeout = tag->getDuration("timeout",
                                       me->registration_timeout);
            me->pingtime = tag->getDuration("pingfreq", me->pingtime);
            std::string sendq;
            if (tag->readString("sendq", sendq)) {
                // attempt to guess a good hard/soft sendq from a single value
                unsigned long value = strtoul(sendq.c_str(), NULL, 10);
                if (value > 16384) {
                    me->softsendqmax = value / 16;
                } else {
                    me->softsendqmax = value;
                }
                me->hardsendqmax = value * 8;
            }
            me->softsendqmax = tag->getUInt("softsendq", me->softsendqmax);
            me->hardsendqmax = tag->getUInt("hardsendq", me->hardsendqmax);
            me->recvqmax = tag->getUInt("recvq", me->recvqmax);
            me->penaltythreshold = tag->getUInt("threshold", me->penaltythreshold);
            me->commandrate = tag->getUInt("commandrate", me->commandrate);
            me->fakelag = tag->getBool("fakelag", me->fakelag);
            me->maxlocal = tag->getUInt("localmax", me->maxlocal);
            me->maxglobal = tag->getUInt("globalmax", me->maxglobal);
            me->maxchans = tag->getUInt("maxchans", me->maxchans);
            me->maxconnwarn = tag->getBool("maxconnwarn", me->maxconnwarn);
            me->limit = tag->getUInt("limit", me->limit);
            me->resolvehostnames = tag->getBool("resolvehostnames", me->resolvehostnames);
            me->uniqueusername = tag->getBool("uniqueusername", me->uniqueusername);
            me->password = tag->getString("password", me->password);

            me->passwordhash = tag->getString("hash", me->passwordhash);
            if (!me->password.empty() && (me->passwordhash.empty()
                                          || stdalgo::string::equalsci(me->passwordhash, "plaintext"))) {
                ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEFAULT,
                                          "<connect> tag '%s' at %s contains an plain text password, this is insecure!",
                                          name.c_str(), tag->getTagLocation().c_str());
            }

            std::string ports = tag->getString("port");
            if (!ports.empty()) {
                irc::portparser portrange(ports, false);
                while (int port = portrange.GetToken()) {
                    me->ports.insert(port);
                }
            }

            ClassMap::iterator oldMask = oldBlocksByMask.find(std::make_pair(mask,
                                         me->type));
            if (oldMask != oldBlocksByMask.end()) {
                ConnectClass* old = oldMask->second;
                oldBlocksByMask.erase(oldMask);
                old->Update(me);
                delete me;
                me = old;
            }
            Classes[i] = me;
        }
    }
}

static std::string GetServerHost() {
#ifndef _WIN32
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        std::string name(hostname);
        if (name.find('.') == std::string::npos) {
            name.append(".local");
        }

        if (name.length() <= ServerInstance->Config->Limits.MaxHost
                && InspIRCd::IsHost(name)) {
            return name;
        }
    }
#endif
    return "irc.example.com";
}

void ServerConfig::Fill() {
    ConfigTag* options = ConfValue("options");
    ConfigTag* security = ConfValue("security");
    ConfigTag* server = ConfValue("server");
    if (sid.empty()) {
        ServerName = server->getString("name", GetServerHost(), InspIRCd::IsHost);

        sid = server->getString("id");
        if (!sid.empty() && !InspIRCd::IsSID(sid)) {
            throw CoreException(sid +
                                " is not a valid server ID. A server ID must be 3 characters long, with the first character a digit and the next two characters a digit or letter.");
        }

        CaseMapping = options->getString("casemapping", "rfc1459", 1);
        if (CaseMapping == "ascii") {
            national_case_insensitive_map = ascii_case_insensitive_map;
        } else if (CaseMapping == "rfc1459") {
            national_case_insensitive_map = rfc_case_insensitive_map;
        } else {
            throw CoreException("<options:casemapping> must be set to 'ascii', or 'rfc1459'");
        }
    } else {
        std::string name = server->getString("name");
        if (!name.empty() && name != ServerName) {
            throw CoreException("You must restart to change the server name");
        }

        std::string nsid = server->getString("id");
        if (!nsid.empty() && nsid != sid) {
            throw CoreException("You must restart to change the server id");
        }

        std::string casemapping = options->getString("casemapping");
        // Ignore this value if CaseMapping is set to something the core doesn't provide (i.e., m_nationalchars).
        if (!casemapping.empty() && casemapping != CaseMapping
                && (CaseMapping == "ascii" || CaseMapping == "rfc1459")) {
            throw CoreException("You must restart to change the server casemapping");
        }

    }
    SoftLimit = ConfValue("performance")->getUInt("softlimit",
                (SocketEngine::GetMaxFds() > 0 ? SocketEngine::GetMaxFds() : LONG_MAX), 10);
    CCOnConnect = ConfValue("performance")->getBool("clonesonconnect", true);
    MaxConn = ConfValue("performance")->getUInt("somaxconn", SOMAXCONN);
    TimeSkipWarn = ConfValue("performance")->getDuration("timeskipwarn", 2, 0, 30);
    XLineMessage = options->getString("xlinemessage",
                                      options->getString("moronbanner", "You're banned!"));
    ServerDesc = server->getString("description", "Configure Me", 1);
    Network = server->getString("network", "Network", 1);
    NetBufferSize = ConfValue("performance")->getInt("netbuffersize", 10240, 1024,
                    65534);
    CustomVersion = security->getString("customversion");
    HideBans = security->getBool("hidebans");
    HideServer = security->getString("hideserver",
                                     security->getString("hidewhois"));
    SyntaxHints = options->getBool("syntaxhints");
    FullHostInTopic = options->getBool("hostintopic");
    MaxTargets = security->getUInt("maxtargets", 20, 1, 31);
    DefaultModes = options->getString("defaultmodes", "not");
    PID = ConfValue("pid")->getString("file");
    MaxChans = ConfValue("channels")->getUInt("users", 20);
    OperMaxChans = ConfValue("channels")->getUInt("opers", 0);
    c_ipv4_range = ConfValue("cidr")->getUInt("ipv4clone", 32, 1, 32);
    c_ipv6_range = ConfValue("cidr")->getUInt("ipv6clone", 128, 1, 128);
    Limits = ServerLimits(ConfValue("limits"));
    Paths = ServerPaths(ConfValue("path"));
    NoSnoticeStack = options->getBool("nosnoticestack", false);

    std::string defbind = options->getString("defaultbind");
    if (stdalgo::string::equalsci(defbind, "ipv4")) {
        WildcardIPv6 = false;
    } else if (stdalgo::string::equalsci(defbind, "ipv6")) {
        WildcardIPv6 = true;
    } else {
        WildcardIPv6 = true;
        int socktest = socket(AF_INET6, SOCK_STREAM, 0);
        if (socktest < 0) {
            WildcardIPv6 = false;
        } else {
            SocketEngine::Close(socktest);
        }
    }

    ReadXLine(this, "badip", "ipmask", ServerInstance->XLines->GetFactory("Z"));
    ReadXLine(this, "badnick", "nick", ServerInstance->XLines->GetFactory("Q"));
    ReadXLine(this, "badhost", "host", ServerInstance->XLines->GetFactory("K"));
    ReadXLine(this, "exception", "host", ServerInstance->XLines->GetFactory("E"));

    const std::string restrictbannedusers =
        options->getString("restrictbannedusers", "yes", 1);
    if (stdalgo::string::equalsci(restrictbannedusers, "no")) {
        RestrictBannedUsers = ServerConfig::BUT_NORMAL;
    } else if (stdalgo::string::equalsci(restrictbannedusers, "silent")) {
        RestrictBannedUsers = ServerConfig::BUT_RESTRICT_SILENT;
    } else if (stdalgo::string::equalsci(restrictbannedusers, "yes")) {
        RestrictBannedUsers =  ServerConfig::BUT_RESTRICT_NOTIFY;
    } else {
        throw CoreException(restrictbannedusers +
                            " is an invalid <options:restrictbannedusers> value, at " +
                            options->getTagLocation());
    }
}

// WARNING: it is not safe to use most of the codebase in this function, as it
// will run in the config reader thread
void ServerConfig::Read() {
    /* Load and parse the config file, if there are any errors then explode */

    ParseStack stack(this);
    try {
        valid = stack.ParseFile(ServerInstance->ConfigFileName, 0);
    } catch (CoreException& err) {
        valid = false;
        errstr << err.GetReason() << std::endl;
    }
}

void ServerConfig::Apply(ServerConfig* old, const std::string &useruid) {
    valid = true;
    if (old) {
        /*
         * These values can only be set on boot. Keep their old values. Do it before we send messages so we actually have a servername.
         */
        this->CaseMapping = old->CaseMapping;
        this->ServerName = old->ServerName;
        this->sid = old->sid;
        this->cmdline = old->cmdline;
    }

    /* The stuff in here may throw CoreException, be sure we're in a position to catch it. */
    try {
        // Ensure the user has actually edited ther config.
        ConfigTagList dietags = ConfTags("die");
        if (dietags.first != dietags.second) {
            errstr << "Your configuration has not been edited correctly!" << std::endl;
            for (ConfigIter iter = dietags.first; iter != dietags.second; ++iter) {
                ConfigTag* tag = iter->second;
                const std::string reason = tag->getString("reason",
                                           "You left a <die> tag in your config", 1);
                errstr << reason <<  " (at " << tag->getTagLocation() << ")" << std::endl;
            }
        }

        Fill();

        // Handle special items
        CrossCheckOperClassType();
        CrossCheckConnectBlocks(old);
    } catch (CoreException &ce) {
        errstr << ce.GetReason() << std::endl;
    }

    // Check errors before dealing with failed binds, since continuing on failed bind is wanted in some circumstances.
    valid = errstr.str().empty();

    // write once here, to try it out and make sure its ok
    if (valid) {
        ServerInstance->WritePID(this->PID, !old);
    }

    ConfigTagList binds = ConfTags("bind");
    if (binds.first == binds.second)
        errstr << "Possible configuration error: you have not defined any <bind> blocks."
               << std::endl
               << "You will need to do this if you want clients to be able to connect!" <<
               std::endl;

    if (old && valid) {
        // On first run, ports are bound later on
        FailedPortList pl;
        ServerInstance->BindPorts(pl);
        if (!pl.empty()) {
            errstr << "Warning! Some of your listener" << (pl.size() == 1 ? "s" : "") <<
                   " failed to bind:" << std::endl;
            for (FailedPortList::const_iterator iter = pl.begin(); iter != pl.end();
                    ++iter) {
                const FailedPort& fp = *iter;
                errstr << "  " << fp.sa.str() << ": " << strerror(fp.error) << std::endl
                       << "  " << "Created from <bind> tag at " << fp.tag->getTagLocation() <<
                       std::endl;
            }
        }
    }

    User* user = useruid.empty() ? NULL : ServerInstance->FindUUID(useruid);

    if (!valid) {
        ServerInstance->Logs->Log("CONFIG", LOG_DEFAULT,
                                  "There were errors in your configuration file:");
        Classes.clear();
    }

    while (errstr.good()) {
        std::string line;
        getline(errstr, line, '\n');
        if (line.empty()) {
            continue;
        }
        // On startup, print out to console (still attached at this point)
        if (!old) {
            std::cout << line << std::endl;
        }
        // If a user is rehashing, tell them directly
        if (user) {
            user->WriteRemoteNotice(InspIRCd::Format("*** %s", line.c_str()));
        }
        // Also tell opers
        ServerInstance->SNO->WriteGlobalSno('a', line);
    }

    errstr.clear();
    errstr.str(std::string());

    /* No old configuration -> initial boot, nothing more to do here */
    if (!old) {
        if (!valid) {
            ServerInstance->Exit(EXIT_STATUS_CONFIG);
        }

        return;
    }


    // If there were errors processing configuration, don't touch modules.
    if (!valid) {
        return;
    }

    ApplyModules(user);

    if (user) {
        user->WriteRemoteNotice("*** Successfully rehashed server.");
    }
    ServerInstance->SNO->WriteGlobalSno('a', "*** Successfully rehashed server.");
}

void ServerConfig::ApplyModules(User* user) {
    std::vector<std::string> added_modules;
    ModuleManager::ModuleMap removed_modules =
        ServerInstance->Modules->GetModules();

    ConfigTagList tags = ConfTags("module");
    for(ConfigIter i = tags.first; i != tags.second; ++i) {
        ConfigTag* tag = i->second;
        std::string name;
        if (tag->readString("name", name)) {
            name = ModuleManager::ExpandModName(name);
            // if this module is already loaded, the erase will succeed, so we need do nothing
            // otherwise, we need to add the module (which will be done later)
            if (removed_modules.erase(name) == 0) {
                added_modules.push_back(name);
            }
        }
    }

    for (ModuleManager::ModuleMap::iterator i = removed_modules.begin();
            i != removed_modules.end(); ++i) {
        const std::string& modname = i->first;
        // Don't remove core_*.so, just remove m_*.so
        if (InspIRCd::Match(modname, "core_*.so", ascii_case_insensitive_map)) {
            continue;
        }
        if (ServerInstance->Modules->Unload(i->second)) {
            ServerInstance->SNO->WriteGlobalSno('a', "*** REHASH UNLOADED MODULE: %s",
                                                modname.c_str());

            if (user) {
                user->WriteNumeric(RPL_UNLOADEDMODULE, modname,
                                   InspIRCd::Format("The %s module was unloaded.", modname.c_str()));
            } else {
                ServerInstance->SNO->WriteGlobalSno('a', "The %s module was unloaded.",
                                                    modname.c_str());
            }
        } else {
            if (user) {
                user->WriteNumeric(ERR_CANTUNLOADMODULE, modname,
                                   InspIRCd::Format("Failed to unload the %s module: %s", modname.c_str(),
                                                    ServerInstance->Modules->LastError().c_str()));
            } else {
                ServerInstance->SNO->WriteGlobalSno('a', "Failed to unload the %s module: %s",
                                                    modname.c_str(), ServerInstance->Modules->LastError().c_str());
            }
        }
    }

    for (std::vector<std::string>::iterator adding = added_modules.begin();
            adding != added_modules.end(); adding++) {
        // Skip modules which are already loaded.
        if (ServerInstance->Modules->Find(*adding)) {
            continue;
        }

        if (ServerInstance->Modules->Load(*adding)) {
            ServerInstance->SNO->WriteGlobalSno('a', "*** REHASH LOADED MODULE: %s",
                                                adding->c_str());
            if (user) {
                user->WriteNumeric(RPL_LOADEDMODULE, *adding,
                                   InspIRCd::Format("The %s module was loaded.", adding->c_str()));
            } else {
                ServerInstance->SNO->WriteGlobalSno('a', "The %s module was loaded.",
                                                    adding->c_str());
            }
        } else {
            if (user) {
                user->WriteNumeric(ERR_CANTLOADMODULE, *adding,
                                   InspIRCd::Format("Failed to load the %s module: %s", adding->c_str(),
                                                    ServerInstance->Modules->LastError().c_str()));
            } else {
                ServerInstance->SNO->WriteGlobalSno('a', "Failed to load the %s module: %s",
                                                    adding->c_str(), ServerInstance->Modules->LastError().c_str());
            }
        }
    }
}

ConfigTag* ServerConfig::ConfValue(const std::string &tag) {
    ConfigTagList found = config_data.equal_range(tag);
    if (found.first == found.second) {
        return EmptyTag;
    }
    ConfigTag* rv = found.first->second;
    found.first++;
    if (found.first != found.second)
        ServerInstance->Logs->Log("CONFIG", LOG_DEFAULT,
                                  "Multiple <" + tag + "> tags found; only first will be used "
                                  "(first at " + rv->getTagLocation() + "; second at " +
                                  found.first->second->getTagLocation() + ")");
    return rv;
}

ConfigTagList ServerConfig::ConfTags(const std::string& tag) {
    return config_data.equal_range(tag);
}

std::string ServerConfig::Escape(const std::string& str, bool xml) {
    std::string escaped;
    for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
        switch (*it) {
        case '"':
            escaped += xml ? "&quot;" : "\"";
            break;
        case '&':
            escaped += xml ? "&amp;" : "&";
            break;
        case '\\':
            escaped += xml ? "\\" : "\\\\";
            break;
        default:
            escaped += *it;
            break;
        }
    }
    return escaped;
}

void ConfigReaderThread::Run() {
    Config->Read();
    done = true;
}

void ConfigReaderThread::Finish() {
    ServerConfig* old = ServerInstance->Config;
    ServerInstance->Logs->Log("CONFIG", LOG_DEBUG,
                              "Switching to new configuration...");
    ServerInstance->Config = this->Config;
    Config->Apply(old, TheUserUID);

    if (Config->valid) {
        /*
         * Apply the changed configuration from the rehash.
         *
         * XXX: The order of these is IMPORTANT, do not reorder them without testing
         * thoroughly!!!
         */
        ServerInstance->Users.RehashCloneCounts();
        ServerInstance->XLines->CheckELines();
        ServerInstance->XLines->ApplyLines();
        User* user = ServerInstance->FindUUID(TheUserUID);

        ConfigStatus status(user);
        const ModuleManager::ModuleMap& mods = ServerInstance->Modules->GetModules();
        for (ModuleManager::ModuleMap::const_iterator i = mods.begin(); i != mods.end();
                ++i) {
            try {
                ServerInstance->Logs->Log("MODULE", LOG_DEBUG, "Rehashing " + i->first);
                i->second->ReadConfig(status);
            } catch (CoreException& modex) {
                ServerInstance->Logs->Log("MODULE", LOG_DEFAULT,
                                          "Exception caught: " + modex.GetReason());
                if (user) {
                    user->WriteNotice(i->first + ": " + modex.GetReason());
                }
            }
        }

        // The description of this server may have changed - update it for WHOIS etc.
        ServerInstance->FakeClient->server->description = Config->ServerDesc;

        ServerInstance->ISupport.Build();

        ServerInstance->Logs->CloseLogs();
        ServerInstance->Logs->OpenFileLogs();

        if (Config->RawLog && !old->RawLog) {
            ServerInstance->Users->ServerNoticeAll("*** Raw I/O logging is enabled on this server. All messages, passwords, and commands are being recorded.");
        }

        Config = old;
    } else {
        // whoops, abort!
        ServerInstance->Config = old;
    }
}
