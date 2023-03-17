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

#ifndef MAIL_H
#define MAIL_H

#include "anope.h"
#include "threadengine.h"
#include "serialize.h"

namespace Mail {
extern CoreExport bool Send(User *from, NickCore *to, BotInfo *service,
                            const Anope::string &subject, const Anope::string &message);
extern CoreExport bool Send(NickCore *to, const Anope::string &subject,
                            const Anope::string &message);
extern CoreExport bool Validate(const Anope::string &email);

/* A email message being sent */
class Message : public Thread {
  private:
    Anope::string sendmail_path;
    Anope::string send_from;
    Anope::string mail_to;
    Anope::string addr;
    Anope::string subject;
    Anope::string message;
    bool dont_quote_addresses;

    bool success;
  public:
    /** Construct this message. Once constructed call Thread::Start to launch the mail sending.
     * @param sf Config->SendFrom
     * @param mailto Name of person being mailed (u->nick, nc->display, etc)
     * @param addr Destination address to mail
     * @param subject Message subject
     * @param message The actual message
     */
    Message(const Anope::string &sf, const Anope::string &mailto,
            const Anope::string &addr, const Anope::string &subject,
            const Anope::string &message);

    ~Message();

    /* Called from within the thread to actually send the mail */
    void Run() anope_override;
};

} // namespace Mail

#endif // MAIL_H
