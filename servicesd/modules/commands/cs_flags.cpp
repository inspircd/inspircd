/* ChanServ core functions
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

static std::map<Anope::string, char> defaultFlags;

class FlagsChanAccess : public ChanAccess {
  public:
    std::set<char> flags;

    FlagsChanAccess(AccessProvider *p) : ChanAccess(p) {
    }

    bool HasPriv(const Anope::string &priv) const anope_override {
        std::map<Anope::string, char>::iterator it = defaultFlags.find(priv);
        if (it != defaultFlags.end() && this->flags.count(it->second) > 0) {
            return true;
        }
        return false;
    }

    Anope::string AccessSerialize() const anope_override {
        return Anope::string(this->flags.begin(), this->flags.end());
    }

    void AccessUnserialize(const Anope::string &data) anope_override {
        for (unsigned i = data.length(); i > 0; --i) {
            this->flags.insert(data[i - 1]);
        }
    }

    static Anope::string DetermineFlags(const ChanAccess *access) {
        if (access->provider->name == "access/flags") {
            return access->AccessSerialize();
        }

        std::set<char> buffer;

        for (std::map<Anope::string, char>::iterator it = defaultFlags.begin(),
                it_end = defaultFlags.end(); it != it_end; ++it)
            if (access->HasPriv(it->first)) {
                buffer.insert(it->second);
            }

        if (buffer.empty()) {
            return "(none)";
        } else {
            return Anope::string(buffer.begin(), buffer.end());
        }
    }
};

class FlagsAccessProvider : public AccessProvider {
  public:
    static FlagsAccessProvider *ap;

    FlagsAccessProvider(Module *o) : AccessProvider(o, "access/flags") {
        ap = this;
    }

    ChanAccess *Create() anope_override {
        return new FlagsChanAccess(this);
    }
};
FlagsAccessProvider* FlagsAccessProvider::ap;

class CommandCSFlags : public Command {
    void DoModify(CommandSource &source, ChannelInfo *ci, Anope::string mask,
                  const Anope::string &flags) {
        if (flags.empty()) {
            this->OnSyntaxError(source, "");
            return;
        }

        AccessGroup u_access = source.AccessFor(ci);
        const ChanAccess *highest = u_access.Highest();
        const NickAlias *na = NULL;

        if (IRCD->IsChannelValid(mask)) {
            if (Config->GetModule("chanserv")->Get<bool>("disallow_channel_access")) {
                source.Reply(_("Channels may not be on access lists."));
                return;
            }

            ChannelInfo *targ_ci = ChannelInfo::Find(mask);
            if (targ_ci == NULL) {
                source.Reply(CHAN_X_NOT_REGISTERED, mask.c_str());
                return;
            } else if (ci == targ_ci) {
                source.Reply(_("You can't add a channel to its own access list."));
                return;
            }

            mask = targ_ci->name;
        } else {
            na = NickAlias::Find(mask);
            if (!na &&
                    Config->GetModule("chanserv")->Get<bool>("disallow_hostmask_access")) {
                source.Reply(_("Masks and unregistered users may not be on access lists."));
                return;
            } else if (mask.find_first_of("!*@") == Anope::string::npos && !na) {
                User *targ = User::Find(mask, true);
                if (targ != NULL) {
                    mask = "*!*@" + targ->GetDisplayedHost();
                } else {
                    source.Reply(NICK_X_NOT_REGISTERED, mask.c_str());
                    return;
                }
            }

            if (na) {
                mask = na->nick;
            }
        }

        ChanAccess *current = NULL;
        unsigned current_idx;
        std::set<char> current_flags;
        bool override = false;
        for (current_idx = ci->GetAccessCount(); current_idx > 0; --current_idx) {
            ChanAccess *access = ci->GetAccess(current_idx - 1);
            if ((na && na->nc == access->GetAccount()) || mask.equals_ci(access->Mask())) {
                // Flags allows removing others that have the same access as you,
                // but no other access system does.
                if (highest && highest->provider != FlagsAccessProvider::ap
                        && !u_access.founder)
                    // operator<= on the non-me entry!
                    if (*highest <= *access) {
                        if (source.HasPriv("chanserv/access/modify")) {
                            override = true;
                        } else {
                            source.Reply(ACCESS_DENIED);
                            return;
                        }
                    }

                current = access;
                Anope::string cur_flags = FlagsChanAccess::DetermineFlags(access);
                for (unsigned j = cur_flags.length(); j > 0; --j) {
                    current_flags.insert(cur_flags[j - 1]);
                }
                break;
            }
        }

        unsigned access_max = Config->GetModule("chanserv")->Get<unsigned>("accessmax",
                              "1024");
        if (access_max && ci->GetDeepAccessCount() >= access_max) {
            source.Reply(
                _("Sorry, you can only have %d access entries on a channel, including access entries from other channels."),
                access_max);
            return;
        }

        Privilege *p = NULL;
        bool add = true;
        for (size_t i = 0; i < flags.length(); ++i) {
            char f = flags[i];
            switch (f) {
            case '+':
                add = true;
                break;
            case '-':
                add = false;
                break;
            case '*':
                for (std::map<Anope::string, char>::iterator it = defaultFlags.begin(),
                        it_end = defaultFlags.end(); it != it_end; ++it) {
                    bool has = current_flags.count(it->second);
                    // If we are adding a flag they already have or removing one they don't have, don't bother
                    if (add == has) {
                        continue;
                    }

                    if (!u_access.HasPriv(it->first) && !u_access.founder) {
                        if (source.HasPriv("chanserv/access/modify")) {
                            override = true;
                        } else {
                            continue;
                        }
                    }

                    if (add) {
                        current_flags.insert(it->second);
                    } else {
                        current_flags.erase(it->second);
                    }
                }
                break;
            default:
                p = PrivilegeManager::FindPrivilege(flags.substr(i));
                if (p != NULL && defaultFlags[p->name]) {
                    f = defaultFlags[p->name];
                    i = flags.length();
                }

                for (std::map<Anope::string, char>::iterator it = defaultFlags.begin(),
                        it_end = defaultFlags.end(); it != it_end; ++it) {
                    if (f != it->second) {
                        continue;
                    } else if (!u_access.HasPriv(it->first) && !u_access.founder) {
                        if (source.HasPriv("chanserv/access/modify")) {
                            override = true;
                        } else {
                            source.Reply(_("You cannot set the \002%c\002 flag."), f);
                            break;
                        }
                    }
                    if (add) {
                        current_flags.insert(f);
                    } else {
                        current_flags.erase(f);
                    }
                    break;
                }
            }
        }
        if (current_flags.empty()) {
            if (current != NULL) {
                ci->EraseAccess(current_idx - 1);
                FOREACH_MOD(OnAccessDel, (ci, source, current));
                delete current;
                Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                    ci) << "to delete " << mask;
                source.Reply(_("\002%s\002 removed from the %s access list."), mask.c_str(),
                             ci->name.c_str());
            } else {
                source.Reply(_("\002%s\002 not found on %s access list."), mask.c_str(),
                             ci->name.c_str());
            }
            return;
        }

        ServiceReference<AccessProvider> provider("AccessProvider", "access/flags");
        if (!provider) {
            return;
        }
        FlagsChanAccess *access = anope_dynamic_static_cast<FlagsChanAccess *>
                                  (provider->Create());
        access->SetMask(mask, ci);
        access->creator = source.GetNick();
        access->last_seen = current ? current->last_seen : 0;
        access->created = Anope::CurTime;
        access->flags = current_flags;

        if (current != NULL) {
            delete current;
        }

        ci->AddAccess(access);

        FOREACH_MOD(OnAccessAdd, (ci, source, access));

        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
            ci) << "to modify " << mask << "'s flags to " << access->AccessSerialize();
        if (p != NULL) {
            if (add) {
                source.Reply(
                    _("Privilege \002%s\002 added to \002%s\002 on \002%s\002, new flags are +\002%s\002"),
                    p->name.c_str(), access->Mask().c_str(), ci->name.c_str(),
                    access->AccessSerialize().c_str());
            } else {
                source.Reply(
                    _("Privilege \002%s\002 removed from \002%s\002 on \002%s\002, new flags are +\002%s\002"),
                    p->name.c_str(), access->Mask().c_str(), ci->name.c_str(),
                    access->AccessSerialize().c_str());
            }
        } else {
            source.Reply(_("Flags for \002%s\002 on %s set to +\002%s\002"),
                         access->Mask().c_str(), ci->name.c_str(), access->AccessSerialize().c_str());
        }
    }

    void DoList(CommandSource &source, ChannelInfo *ci,
                const std::vector<Anope::string> &params) {
        const Anope::string &arg = params.size() > 2 ? params[2] : "";

        if (!ci->GetAccessCount()) {
            source.Reply(_("%s access list is empty."), ci->name.c_str());
            return;
        }

        ListFormatter list(source.GetAccount());

        list.AddColumn(_("Number")).AddColumn(_("Mask")).AddColumn(
            _("Flags")).AddColumn(_("Creator")).AddColumn(_("Created"));

        unsigned count = 0;
        for (unsigned i = 0, end = ci->GetAccessCount(); i < end; ++i) {
            const ChanAccess *access = ci->GetAccess(i);
            const Anope::string &flags = FlagsChanAccess::DetermineFlags(access);

            if (!arg.empty()) {
                if (arg[0] == '+') {
                    bool pass = true;
                    for (size_t j = 1; j < arg.length(); ++j)
                        if (flags.find(arg[j]) == Anope::string::npos) {
                            pass = false;
                        }
                    if (pass == false) {
                        continue;
                    }
                } else if (!Anope::Match(access->Mask(), arg)) {
                    continue;
                }
            }

            ListFormatter::ListEntry entry;
            ++count;
            entry["Number"] = stringify(i + 1);
            entry["Mask"] = access->Mask();
            entry["Flags"] = flags;
            entry["Creator"] = access->creator;
            entry["Created"] = Anope::strftime(access->created, source.nc, true);
            list.AddEntry(entry);
        }

        if (list.IsEmpty()) {
            source.Reply(_("No matching entries on %s access list."), ci->name.c_str());
        } else {
            std::vector<Anope::string> replies;
            list.Process(replies);

            source.Reply(_("Flags list for %s"), ci->name.c_str());
            for (unsigned i = 0; i < replies.size(); ++i) {
                source.Reply(replies[i]);
            }
            if (count == ci->GetAccessCount()) {
                source.Reply(_("End of access list."));
            } else {
                source.Reply(_("End of access list - %d/%d entries shown."), count,
                             ci->GetAccessCount());
            }
        }
    }

    void DoClear(CommandSource &source, ChannelInfo *ci) {
        if (!source.IsFounder(ci) && !source.HasPriv("chanserv/access/modify")) {
            source.Reply(ACCESS_DENIED);
        } else {
            ci->ClearAccess();

            FOREACH_MOD(OnAccessClear, (ci, source));

            source.Reply(_("Channel %s access list has been cleared."), ci->name.c_str());

            bool override = !source.IsFounder(ci);
            Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                ci) << "to clear the access list";
        }

        return;
    }

  public:
    CommandCSFlags(Module *creator) : Command(creator, "chanserv/flags", 1, 4) {
        this->SetDesc(_("Modify the list of privileged users"));
        this->SetSyntax(_("\037channel\037 [MODIFY] \037mask\037 \037changes\037"));
        this->SetSyntax(_("\037channel\037 LIST [\037mask\037 | +\037flags\037]"));
        this->SetSyntax(_("\037channel\037 CLEAR"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &chan = params[0];
        const Anope::string &cmd = params.size() > 1 ? params[1] : "";

        ChannelInfo *ci = ChannelInfo::Find(chan);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, chan.c_str());
            return;
        }

        bool is_list = cmd.empty() || cmd.equals_ci("LIST");
        bool has_access = false;
        if (source.HasPriv("chanserv/access/modify")) {
            has_access = true;
        } else if (is_list && source.HasPriv("chanserv/access/list")) {
            has_access = true;
        } else if (is_list && source.AccessFor(ci).HasPriv("ACCESS_LIST")) {
            has_access = true;
        } else if (source.AccessFor(ci).HasPriv("ACCESS_CHANGE")) {
            has_access = true;
        }

        if (!has_access) {
            source.Reply(ACCESS_DENIED);
        } else if (Anope::ReadOnly && !is_list) {
            source.Reply(
                _("Sorry, channel access list modification is temporarily disabled."));
        } else if (is_list) {
            this->DoList(source, ci, params);
        } else if (cmd.equals_ci("CLEAR")) {
            this->DoClear(source, ci);
        } else {
            Anope::string mask, flags;
            if (cmd.equals_ci("MODIFY")) {
                mask = params.size() > 2 ? params[2] : "";
                flags = params.size() > 3 ? params[3] : "";
            } else {
                mask = cmd;
                flags = params.size() > 2 ? params[2] : "";
            }

            this->DoModify(source, ci, mask, flags);
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("%s is another way to modify the channel access list, similar to\n"
                       "the XOP and ACCESS methods."), source.command.c_str());
        source.Reply(" ");
        source.Reply(_("The \002MODIFY\002 command allows you to modify the access list. If the mask is\n"
                       "not already on the access list it is added, then the changes are applied.\n"
                       "If the mask has no more flags, then the mask is removed from the access list.\n"
                       "Additionally, you may use +* or -* to add or remove all flags, respectively. You are\n"
                       "only able to modify the access list if you have the proper permission on the channel,\n"
                       "and even then you can only give other people access to the equivalent of what your access is."));
        source.Reply(" ");
        source.Reply(_("The \002LIST\002 command allows you to list existing entries on the channel access list.\n"
                       "If a mask is given, the mask is wildcard matched against all existing entries on the\n"
                       "access list, and only those entries are returned. If a set of flags is given, only those\n"
                       "on the access list with the specified flags are returned."));
        source.Reply(" ");
        source.Reply(_("The \002CLEAR\002 command clears the channel access list. This requires channel founder access."));
        source.Reply(" ");
        source.Reply(_("The available flags are:"));

        typedef std::multimap<char, Anope::string, ci::less> reverse_map;
        reverse_map reverse;
        for (std::map<Anope::string, char>::iterator it = defaultFlags.begin(), it_end = defaultFlags.end(); it != it_end; ++it) {
            reverse.insert(std::make_pair(it->second, it->first));
        }

        for (reverse_map::iterator it = reverse.begin(), it_end = reverse.end(); it != it_end; ++it) {
            Privilege *p = PrivilegeManager::FindPrivilege(it->second);
            if (p == NULL) {
                continue;
            }
            source.Reply("  %c - %s", it->first, Language::Translate(source.nc,
                         p->desc.c_str()));
        }

        return true;
    }
};

class CSFlags : public Module {
    FlagsAccessProvider accessprovider;
    CommandCSFlags commandcsflags;

  public:
    CSFlags(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, VENDOR),
        accessprovider(this), commandcsflags(this) {
        this->SetPermanent(true);

    }

    void OnReload(Configuration::Conf *conf) anope_override {
        defaultFlags.clear();

        for (int i = 0; i < conf->CountBlock("privilege"); ++i) {
            Configuration::Block *priv = conf->GetBlock("privilege", i);

            const Anope::string &pname = priv->Get<const Anope::string>("name");

            Privilege *p = PrivilegeManager::FindPrivilege(pname);
            if (p == NULL) {
                continue;
            }

            const Anope::string &value = priv->Get<const Anope::string>("flag");
            if (value.empty()) {
                continue;
            }

            defaultFlags[p->name] = value[0];
        }
    }
};

MODULE_INIT(CSFlags)
