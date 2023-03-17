/* NickServ core functions
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

class CommandNSAccess : public Command {
  private:
    void DoAdd(CommandSource &source, NickCore *nc, const Anope::string &mask) {
        if (mask.empty()) {
            this->OnSyntaxError(source, "ADD");
            return;
        }

        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        if (nc->access.size() >= Config->GetModule(
                    this->owner)->Get<unsigned>("accessmax", "32")) {
            source.Reply(_("Sorry, the maximum of %d access entries has been reached."),
                         Config->GetModule(this->owner)->Get<unsigned>("accessmax"));
            return;
        }

        if (nc->FindAccess(mask)) {
            source.Reply(_("Mask \002%s\002 already present on %s's access list."),
                         mask.c_str(), nc->display.c_str());
            return;
        }

        nc->AddAccess(mask);
        Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
            this) << "to ADD mask " << mask << " to " << nc->display;
        source.Reply(_("\002%s\002 added to %s's access list."), mask.c_str(),
                     nc->display.c_str());

        return;
    }

    void DoDel(CommandSource &source, NickCore *nc, const Anope::string &mask) {
        if (mask.empty()) {
            this->OnSyntaxError(source, "DEL");
            return;
        }

        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        if (!nc->FindAccess(mask)) {
            source.Reply(_("\002%s\002 not found on %s's access list."), mask.c_str(),
                         nc->display.c_str());
            return;
        }

        nc->EraseAccess(mask);
        Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
            this) << "to DELETE mask " << mask << " from " << nc->display;
        source.Reply(_("\002%s\002 deleted from %s's access list."), mask.c_str(),
                     nc->display.c_str());

        return;
    }

    void DoList(CommandSource &source, NickCore *nc, const Anope::string &mask) {
        unsigned i, end;

        if (nc->access.empty()) {
            source.Reply(_("%s's access list is empty."), nc->display.c_str());
            return;
        }

        source.Reply(_("Access list for %s:"), nc->display.c_str());
        for (i = 0, end = nc->access.size(); i < end; ++i) {
            Anope::string access = nc->GetAccess(i);
            if (!mask.empty() && !Anope::Match(access, mask)) {
                continue;
            }
            source.Reply("    %s", access.c_str());
        }

        return;
    }
  public:
    CommandNSAccess(Module *creator) : Command(creator, "nickserv/access", 1, 3) {
        this->SetDesc(_("Modify the list of authorized addresses"));
        this->SetSyntax(_("ADD [\037nickname\037] \037mask\037"));
        this->SetSyntax(_("DEL [\037nickname\037] \037mask\037"));
        this->SetSyntax(_("LIST [\037nickname\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &cmd = params[0];
        Anope::string nick, mask;

        if (cmd.equals_ci("LIST")) {
            nick = params.size() > 1 ? params[1] : "";
        } else {
            nick = params.size() == 3 ? params[1] : "";
            mask = params.size() > 1 ? params[params.size() - 1] : "";
        }

        NickCore *nc;
        if (!nick.empty()) {
            const NickAlias *na = NickAlias::Find(nick);
            if (na == NULL) {
                source.Reply(NICK_X_NOT_REGISTERED, nick.c_str());
                return;
            } else if (na->nc != source.GetAccount()
                       && !source.HasPriv("nickserv/access")) {
                source.Reply(ACCESS_DENIED);
                return;
            } else if (Config->GetModule("nickserv")->Get<bool>("secureadmins", "yes")
                       && source.GetAccount() != na->nc && na->nc->IsServicesOper()
                       && !cmd.equals_ci("LIST")) {
                source.Reply(
                    _("You may view but not modify the access list of other Services Operators."));
                return;
            }

            nc = na->nc;
        } else {
            nc = source.nc;
        }

        if (!mask.empty() && (mask.find('@') == Anope::string::npos || mask.find('!') != Anope::string::npos)) {
            source.Reply(BAD_USERHOST_MASK);
            source.Reply(MORE_INFO, Config->StrictPrivmsg.c_str(),
                         source.service->nick.c_str(), source.command.c_str());
        } else if (cmd.equals_ci("LIST")) {
            return this->DoList(source, nc, mask);
        } else if (nc->HasExt("NS_SUSPENDED")) {
            source.Reply(NICK_X_SUSPENDED, nc->display.c_str());
        } else if (cmd.equals_ci("ADD")) {
            return this->DoAdd(source, nc, mask);
        } else if (cmd.equals_ci("DEL")) {
            return this->DoDel(source, nc, mask);
        } else {
            this->OnSyntaxError(source, "");
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Modifies or displays the access list for your nick.  This\n"
                       "is the list of addresses which will be automatically\n"
                       "recognized by %s as allowed to use the nick.  If\n"
                       "you want to use the nick from a different address, you\n"
                       "need to send an \002IDENTIFY\002 command to make %s\n"
                       "recognize you. Services Operators may provide a nick\n"
                       "to modify other users' access lists.\n"
                       " \n"
                       "Examples:\n"
                       " \n"
                       "    \002ACCESS ADD anyone@*.bepeg.com\002\n"
                       "        Allows access to user \002anyone\002 from any machine in\n"
                       "        the \002bepeg.com\002 domain.\n"
                       " \n"
                       "    \002ACCESS DEL anyone@*.bepeg.com\002\n"
                       "        Reverses the previous command.\n"
                       " \n"
                       "    \002ACCESS LIST\002\n"
                       "        Displays the current access list."), source.service->nick.c_str(), source.service->nick.c_str());
        return true;
    }
};

class NSAccess : public Module {
    CommandNSAccess commandnsaccess;

  public:
    NSAccess(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandnsaccess(this) {
    }

    void OnNickRegister(User *u, NickAlias *na,
                        const Anope::string &) anope_override {
        if (u && Config->GetModule(this)->Get<bool>("addaccessonreg")) {
            na->nc->AddAccess(u->Mask());
        }
    }
};

MODULE_INIT(NSAccess)
