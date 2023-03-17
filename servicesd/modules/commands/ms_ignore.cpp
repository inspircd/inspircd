/* MemoServ core functions
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

class CommandMSIgnore : public Command {
  public:
    CommandMSIgnore(Module *creator) : Command(creator, "memoserv/ignore", 1, 3) {
        this->SetDesc(_("Manage the memo ignore list"));
        this->SetSyntax(_("[\037channel\037] ADD \037entry\037"));
        this->SetSyntax(_("[\037channel\037] DEL \037entry\037"));
        this->SetSyntax(_("[\037channel\037] LIST"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        Anope::string channel = params[0];
        Anope::string command = (params.size() > 1 ? params[1] : "");
        Anope::string param = (params.size() > 2 ? params[2] : "");

        if (channel[0] != '#') {
            param = command;
            command = channel;
            channel = source.GetNick();
        }

        bool ischan;
        MemoInfo *mi = MemoInfo::GetMemoInfo(channel, ischan);
        ChannelInfo *ci = ChannelInfo::Find(channel);
        if (!mi) {
            source.Reply(ischan ? CHAN_X_NOT_REGISTERED : _(NICK_X_NOT_REGISTERED),
                         channel.c_str());
        } else if (ischan && !source.AccessFor(ci).HasPriv("MEMO")) {
            source.Reply(ACCESS_DENIED);
        } else if (command.equals_ci("ADD") && !param.empty()) {
            if (mi->ignores.size() >= Config->GetModule(this->owner)->Get<unsigned>("max",
                    "32")) {
                source.Reply(_("Sorry, the memo ignore list for \002%s\002 is full."),
                             channel.c_str());
                return;
            }

            if (std::find(mi->ignores.begin(), mi->ignores.end(),
                          param.ci_str()) == mi->ignores.end()) {
                mi->ignores.push_back(param.ci_str());
                source.Reply(_("\002%s\002 added to ignore list."), param.c_str());
            } else {
                source.Reply(_("\002%s\002 is already on the ignore list."), param.c_str());
            }
        } else if (command.equals_ci("DEL") && !param.empty()) {
            std::vector<Anope::string>::iterator it = std::find(mi->ignores.begin(),
                    mi->ignores.end(), param.ci_str());

            if (it != mi->ignores.end()) {
                mi->ignores.erase(it);
                source.Reply(_("\002%s\002 removed from the ignore list."), param.c_str());
            } else {
                source.Reply(_("\002%s\002 is not on the ignore list."), param.c_str());
            }
        } else if (command.equals_ci("LIST")) {
            if (mi->ignores.empty()) {
                source.Reply(_("Memo ignore list is empty."));
            } else {
                ListFormatter list(source.GetAccount());
                list.AddColumn(_("Mask"));
                for (unsigned i = 0; i < mi->ignores.size(); ++i) {
                    ListFormatter::ListEntry entry;
                    entry["Mask"] = mi->ignores[i];
                    list.AddEntry(entry);
                }

                source.Reply(_("Ignore list:"));

                std::vector<Anope::string> replies;
                list.Process(replies);

                for (unsigned i = 0; i < replies.size(); ++i) {
                    source.Reply(replies[i]);
                }
            }
        } else {
            this->OnSyntaxError(source, "");
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows you to ignore users by nick or host from memoing\n"
                       "you or a channel. If someone on the memo ignore list tries\n"
                       "to memo you or a channel, they will not be told that you have\n"
                       "them ignored."));
        return true;
    }
};

class MSIgnore : public Module {
    CommandMSIgnore commandmsignore;

  public:
    MSIgnore(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandmsignore(this) {
    }
};

MODULE_INIT(MSIgnore)
