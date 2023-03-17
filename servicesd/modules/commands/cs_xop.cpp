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

namespace {
std::vector<Anope::string> order;
std::map<Anope::string, std::vector<Anope::string> > permissions;
}

class XOPChanAccess : public ChanAccess {
  public:
    Anope::string type;

    XOPChanAccess(AccessProvider *p) : ChanAccess(p) {
    }

    bool HasPriv(const Anope::string &priv) const anope_override {
        for (std::vector<Anope::string>::iterator it = std::find(order.begin(),
                order.end(), this->type); it != order.end(); ++it) {
            const std::vector<Anope::string> &privs = permissions[*it];
            if (std::find(privs.begin(), privs.end(), priv) != privs.end()) {
                return true;
            }
        }
        return false;
    }

    Anope::string AccessSerialize() const anope_override {
        return this->type;
    }

    void AccessUnserialize(const Anope::string &data) anope_override {
        this->type = data;
    }

    static Anope::string DetermineLevel(const ChanAccess *access) {
        if (access->provider->name == "access/xop") {
            const XOPChanAccess *xaccess = anope_dynamic_static_cast<const XOPChanAccess *>
                                           (access);
            return xaccess->type;
        } else {
            std::map<Anope::string, int> count;

            for (std::map<Anope::string, std::vector<Anope::string> >::const_iterator it =
                        permissions.begin(), it_end = permissions.end(); it != it_end; ++it) {
                int &c = count[it->first];
                const std::vector<Anope::string> &perms = it->second;
                for (unsigned i = 0; i < perms.size(); ++i)
                    if (access->HasPriv(perms[i])) {
                        ++c;
                    }
            }

            Anope::string max;
            int maxn = 0;
            for (std::map<Anope::string, int>::iterator it = count.begin(),
                    it_end = count.end(); it != it_end; ++it)
                if (it->second > maxn) {
                    maxn = it->second;
                    max = it->first;
                }

            return max;
        }
    }
};

class XOPAccessProvider : public AccessProvider {
  public:
    XOPAccessProvider(Module *o) : AccessProvider(o, "access/xop") {
    }

    ChanAccess *Create() anope_override {
        return new XOPChanAccess(this);
    }
};

class CommandCSXOP : public Command {
  private:
    void DoAdd(CommandSource &source, ChannelInfo *ci,
               const std::vector<Anope::string> &params) {
        Anope::string mask = params.size() > 2 ? params[2] : "";

        if (mask.empty()) {
            this->OnSyntaxError(source, "ADD");
            return;
        }

        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, channel %s list modification is temporarily disabled."),
                         source.command.c_str());
            return;
        }

        AccessGroup access = source.AccessFor(ci);
        const ChanAccess *highest = access.Highest();
        bool override = false;
        const NickAlias *na = NULL;

        std::vector<Anope::string>::iterator cmd_it = std::find(order.begin(),
                order.end(), source.command.upper()),
                access_it = highest ? std::find(order.begin(), order.end(),
                                                XOPChanAccess::DetermineLevel(highest)) : order.end();

        if (!access.founder && (!access.HasPriv("ACCESS_CHANGE")
                                || cmd_it <= access_it)) {
            if (source.HasPriv("chanserv/access/modify")) {
                override = true;
            } else {
                source.Reply(ACCESS_DENIED);
                return;
            }
        }

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

        for (unsigned i = 0; i < ci->GetAccessCount(); ++i) {
            const ChanAccess *a = ci->GetAccess(i);

            if ((na && na->nc == a->GetAccount()) || mask.equals_ci(a->Mask())) {
                if ((!highest || *a >= *highest) && !access.founder
                        && !source.HasPriv("chanserv/access/modify")) {
                    source.Reply(ACCESS_DENIED);
                    return;
                }

                delete ci->EraseAccess(i);
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

        ServiceReference<AccessProvider> provider("AccessProvider", "access/xop");
        if (!provider) {
            return;
        }
        XOPChanAccess *acc = anope_dynamic_static_cast<XOPChanAccess *>
                             (provider->Create());
        acc->SetMask(mask, ci);
        acc->creator = source.GetNick();
        acc->type = source.command.upper();
        acc->last_seen = 0;
        acc->created = Anope::CurTime;
        ci->AddAccess(acc);

        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
            ci) << "to add " << mask;

        FOREACH_MOD(OnAccessAdd, (ci, source, acc));
        source.Reply(_("\002%s\002 added to %s %s list."), acc->Mask().c_str(),
                     ci->name.c_str(), source.command.c_str());
    }

    void DoDel(CommandSource &source, ChannelInfo *ci,
               const std::vector<Anope::string> &params) {
        NickCore *nc = source.nc;
        Anope::string mask = params.size() > 2 ? params[2] : "";

        if (mask.empty()) {
            this->OnSyntaxError(source, "DEL");
            return;
        }

        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, channel %s list modification is temporarily disabled."),
                         source.command.c_str());
            return;
        }

        if (!ci->GetAccessCount()) {
            source.Reply(_("%s %s list is empty."), ci->name.c_str(),
                         source.command.c_str());
            return;
        }

        AccessGroup access = source.AccessFor(ci);
        const ChanAccess *highest = access.Highest();
        bool override = false;

        if (!isdigit(mask[0]) && mask.find_first_of("#!*@") == Anope::string::npos
                && !NickAlias::Find(mask)) {
            User *targ = User::Find(mask, true);
            if (targ != NULL) {
                mask = "*!*@" + targ->GetDisplayedHost();
            } else {
                source.Reply(NICK_X_NOT_REGISTERED, mask.c_str());
                return;
            }
        }

        std::vector<Anope::string>::iterator cmd_it = std::find(order.begin(),
                order.end(), source.command.upper()),
                access_it = highest ? std::find(order.begin(), order.end(),
                                                XOPChanAccess::DetermineLevel(highest)) : order.end();

        if (!mask.equals_ci(nc->display) && !access.founder
                && (!access.HasPriv("ACCESS_CHANGE") || cmd_it <= access_it)) {
            if (source.HasPriv("chanserv/access/modify")) {
                override = true;
            } else {
                source.Reply(ACCESS_DENIED);
                return;
            }
        }

        /* Special case: is it a number/list?  Only do search if it isn't. */
        if (isdigit(mask[0])
                && mask.find_first_not_of("1234567890,-") == Anope::string::npos) {
            class XOPDelCallback : public NumberList {
                CommandSource &source;
                ChannelInfo *ci;
                Command *c;
                unsigned deleted;
                Anope::string nicks;
                bool override;
              public:
                XOPDelCallback(CommandSource &_source, ChannelInfo *_ci, Command *_c,
                               bool _override, const Anope::string &numlist) : NumberList(numlist, true),
                    source(_source), ci(_ci), c(_c), deleted(0), override(_override) {
                }

                ~XOPDelCallback() {
                    if (!deleted) {
                        source.Reply(_("No matching entries on %s %s list."), ci->name.c_str(),
                                     source.command.c_str());
                    } else {
                        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, c,
                            ci) << "to delete " << nicks;

                        if (deleted == 1) {
                            source.Reply(_("Deleted one entry from %s %s list."), ci->name.c_str(),
                                         source.command.c_str());
                        } else {
                            source.Reply(_("Deleted %d entries from %s %s list."), deleted,
                                         ci->name.c_str(), source.command.c_str());
                        }
                    }
                }

                void HandleNumber(unsigned number) anope_override {
                    if (!number || number > ci->GetAccessCount()) {
                        return;
                    }

                    ChanAccess *caccess = ci->GetAccess(number - 1);

                    if (caccess->provider->name != "access/xop" || this->source.command.upper() != caccess->AccessSerialize()) {
                        return;
                    }

                    ++deleted;
                    if (!nicks.empty()) {
                        nicks += ", " + caccess->Mask();
                    } else {
                        nicks = caccess->Mask();
                    }

                    ci->EraseAccess(number - 1);
                    FOREACH_MOD(OnAccessDel, (ci, source, caccess));
                    delete caccess;
                }
            }
            delcallback(source, ci, this, override, mask);
            delcallback.Process();
        } else {
            for (unsigned i = 0; i < ci->GetAccessCount(); ++i) {
                ChanAccess *a = ci->GetAccess(i);

                if (a->provider->name != "access/xop"
                        || source.command.upper() != a->AccessSerialize()) {
                    continue;
                }

                if (a->Mask().equals_ci(mask)) {
                    Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
                        ci) << "to delete " << a->Mask();

                    source.Reply(_("\002%s\002 deleted from %s %s list."), a->Mask().c_str(),
                                 ci->name.c_str(), source.command.c_str());

                    ci->EraseAccess(i);
                    FOREACH_MOD(OnAccessDel, (ci, source, a));
                    delete a;

                    return;
                }
            }

            source.Reply(_("\002%s\002 not found on %s %s list."), mask.c_str(),
                         ci->name.c_str(), source.command.c_str());
        }
    }

    void DoList(CommandSource &source, ChannelInfo *ci,
                const std::vector<Anope::string> &params) {

        const Anope::string &nick = params.size() > 2 ? params[2] : "";

        AccessGroup access = source.AccessFor(ci);

        if (!access.HasPriv("ACCESS_LIST") && !source.HasPriv("chanserv/access/list")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (!ci->GetAccessCount()) {
            source.Reply(_("%s %s list is empty."), ci->name.c_str(),
                         source.command.c_str());
            return;
        }

        ListFormatter list(source.GetAccount());
        list.AddColumn(_("Number")).AddColumn(_("Mask"));

        if (!nick.empty()
                && nick.find_first_not_of("1234567890,-") == Anope::string::npos) {
            class XOPListCallback : public NumberList {
                ListFormatter &list;
                ChannelInfo *ci;
                CommandSource &source;
              public:
                XOPListCallback(ListFormatter &_list, ChannelInfo *_ci,
                                const Anope::string &numlist, CommandSource &src) : NumberList(numlist, false),
                    list(_list), ci(_ci), source(src) {
                }

                void HandleNumber(unsigned Number) anope_override {
                    if (!Number || Number > ci->GetAccessCount()) {
                        return;
                    }

                    const ChanAccess *a = ci->GetAccess(Number - 1);

                    if (a->provider->name != "access/xop" || this->source.command.upper() != a->AccessSerialize()) {
                        return;
                    }

                    ListFormatter::ListEntry entry;
                    entry["Number"] = stringify(Number);
                    entry["Mask"] = a->Mask();
                    this->list.AddEntry(entry);
                }
            } nl_list(list, ci, nick, source);
            nl_list.Process();
        } else {
            for (unsigned i = 0, end = ci->GetAccessCount(); i < end; ++i) {
                const ChanAccess *a = ci->GetAccess(i);

                if (a->provider->name != "access/xop"
                        || source.command.upper() != a->AccessSerialize()) {
                    continue;
                } else if (!nick.empty() && !Anope::Match(a->Mask(), nick)) {
                    continue;
                }

                ListFormatter::ListEntry entry;
                entry["Number"] = stringify(i + 1);
                entry["Mask"] = a->Mask();
                list.AddEntry(entry);
            }
        }

        if (list.IsEmpty()) {
            source.Reply(_("No matching entries on %s access list."), ci->name.c_str());
        } else {
            std::vector<Anope::string> replies;
            list.Process(replies);

            source.Reply(_("%s list for %s"), source.command.c_str(), ci->name.c_str());
            for (unsigned i = 0; i < replies.size(); ++i) {
                source.Reply(replies[i]);
            }
        }
    }

    void DoClear(CommandSource &source, ChannelInfo *ci) {
        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, channel %s list modification is temporarily disabled."),
                         source.command.c_str());
            return;
        }

        if (!ci->GetAccessCount()) {
            source.Reply(_("%s %s list is empty."), ci->name.c_str(),
                         source.command.c_str());
            return;
        }

        if (!source.AccessFor(ci).HasPriv("FOUNDER")
                && !source.HasPriv("chanserv/access/modify")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        bool override = !source.AccessFor(ci).HasPriv("FOUNDER");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
            ci) << "to clear the access list";

        for (unsigned i = ci->GetAccessCount(); i > 0; --i) {
            const ChanAccess *access = ci->GetAccess(i - 1);

            if (access->provider->name != "access/xop"
                    || source.command.upper() != access->AccessSerialize()) {
                continue;
            }

            delete ci->EraseAccess(i - 1);
        }

        FOREACH_MOD(OnAccessClear, (ci, source));

        source.Reply(_("Channel %s %s list has been cleared."), ci->name.c_str(),
                     source.command.c_str());
    }

  public:
    CommandCSXOP(Module *modname) : Command(modname, "chanserv/xop", 2, 4) {
        this->SetSyntax(_("\037channel\037 ADD \037mask\037"));
        this->SetSyntax(
            _("\037channel\037 DEL {\037mask\037 | \037entry-num\037 | \037list\037}"));
        this->SetSyntax(_("\037channel\037 LIST [\037mask\037 | \037list\037]"));
        this->SetSyntax(_("\037channel\037 CLEAR"));
    }

    const Anope::string GetDesc(CommandSource &source) const anope_override {
        return Anope::printf(Language::Translate(source.GetAccount(),
                             _("Modify the list of %s users")), source.command.upper().c_str());
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        const Anope::string &cmd = params[1];

        if (cmd.equals_ci("ADD")) {
            return this->DoAdd(source, ci, params);
        } else if (cmd.equals_ci("DEL")) {
            return this->DoDel(source, ci, params);
        } else if (cmd.equals_ci("LIST")) {
            return this->DoList(source, ci, params);
        } else if (cmd.equals_ci("CLEAR")) {
            return this->DoClear(source, ci);
        } else {
            this->OnSyntaxError(source, "");
        }
    }


    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        const Anope::string &cmd = source.command.upper();

        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Maintains the \002%s list\002 for a channel. Users who match an access entry\n"
                       "on the %s list receive the following privileges:\n"
                       " "), cmd.c_str(), cmd.c_str());

        Anope::string buf;
        for (unsigned i = 0; i < permissions[cmd].size(); ++i) {
            buf += ", " + permissions[cmd][i];
            if (buf.length() > 75) {
                source.Reply("  %s\n", buf.substr(2).c_str());
                buf.clear();
            }
        }
        if (!buf.empty()) {
            source.Reply("  %s\n", buf.substr(2).c_str());
            buf.clear();
        }

        source.Reply(_(" \n"
                       "The \002%s ADD\002 command adds the given nickname to the\n"
                       "%s list.\n"
                       " \n"
                       "The \002%s DEL\002 command removes the given nick from the\n"
                       "%s list. If a list of entry numbers is given, those\n"
                       "entries are deleted. (See the example for LIST below.)\n"
                       " \n"
                       "The \002%s LIST\002 command displays the %s list. If\n"
                       "a wildcard mask is given, only those entries matching the\n"
                       "mask are displayed. If a list of entry numbers is given,\n"
                       "only those entries are shown; for example:\n"
                       "   \002%s #channel LIST 2-5,7-9\002\n"
                       "      Lists %s entries numbered 2 through 5 and\n"
                       "      7 through 9.\n"
                       "      \n"
                       "The \002%s CLEAR\002 command clears all entries of the\n"
                       "%s list."), cmd.c_str(), cmd.c_str(), cmd.c_str(), cmd.c_str(),
                     cmd.c_str(), cmd.c_str(), cmd.c_str(), cmd.c_str(), cmd.c_str(), cmd.c_str());
        BotInfo *access_bi, *flags_bi;
        Anope::string access_cmd, flags_cmd;
        Command::FindCommandFromService("chanserv/access", access_bi, access_cmd);
        Command::FindCommandFromService("chanserv/flags", flags_bi, flags_cmd);
        if (!access_cmd.empty() || !flags_cmd.empty()) {
            source.Reply(_("Alternative methods of modifying channel access lists are\n"
                           "available. "));
            if (!access_cmd.empty())
                source.Reply(_("See \002%s%s HELP %s\002 for more information\n"
                               "about the access list."), Config->StrictPrivmsg.c_str(),
                             access_bi->nick.c_str(), access_cmd.c_str());
            if (!flags_cmd.empty())
                source.Reply(_("See \002%s%s HELP %s\002 for more information\n"
                               "about the flags system."), Config->StrictPrivmsg.c_str(),
                             flags_bi->nick.c_str(), flags_cmd.c_str());
        }
        return true;
    }
};

class CSXOP : public Module {
    XOPAccessProvider accessprovider;
    CommandCSXOP commandcsxop;

  public:
    CSXOP(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        accessprovider(this), commandcsxop(this) {
        this->SetPermanent(true);

    }

    void OnReload(Configuration::Conf *conf) anope_override {
        order.clear();
        permissions.clear();

        for (int i = 0; i < conf->CountBlock("privilege"); ++i) {
            Configuration::Block *block = conf->GetBlock("privilege", i);
            const Anope::string &pname = block->Get<const Anope::string>("name");

            Privilege *p = PrivilegeManager::FindPrivilege(pname);
            if (p == NULL) {
                continue;
            }

            const Anope::string &xop = block->Get<const Anope::string>("xop");
            if (pname.empty() || xop.empty()) {
                continue;
            }

            permissions[xop].push_back(pname);
        }

        for (int i = 0; i < conf->CountBlock("command"); ++i) {
            Configuration::Block *block = conf->GetBlock("command", i);
            const Anope::string &cname = block->Get<const Anope::string>("name"),
                                 &cserv = block->Get<const Anope::string>("command");
            if (cname.empty() || cserv != "chanserv/xop") {
                continue;
            }

            order.push_back(cname);
        }
    }
};

MODULE_INIT(CSXOP)
