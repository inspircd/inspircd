/*
 *
 * (C) 2011-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#ifndef MEMOSERV_H
#define MEMOSERV_H

class MemoServService : public Service {
  public:
    enum MemoResult {
        MEMO_SUCCESS,
        MEMO_INVALID_TARGET,
        MEMO_TOO_FAST,
        MEMO_TARGET_FULL
    };

    MemoServService(Module *m) : Service(m, "MemoServService", "MemoServ") {
    }

    /** Sends a memo.
     * @param source The source of the memo, can be anything.
     * @param target The target of the memo, nick or channel.
     * @param message Memo text
     * @param force true to force the memo, restrictions/delays etc are not checked
     */
    virtual MemoResult Send(const Anope::string &source,
                            const Anope::string &target, const Anope::string &message,
                            bool force = false) = 0;

    /** Check for new memos and notify the user if there are any
     * @param u The user
     */
    virtual void Check(User *u) = 0;
};

#endif // MEMOSERV_H
