/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2021 David Schultz <me@zpld.me>
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2018-2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017-2019, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 James Lu <GLolol@overdrivenetworks.com>
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "modules/exemption.h"

class ChannelSettings {
  public:
    enum RepeatAction {
        ACT_KICK,
        ACT_BLOCK,
        ACT_BAN
    };

    RepeatAction Action;
    unsigned int Backlog;
    unsigned int Lines;
    unsigned int Diff;
    unsigned long Seconds;

    void serialize(std::string& out) const {
        if (Action == ACT_BAN) {
            out.push_back('*');
        } else if (Action == ACT_BLOCK) {
            out.push_back('~');
        }

        out.append(ConvToStr(Lines)).push_back(':');
        out.append(ConvToStr(Seconds));
        if (Diff) {
            out.push_back(':');
            out.append(ConvToStr(Diff));
            if (Backlog) {
                out.push_back(':');
                out.append(ConvToStr(Backlog));
            }
        }
    }
};

class RepeatMode : public
    ParamMode<RepeatMode, SimpleExtItem<ChannelSettings> > {
  private:
    struct RepeatItem {
        time_t ts;
        std::string line;
        RepeatItem(time_t TS, const std::string& Line) : ts(TS), line(Line) { }
    };

    typedef std::deque<RepeatItem> RepeatItemList;

    struct MemberInfo {
        RepeatItemList ItemList;
        unsigned int Counter;
        MemberInfo() : Counter(0) {}
    };

    struct ModuleSettings {
        unsigned int MaxLines;
        unsigned int MaxSecs;
        unsigned int MaxBacklog;
        unsigned int MaxDiff;
        unsigned int MaxMessageSize;
        std::string KickMessage;
        ModuleSettings() : MaxLines(0), MaxSecs(0), MaxBacklog(0), MaxDiff() { }
    };

    std::vector<unsigned int> mx[2];
    ModuleSettings ms;

    bool CompareLines(const std::string& message, const std::string& historyline,
                      unsigned int trigger) {
        if (message == historyline) {
            return true;
        } else if (trigger) {
            return (Levenshtein(message, historyline) <= trigger);
        }

        return false;
    }

    unsigned int Levenshtein(const std::string& s1, const std::string& s2) {
        unsigned int l1 = s1.size();
        unsigned int l2 = s2.size();

        for (unsigned int i = 0; i < l2; i++) {
            mx[0][i] = i;
        }
        for (unsigned int i = 0; i < l1; i++) {
            mx[1][0] = i + 1;
            for (unsigned int j = 0; j < l2; j++) {
                mx[1][j + 1] = std::min(std::min(mx[1][j] + 1, mx[0][j + 1] + 1),
                                        mx[0][j] + ((s1[i] == s2[j]) ? 0 : 1));
            }

            mx[0].swap(mx[1]);
        }
        return mx[0][l2];
    }

  public:
    SimpleExtItem<MemberInfo> MemberInfoExt;

    RepeatMode(Module* Creator)
        : ParamMode<RepeatMode, SimpleExtItem<ChannelSettings> >(Creator, "repeat", 'E')
        , MemberInfoExt("repeat_memb", ExtensionItem::EXT_MEMBERSHIP, Creator) {
        syntax = "[~|*]<lines>:<duration>[:<difference>][:<backlog>]";
    }

    void OnUnset(User* source, Channel* chan) CXX11_OVERRIDE {
        // Unset the per-membership extension when the mode is removed
        const Channel::MemberMap& users = chan->GetUsers();
        for (Channel::MemberMap::const_iterator i = users.begin(); i != users.end(); ++i) {
            MemberInfoExt.unset(i->second);
        }
    }

    ModeAction OnSet(User* source, Channel* channel,
                     std::string& parameter) CXX11_OVERRIDE {
        ChannelSettings settings;
        if (!ParseSettings(source, parameter, settings)) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }

        if ((settings.Backlog > 0) && (settings.Lines > settings.Backlog)) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter,
                                 "You can't set lines higher than backlog."));
            return MODEACTION_DENY;
        }

        LocalUser* localsource = IS_LOCAL(source);
        if ((localsource) && (!ValidateSettings(localsource, channel, parameter, settings))) {
            return MODEACTION_DENY;
        }

        ext.set(channel, settings);

        return MODEACTION_ALLOW;
    }

    bool MatchLine(Membership* memb, ChannelSettings* rs, std::string message) {
        // If the message is larger than whatever size it's set to,
        // let's pretend it isn't. If the first 512 (def. setting) match, it's probably spam.
        if (message.size() > ms.MaxMessageSize) {
            message.erase(ms.MaxMessageSize);
        }

        MemberInfo* rp = MemberInfoExt.get(memb);
        if (!rp) {
            rp = new MemberInfo;
            MemberInfoExt.set(memb, rp);
        }

        unsigned int matches = 0;
        if (!rs->Backlog) {
            matches = rp->Counter;
        }

        RepeatItemList& items = rp->ItemList;
        const unsigned int trigger = (message.size() * rs->Diff / 100);
        const time_t now = ServerInstance->Time();

        std::transform(message.begin(), message.end(), message.begin(), ::tolower);

        for (std::deque<RepeatItem>::iterator it = items.begin(); it != items.end();
                ++it) {
            if (it->ts < now) {
                items.erase(it, items.end());
                matches = 0;
                break;
            }

            if (CompareLines(message, it->line, trigger)) {
                if (++matches >= rs->Lines) {
                    if (rs->Action != ChannelSettings::ACT_BLOCK) {
                        rp->Counter = 0;
                    }
                    return true;
                }
            } else if ((ms.MaxBacklog == 0) || (rs->Backlog == 0)) {
                matches = 0;
                items.clear();
                break;
            }
        }

        unsigned int max_items = (rs->Backlog ? rs->Backlog : 1);
        if (items.size() >= max_items) {
            items.pop_back();
        }

        items.push_front(RepeatItem(now + rs->Seconds, message));
        rp->Counter = matches;
        return false;
    }

    void Resize(size_t size) {
        size_t newsize = size+1;
        if (newsize <= mx[0].size()) {
            return;
        }
        ms.MaxMessageSize = size;
        mx[0].resize(newsize);
        mx[1].resize(newsize);
    }

    void ReadConfig() {
        ConfigTag* conf = ServerInstance->Config->ConfValue("repeat");
        ms.MaxLines = conf->getUInt("maxlines", 20);
        ms.MaxBacklog = conf->getUInt("maxbacklog", 20);
        ms.MaxSecs = conf->getDuration("maxtime", conf->getDuration("maxsecs", 0));

        ms.MaxDiff = conf->getUInt("maxdistance", 50);
        if (ms.MaxDiff > 100) {
            ms.MaxDiff = 100;
        }

        unsigned int newsize = conf->getUInt("size", 512);
        if (newsize > ServerInstance->Config->Limits.MaxLine) {
            newsize = ServerInstance->Config->Limits.MaxLine;
        }
        Resize(newsize);

        ms.KickMessage = conf->getString("kickmessage", "Repeat flood");
    }

    std::string GetModuleSettings() const {
        return ConvToStr(ms.MaxLines) + ":" + ConvToStr(ms.MaxSecs) + ":" + ConvToStr(
                   ms.MaxDiff) + ":" + ConvToStr(ms.MaxBacklog);
    }

    std::string GetKickMessage() const {
        return ms.KickMessage;
    }

    void SerializeParam(Channel* chan, const ChannelSettings* chset,
                        std::string& out) {
        chset->serialize(out);
    }

  private:
    bool ParseSettings(User* source, std::string& parameter,
                       ChannelSettings& settings) {
        irc::sepstream stream(parameter, ':');
        std::string item;
        if (!stream.GetToken(item))
            // Required parameter missing
        {
            return false;
        }

        if ((item[0] == '*') || (item[0] == '~')) {
            settings.Action = ((item[0] == '*') ? ChannelSettings::ACT_BAN :
                               ChannelSettings::ACT_BLOCK);
            item.erase(item.begin());
        } else {
            settings.Action = ChannelSettings::ACT_KICK;
        }

        if ((settings.Lines = ConvToNum<unsigned int>(item)) == 0) {
            return false;
        }

        if ((!stream.GetToken(item)) || !InspIRCd::Duration(item, settings.Seconds)
                || (settings.Seconds == 0))
            // Required parameter missing
        {
            return false;
        }

        // The diff and backlog parameters are optional
        settings.Diff = settings.Backlog = 0;
        if (stream.GetToken(item)) {
            // There is a diff parameter, see if it's valid (> 0)
            if ((settings.Diff = ConvToNum<unsigned int>(item)) == 0) {
                return false;
            }

            if (stream.GetToken(item)) {
                // There is a backlog parameter, see if it's valid
                if ((settings.Backlog = ConvToNum<unsigned int>(item)) == 0) {
                    return false;
                }

                // If there are still tokens, then it's invalid because we allow only 4
                if (stream.GetToken(item)) {
                    return false;
                }
            }
        }

        return true;
    }

    bool ValidateSettings(LocalUser* source, Channel* channel,
                          const std::string& parameter, const ChannelSettings& settings) {
        if (ms.MaxLines && settings.Lines > ms.MaxLines) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter,
                                 InspIRCd::Format(
                                     "The line number you specified is too big. Maximum allowed is %u.",
                                     ms.MaxLines)));
            return false;
        }

        if (ms.MaxSecs && settings.Seconds > ms.MaxSecs) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter,
                                 InspIRCd::Format(
                                     "The seconds you specified are too big. Maximum allowed is %u.", ms.MaxSecs)));
            return false;
        }

        if (settings.Diff && settings.Diff > ms.MaxDiff) {
            if (ms.MaxDiff == 0)
                source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter,
                                     "The server administrator has disabled matching on edit distance."));
            else
                source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter,
                                     InspIRCd::Format(
                                         "The distance you specified is too big. Maximum allowed is %u.", ms.MaxDiff)));
            return false;
        }

        if (settings.Backlog && settings.Backlog > ms.MaxBacklog) {
            if (ms.MaxBacklog == 0)
                source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter,
                                     "The server administrator has disabled backlog matching."));
            else
                source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter,
                                     InspIRCd::Format(
                                         "The backlog you specified is too big. Maximum allowed is %u.",
                                         ms.MaxBacklog)));
            return false;
        }

        return true;
    }
};

class RepeatModule : public Module {
  private:
    ChanModeReference banmode;
    CheckExemption::EventProvider exemptionprov;
    RepeatMode rm;

  public:
    RepeatModule()
        : banmode(this, "ban")
        , exemptionprov(this)
        , rm(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        rm.ReadConfig();
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        if (target.type != MessageTarget::TYPE_CHANNEL || !IS_LOCAL(user)) {
            return MOD_RES_PASSTHRU;
        }

        Channel* chan = target.Get<Channel>();
        ChannelSettings* settings = rm.ext.get(chan);
        if (!settings) {
            return MOD_RES_PASSTHRU;
        }

        Membership* memb = chan->GetUser(user);
        if (!memb) {
            return MOD_RES_PASSTHRU;
        }

        ModResult res = CheckExemption::Call(exemptionprov, user, chan, "repeat");
        if (res == MOD_RES_ALLOW) {
            return MOD_RES_PASSTHRU;
        }

        if (user->HasPrivPermission("channels/ignore-repeat")) {
            return MOD_RES_PASSTHRU;
        }

        if (rm.MatchLine(memb, settings, details.text)) {
            if (settings->Action == ChannelSettings::ACT_BLOCK) {
                user->WriteNotice("*** This line is too similar to one of your last lines.");
                return MOD_RES_DENY;
            }

            if (settings->Action == ChannelSettings::ACT_BAN) {
                Modes::ChangeList changelist;
                changelist.push_add(*banmode,
                                    "*!" + user->GetBanIdent() + "@" + user->GetDisplayedHost());
                ServerInstance->Modes->Process(ServerInstance->FakeClient, chan, NULL,
                                               changelist);
            }

            memb->chan->KickUser(ServerInstance->FakeClient, user, rm.GetKickMessage());
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }

    void Prioritize() CXX11_OVERRIDE {
        ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode E (repeat) which helps protect against spammers which spam the same message repeatedly.", VF_COMMON|VF_VENDOR, rm.GetModuleSettings());
    }
};

MODULE_INIT(RepeatModule)
