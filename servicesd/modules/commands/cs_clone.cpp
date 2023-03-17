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
#include "modules/bs_badwords.h"

class CommandCSClone : public Command {
    void CopySetting(ChannelInfo *ci, ChannelInfo *target_ci,
                     const Anope::string &setting) {
        if (ci->HasExt(setting)) {
            target_ci->Extend<bool>(setting);
        }
    }

    void CopyAccess(CommandSource &source, ChannelInfo *ci,
                    ChannelInfo *target_ci) {
        std::set<Anope::string> masks;
        unsigned access_max = Config->GetModule("chanserv")->Get<unsigned>("accessmax",
                              "1024");
        unsigned count = 0;

        for (unsigned i = 0; i < target_ci->GetAccessCount(); ++i) {
            masks.insert(target_ci->GetAccess(i)->Mask());
        }

        for (unsigned i = 0; i < ci->GetAccessCount(); ++i) {
            const ChanAccess *taccess = ci->GetAccess(i);
            AccessProvider *provider = taccess->provider;

            if (access_max && target_ci->GetDeepAccessCount() >= access_max) {
                break;
            }

            if (masks.count(taccess->Mask())) {
                continue;
            }
            masks.insert(taccess->Mask());

            ChanAccess *newaccess = provider->Create();
            newaccess->SetMask(taccess->Mask(), target_ci);
            newaccess->creator = taccess->creator;
            newaccess->last_seen = taccess->last_seen;
            newaccess->created = taccess->created;
            newaccess->AccessUnserialize(taccess->AccessSerialize());

            target_ci->AddAccess(newaccess);

            ++count;
        }

        source.Reply(
            _("%d access entries from \002%s\002 have been cloned to \002%s\002."), count,
            ci->name.c_str(), target_ci->name.c_str());
    }

    void CopyAkick(CommandSource &source, ChannelInfo *ci, ChannelInfo *target_ci) {
        target_ci->ClearAkick();
        for (unsigned i = 0; i < ci->GetAkickCount(); ++i) {
            const AutoKick *akick = ci->GetAkick(i);
            if (akick->nc) {
                target_ci->AddAkick(akick->creator, akick->nc, akick->reason, akick->addtime,
                                    akick->last_used);
            } else {
                target_ci->AddAkick(akick->creator, akick->mask, akick->reason, akick->addtime,
                                    akick->last_used);
            }
        }

        source.Reply(
            _("All akick entries from \002%s\002 have been cloned to \002%s\002."),
            ci->name.c_str(), target_ci->name.c_str());
    }

    void CopyBadwords(CommandSource &source, ChannelInfo *ci,
                      ChannelInfo *target_ci) {
        BadWords *target_badwords = target_ci->Require<BadWords>("badwords"),
                  *badwords = ci->Require<BadWords>("badwords");

        if (!target_badwords || !badwords) {
            source.Reply(ACCESS_DENIED); // BotServ doesn't exist/badwords isn't loaded
            return;
        }

        target_badwords->ClearBadWords();

        for (unsigned i = 0; i < badwords->GetBadWordCount(); ++i) {
            const BadWord *bw = badwords->GetBadWord(i);
            target_badwords->AddBadWord(bw->word, bw->type);
        }

        badwords->Check();
        target_badwords->Check();

        source.Reply(
            _("All badword entries from \002%s\002 have been cloned to \002%s\002."),
            ci->name.c_str(), target_ci->name.c_str());
    }

    void CopyLevels(CommandSource &source, ChannelInfo *ci,
                    ChannelInfo *target_ci) {
        const Anope::map<int16_t> &cilevels = ci->GetLevelEntries();

        for (Anope::map<int16_t>::const_iterator it = cilevels.begin();
                it != cilevels.end(); ++it) {
            target_ci->SetLevel(it->first, it->second);
        }

        source.Reply(
            _("All level entries from \002%s\002 have been cloned into \002%s\002."),
            ci->name.c_str(), target_ci->name.c_str());
    }

  public:
    CommandCSClone(Module *creator) : Command(creator, "chanserv/clone", 2, 3) {
        this->SetDesc(_("Copy all settings from one channel to another"));
        this->SetSyntax(_("\037channel\037 \037target\037 [\037what\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &channel = params[0];
        const Anope::string &target = params[1];
        Anope::string what = params.size() > 2 ? params[2] : "";

        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        User *u = source.GetUser();
        ChannelInfo *ci = ChannelInfo::Find(channel);
        bool override = false;

        if (ci == NULL) {
            source.Reply(CHAN_X_NOT_REGISTERED, channel.c_str());
            return;
        }

        ChannelInfo *target_ci = ChannelInfo::Find(target);
        if (!target_ci) {
            source.Reply(CHAN_X_NOT_REGISTERED, target.c_str());
            return;
        }

        if (ci == target_ci) {
            source.Reply(_("Cannot clone channel \002%s\002 to itself!"), target.c_str());
            return;
        }

        if (!source.IsFounder(ci) || !source.IsFounder(target_ci)) {
            if (!source.HasPriv("chanserv/administration")) {
                source.Reply(ACCESS_DENIED);
                return;
            } else {
                override = true;
            }
        }

        if (what.equals_ci("ALL")) {
            what.clear();
        }

        if (what.empty()) {
            delete target_ci;
            target_ci = new ChannelInfo(*ci);
            target_ci->name = target;
            target_ci->time_registered = Anope::CurTime;
            (*RegisteredChannelList)[target_ci->name] = target_ci;
            target_ci->c = Channel::Find(target_ci->name);

            target_ci->bi = NULL;
            if (ci->bi) {
                ci->bi->Assign(u, target_ci);
            }

            if (target_ci->c) {
                target_ci->c->ci = target_ci;

                target_ci->c->CheckModes();

                target_ci->c->SetCorrectModes(u, true);
            }

            if (target_ci->c && !target_ci->c->topic.empty()) {
                target_ci->last_topic = target_ci->c->topic;
                target_ci->last_topic_setter = target_ci->c->topic_setter;
                target_ci->last_topic_time = target_ci->c->topic_time;
            } else {
                target_ci->last_topic_setter = source.service->nick;
            }

            const Anope::string settings[] = { "NOAUTOOP", "CS_KEEP_MODES", "PEACE", "PERSIST", "RESTRICTED",
                                               "CS_SECURE", "SECUREFOUNDER", "SECUREOPS", "SIGNKICK", "SIGNKICK_LEVEL", "CS_NO_EXPIRE"
                                             };

            for (unsigned int i = 0; i < sizeof(settings) / sizeof(Anope::string); ++i) {
                CopySetting(ci, target_ci, settings[i]);
            }

            CopyAccess(source, ci, target_ci);
            CopyAkick(source, ci, target_ci);
            CopyBadwords(source, ci, target_ci);
            CopyLevels(source, ci, target_ci);

            FOREACH_MOD(OnChanRegistered, (target_ci));

            source.Reply(_("All settings from \002%s\002 have been cloned to \002%s\002."),
                         ci->name.c_str(), target_ci->name.c_str());
        } else if (what.equals_ci("ACCESS")) {
            CopyAccess(source, ci, target_ci);
        } else if (what.equals_ci("AKICK")) {
            CopyAkick(source, ci, target_ci);
        } else if (what.equals_ci("BADWORDS")) {
            CopyBadwords(source, ci, target_ci);
        } else if (what.equals_ci("LEVELS")) {
            CopyLevels(source, ci, target_ci);
        } else {
            this->OnSyntaxError(source, "");
            return;
        }

        Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, this, ci) << "to clone " << (what.empty() ? "everything from it" : what) << " to " << target_ci->name;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Copies all settings, access, akicks, etc from \002channel\002 to the\n"
                       "\002target\002 channel. If \037what\037 is \002ACCESS\002, \002AKICK\002, \002BADWORDS\002,\n"
                       "or \002LEVELS\002 then only the respective settings are cloned.\n"
                       "You must be the founder of \037channel\037 and \037target\037."));
        return true;
    }
};

class CSClone : public Module {
    CommandCSClone commandcsclone;

  public:
    CSClone(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcsclone(this) {

    }
};

MODULE_INIT(CSClone)
