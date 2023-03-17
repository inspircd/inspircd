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
#include "modules/cs_mode.h"

class CommandCSSetKeepTopic : public Command {
  public:
    CommandCSSetKeepTopic(Module *creator,
                          const Anope::string &cname = "chanserv/set/keeptopic") : Command(creator, cname,
                                      2, 2) {
        this->SetDesc(_("Retain topic when channel is not in use"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable keeptopic";
            ci->Extend<bool>("KEEPTOPIC");
            source.Reply(_("Topic retention option for %s is now \002on\002."),
                         ci->name.c_str());
        } else if (params[1].equals_ci("OFF")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable keeptopic";
            ci->Shrink<bool>("KEEPTOPIC");
            source.Reply(_("Topic retention option for %s is now \002off\002."),
                         ci->name.c_str());
        } else {
            this->OnSyntaxError(source, "KEEPTOPIC");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables the \002topic retention\002 option for a\n"
                       "channel. When \002%s\002 is set, the topic for the\n"
                       "channel will be remembered by %s even after the\n"
                       "last user leaves the channel, and will be restored the\n"
                       "next time the channel is created."), source.command.c_str(), source.service->nick.c_str());
        return true;
    }
};

class CommandCSTopic : public Command {
    ExtensibleRef<bool> topiclock;

    void Lock(CommandSource &source, ChannelInfo *ci,
              const std::vector<Anope::string> &params) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci,
                       "topiclock on"));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        topiclock->Set(ci, true);
        source.Reply(_("Topic lock option for %s is now \002on\002."),
                     ci->name.c_str());
    }

    void Unlock(CommandSource &source, ChannelInfo *ci,
                const std::vector<Anope::string> &params) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci,
                       "topiclock off"));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        topiclock->Unset(ci);
        source.Reply(_("Topic lock option for %s is now \002off\002."),
                     ci->name.c_str());
    }

    void Set(CommandSource &source, ChannelInfo *ci, const Anope::string &topic) {
        bool has_topiclock = topiclock->HasExt(ci);
        topiclock->Unset(ci);
        ci->c->ChangeTopic(source.GetNick(), topic, Anope::CurTime);
        if (has_topiclock) {
            topiclock->Set(ci);
        }

        bool override = !source.AccessFor(ci).HasPriv("TOPIC");
        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this,
            ci) << (!topic.empty() ? "to change the topic to: " : "to unset the topic") <<
                (!topic.empty() ? topic : "");
    }

    void Append(CommandSource &source, ChannelInfo *ci,
                const std::vector<Anope::string> &params) {
        const Anope::string &topic = params[2];

        Anope::string new_topic;
        if (!ci->c->topic.empty()) {
            new_topic = ci->c->topic + " " + topic;
            ci->last_topic.clear();
        } else {
            new_topic = topic;
        }

        this->Set(source, ci, new_topic);
    }

  public:
    CommandCSTopic(Module *creator) : Command(creator, "chanserv/topic", 2, 3),
        topiclock("TOPICLOCK") {
        this->SetDesc(_("Manipulate the topic of the specified channel"));
        this->SetSyntax(_("\037channel\037 [SET] [\037topic\037]"));
        this->SetSyntax(_("\037channel\037 APPEND \037topic\037"));
        this->SetSyntax(_("\037channel\037 [UNLOCK|LOCK]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &subcmd = params[1];

        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
        } else if (!source.AccessFor(ci).HasPriv("TOPIC") && !source.HasCommand("chanserv/topic")) {
            source.Reply(ACCESS_DENIED);
        } else if (subcmd.equals_ci("LOCK")) {
            this->Lock(source, ci, params);
        } else if (subcmd.equals_ci("UNLOCK")) {
            this->Unlock(source, ci, params);
        } else if (!ci->c) {
            source.Reply(CHAN_X_NOT_IN_USE, ci->name.c_str());
        } else if (subcmd.equals_ci("APPEND") && params.size() > 2) {
            this->Append(source, ci, params);
        } else {
            Anope::string topic;
            if (subcmd.equals_ci("SET")) {
                topic = params.size() > 2 ? params[2] : "";
            } else {
                topic = subcmd;
                if (params.size() > 2) {
                    topic += " " + params[2];
                }
            }
            this->Set(source, ci, topic);
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows manipulating the topic of the specified channel.\n"
                       "The \002SET\002 command changes the topic of the channel to the given topic\n"
                       "or unsets the topic if no topic is given. The \002APPEND\002 command appends\n"
                       "the given topic to the existing topic.\n"
                       " \n"
                       "\002LOCK\002 and \002UNLOCK\002 may be used to enable and disable topic lock. When\n"
                       "topic lock is set, the channel topic will be unchangeable by users who do not have\n"
                       "the \002TOPIC\002 privilege."));
        return true;
    }
};

class CSTopic : public Module {
    CommandCSTopic commandcstopic;
    CommandCSSetKeepTopic commandcssetkeeptopic;

    SerializableExtensibleItem<bool> topiclock, keeptopic;

  public:
    CSTopic(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcstopic(this), commandcssetkeeptopic(this), topiclock(this, "TOPICLOCK"),
        keeptopic(this, "KEEPTOPIC") {

    }

    void OnChannelSync(Channel *c) anope_override {
        if (c->ci) {
            /* Update channel topic */
            if ((topiclock.HasExt(c->ci) || keeptopic.HasExt(c->ci))
                    && c->ci->last_topic != c->topic) {
                c->ChangeTopic(!c->ci->last_topic_setter.empty() ? c->ci->last_topic_setter :
                               c->ci->WhoSends()->nick, c->ci->last_topic,
                               c->ci->last_topic_time ? c->ci->last_topic_time : Anope::CurTime);
            }
        }
    }

    void OnTopicUpdated(User *source, Channel *c, const Anope::string &user,
                        const Anope::string &topic) anope_override {
        if (!c->ci) {
            return;
        }

        /* We only compare the topics here, not the time or setter. This is because some (old) IRCds do not
         * allow us to set the topic as someone else, meaning we have to bump the TS and change the setter to us.
         * This desyncs what is really set with what we have stored, and we end up resetting the topic often when
         * it is not required
         */
        if (topiclock.HasExt(c->ci) && c->ci->last_topic != c->topic && (!source || !c->ci->AccessFor(source).HasPriv("TOPIC"))) {
            c->ChangeTopic(c->ci->last_topic_setter, c->ci->last_topic,
                           c->ci->last_topic_time);
        } else {
            c->ci->last_topic = c->topic;
            c->ci->last_topic_setter = c->topic_setter;
            c->ci->last_topic_time = c->topic_ts;
        }
    }

    void OnChanInfo(CommandSource &source, ChannelInfo *ci, InfoFormatter &info,
                    bool show_all) anope_override {
        if (keeptopic.HasExt(ci)) {
            info.AddOption(_("Topic retention"));
        }
        if (topiclock.HasExt(ci)) {
            info.AddOption(_("Topic lock"));
        }

        ModeLocks *ml = ci->GetExt<ModeLocks>("modelocks");
        const ModeLock *secret = ml ? ml->GetMLock("SECRET") : NULL;
        if (!ci->last_topic.empty() && (show_all || ((!secret || secret->set == false) && (!ci->c || !ci->c->HasMode("SECRET"))))) {
            info[_("Last topic")] = ci->last_topic;
            info[_("Topic set by")] = ci->last_topic_setter;
        }
    }
};

MODULE_INIT(CSTopic)
