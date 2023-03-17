/* ns_maxemail.cpp - Limit the amount of times an email address
 *                   can be used for a NickServ account.
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Included in the Anope module pack since Anope 1.7.9
 * Anope Coder: GeniusDex <geniusdex@anope.org>
 *
 * Please read COPYING and README for further details.
 */

#include "module.h"

class NSMaxEmail : public Module {
    bool clean;

    /* strip dots from username, and remove anything after the first + */
    Anope::string CleanMail(const Anope::string &email) {
        size_t host = email.find('@');
        if (host == Anope::string::npos) {
            return email;
        }

        Anope::string username = email.substr(0, host);
        username = username.replace_all_cs(".", "");

        size_t sz = username.find('+');
        if (sz != Anope::string::npos) {
            username = username.substr(0, sz);
        }

        Anope::string cleaned = username + email.substr(host);
        Log(LOG_DEBUG) << "cleaned " << email << " to " << cleaned;
        return cleaned;
    }

    bool CheckLimitReached(CommandSource &source, const Anope::string &email) {
        int NSEmailMax = Config->GetModule(this)->Get<int>("maxemails");

        if (NSEmailMax < 1 || email.empty()) {
            return false;
        }

        if (this->CountEmail(email, source.nc) < NSEmailMax) {
            return false;
        }

        if (NSEmailMax == 1) {
            source.Reply(
                _("The email address \002%s\002 has reached its usage limit of 1 user."),
                email.c_str());
        } else {
            source.Reply(
                _("The email address \002%s\002 has reached its usage limit of %d users."),
                email.c_str(), NSEmailMax);
        }

        return true;
    }

    int CountEmail(const Anope::string &email, NickCore *unc) {
        int count = 0;

        if (email.empty()) {
            return 0;
        }

        Anope::string cleanemail = clean ? CleanMail(email) : email;

        for (nickcore_map::const_iterator it = NickCoreList->begin(),
                it_end = NickCoreList->end(); it != it_end; ++it) {
            const NickCore *nc = it->second;

            Anope::string cleannc = clean ? CleanMail(nc->email) : nc->email;

            if (unc != nc && cleanemail.equals_ci(cleannc)) {
                ++count;
            }
        }

        return count;
    }

  public:
    NSMaxEmail(const Anope::string &modname,
               const Anope::string &creator) : Module(modname, creator, VENDOR)
        , clean(false) {
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        clean = conf->GetModule(this)->Get<bool>("remove_aliases", "true");
    }

    EventReturn OnPreCommand(CommandSource &source, Command *command,
                             std::vector<Anope::string> &params) anope_override {
        if (source.IsOper()) {
            return EVENT_CONTINUE;
        }

        if (command->name == "nickserv/register") {
            if (this->CheckLimitReached(source, params.size() > 1 ? params[1] : "")) {
                return EVENT_STOP;
            }
        } else if (command->name == "nickserv/set/email") {
            if (this->CheckLimitReached(source, params.size() > 0 ? params[0] : "")) {
                return EVENT_STOP;
            }
        } else if (command->name == "nickserv/ungroup" && source.GetAccount()) {
            if (this->CheckLimitReached(source, source.GetAccount()->email)) {
                return EVENT_STOP;
            }
        }

        return EVENT_CONTINUE;
    }
};

MODULE_INIT(NSMaxEmail)
