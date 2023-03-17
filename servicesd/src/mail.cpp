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

#include "services.h"
#include "mail.h"
#include "config.h"

Mail::Message::Message(const Anope::string &sf, const Anope::string &mailto,
                       const Anope::string &a, const Anope::string &s,
                       const Anope::string &m) : Thread(),
    sendmail_path(
        Config->GetBlock("mail")->Get<const Anope::string>("sendmailpath")),
    send_from(sf), mail_to(mailto), addr(a), subject(s), message(m),
    dont_quote_addresses(Config->GetBlock("mail")->Get<bool>("dontquoteaddresses")),
    success(false) {
}

Mail::Message::~Message() {
    if (success) {
        Log(LOG_NORMAL, "mail") << "Successfully delivered mail for " << mail_to << " ("
                                << addr << ")";
    } else {
        Log(LOG_NORMAL, "mail") << "Error delivering mail for " << mail_to << " (" <<
                                addr << ")";
    }
}

void Mail::Message::Run() {
    FILE *pipe = popen(sendmail_path.c_str(), "w");

    if (!pipe) {
        SetExitState();
        return;
    }

    fprintf(pipe, "From: %s\n", send_from.c_str());
    if (this->dont_quote_addresses) {
        fprintf(pipe, "To: %s <%s>\n", mail_to.c_str(), addr.c_str());
    } else {
        fprintf(pipe, "To: \"%s\" <%s>\n", mail_to.c_str(), addr.c_str());
    }
    fprintf(pipe, "Subject: %s\n", subject.c_str());
    fprintf(pipe, "Content-Type: text/plain; charset=UTF-8;\n");
    fprintf(pipe, "Content-Transfer-Encoding: 8bit\n");
    fprintf(pipe, "\n");
    fprintf(pipe, "%s", message.c_str());
    fprintf(pipe, "\n.\n");

    pclose(pipe);

    success = true;
    SetExitState();
}

bool Mail::Send(User *u, NickCore *nc, BotInfo *service,
                const Anope::string &subject, const Anope::string &message) {
    if (!nc || !service || subject.empty() || message.empty()) {
        return false;
    }

    Configuration::Block *b = Config->GetBlock("mail");

    if (!u) {
        if (!b->Get<bool>("usemail")
                || b->Get<const Anope::string>("sendfrom").empty()) {
            return false;
        } else if (nc->email.empty()) {
            return false;
        }

        nc->lastmail = Anope::CurTime;
        Thread *t = new Mail::Message(b->Get<const Anope::string>("sendfrom"),
                                      nc->display, nc->email, subject, message);
        t->Start();
        return true;
    } else {
        if (!b->Get<bool>("usemail")
                || b->Get<const Anope::string>("sendfrom").empty()) {
            u->SendMessage(service, _("Services have been configured to not send mail."));
        } else if (Anope::CurTime - u->lastmail < b->Get<time_t>("delay")) {
            u->SendMessage(service, _("Please wait \002%d\002 seconds and retry."),
                           b->Get<time_t>("delay") - (Anope::CurTime - u->lastmail));
        } else if (nc->email.empty()) {
            u->SendMessage(service, _("E-mail for \002%s\002 is invalid."),
                           nc->display.c_str());
        } else {
            u->lastmail = nc->lastmail = Anope::CurTime;
            Thread *t = new Mail::Message(b->Get<const Anope::string>("sendfrom"),
                                          nc->display, nc->email, subject, message);
            t->Start();
            return true;
        }

        return false;
    }
}

bool Mail::Send(NickCore *nc, const Anope::string &subject,
                const Anope::string &message) {
    Configuration::Block *b = Config->GetBlock("mail");
    if (!b->Get<bool>("usemail") || b->Get<const Anope::string>("sendfrom").empty()
            || !nc || nc->email.empty() || subject.empty() || message.empty()) {
        return false;
    }

    nc->lastmail = Anope::CurTime;
    Thread *t = new Mail::Message(b->Get<const Anope::string>("sendfrom"),
                                  nc->display, nc->email, subject, message);
    t->Start();

    return true;
}

/**
 * Checks whether we have a valid, common e-mail address.
 * This is NOT entirely RFC compliant, and won't be so, because I said
 * *common* cases. ;) It is very unlikely that e-mail addresses that
 * are really being used will fail the check.
 *
 * @param email Email to Validate
 * @return bool
 */
bool Mail::Validate(const Anope::string &email) {
    bool has_period = false;

    static char specials[] = {'(', ')', '<', '>', '@', ',', ';', ':', '\\', '\"', '[', ']', ' '};

    if (email.empty()) {
        return false;
    }
    Anope::string copy = email;

    size_t at = copy.find('@');
    if (at == Anope::string::npos) {
        return false;
    }
    Anope::string domain = copy.substr(at + 1);
    copy = copy.substr(0, at);

    /* Don't accept empty copy or domain. */
    if (copy.empty() || domain.empty()) {
        return false;
    }

    /* Check for forbidden characters in the name */
    for (unsigned i = 0, end = copy.length(); i < end; ++i) {
        if (copy[i] <= 31 || copy[i] >= 127) {
            return false;
        }
        for (unsigned int j = 0; j < 13; ++j)
            if (copy[i] == specials[j]) {
                return false;
            }
    }

    /* Check for forbidden characters in the domain */
    for (unsigned i = 0, end = domain.length(); i < end; ++i) {
        if (domain[i] <= 31 || domain[i] >= 127) {
            return false;
        }
        for (unsigned int j = 0; j < 13; ++j)
            if (domain[i] == specials[j]) {
                return false;
            }
        if (domain[i] == '.') {
            if (!i || i == end - 1) {
                return false;
            }
            has_period = true;
        }
    }

    return has_period;
}
