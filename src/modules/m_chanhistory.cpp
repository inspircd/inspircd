/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2015, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "modules/ircv3_servertime.h"
#include "modules/ircv3_batch.h"
#include "modules/server.h"

typedef insp::flat_map<std::string, std::string> HistoryTagMap;

struct HistoryItem {
    time_t ts;
    std::string text;
    MessageType type;
    HistoryTagMap tags;
    std::string sourcemask;

    HistoryItem(User* source, const MessageDetails& details)
        : ts(ServerInstance->Time())
        , text(details.text)
        , type(details.type)
        , sourcemask(source->GetFullHost()) {
        tags.reserve(details.tags_out.size());
        for (ClientProtocol::TagMap::const_iterator iter = details.tags_out.begin();
                iter != details.tags_out.end(); ++iter) {
            tags[iter->first] = iter->second.value;
        }
    }
};

struct HistoryList {
    std::deque<HistoryItem> lines;
    unsigned int maxlen;
    unsigned int maxtime;

    HistoryList(unsigned int len, unsigned int time)
        : maxlen(len)
        , maxtime(time) {
    }

    size_t Prune() {
        // Prune expired entries from the list.
        if (maxtime) {
            time_t mintime = ServerInstance->Time() - maxtime;
            while (!lines.empty() && lines.front().ts < mintime) {
                lines.pop_front();
            }
        }
        return lines.size();
    }
};

class HistoryMode : public ParamMode<HistoryMode, SimpleExtItem<HistoryList> > {
  public:
    unsigned int maxlines;
    HistoryMode(Module* Creator)
        : ParamMode<HistoryMode, SimpleExtItem<HistoryList> >(Creator, "history", 'H') {
        syntax = "<max-messages>:<max-duration>";
    }

    ModeAction OnSet(User* source, Channel* channel,
                     std::string& parameter) CXX11_OVERRIDE {
        std::string::size_type colon = parameter.find(':');
        if (colon == std::string::npos) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }

        std::string duration(parameter, colon+1);
        if ((IS_LOCAL(source)) && ((duration.length() > 10) || (!InspIRCd::IsValidDuration(duration)))) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }

        unsigned int len = ConvToNum<unsigned int>(parameter.substr(0, colon));
        unsigned long time;
        if (!InspIRCd::Duration(duration, time) || len == 0 || (len > maxlines && IS_LOCAL(source))) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }
        if (len > maxlines) {
            len = maxlines;
        }

        HistoryList* history = ext.get(channel);
        if (history) {
            // Shrink the list if the new line number limit is lower than the old one
            if (len < history->lines.size()) {
                history->lines.erase(history->lines.begin(),
                                     history->lines.begin() + (history->lines.size() - len));
            }

            history->maxlen = len;
            history->maxtime = time;
            history->Prune();
        } else {
            ext.set(channel, new HistoryList(len, time));
        }
        return MODEACTION_ALLOW;
    }

    void SerializeParam(Channel* chan, const HistoryList* history,
                        std::string& out) {
        out.append(ConvToStr(history->maxlen));
        out.append(":");
        out.append(InspIRCd::DurationString(history->maxtime));
    }
};

class NoHistoryMode : public SimpleUserModeHandler {
  public:
    NoHistoryMode(Module* Creator)
        : SimpleUserModeHandler(Creator, "nohistory", 'N') {
        if (!ServerInstance->Config->ConfValue("chanhistory")->getBool("enableumode")) {
            DisableAutoRegister();
        }
    }
};

class ModuleChanHistory
    : public Module
    , public ServerProtocol::BroadcastEventListener {
  private:
    HistoryMode historymode;
    NoHistoryMode nohistorymode;
    bool prefixmsg;
    UserModeReference botmode;
    bool dobots;
    IRCv3::Batch::CapReference batchcap;
    IRCv3::Batch::API batchmanager;
    IRCv3::Batch::Batch batch;
    IRCv3::ServerTime::API servertimemanager;
    ClientProtocol::MessageTagEvent tagevent;

    void AddTag(ClientProtocol::Message& msg, const std::string& tagkey,
                std::string& tagval) {
        const Events::ModuleEventProvider::SubscriberList& list =
            tagevent.GetSubscribers();
        for (Events::ModuleEventProvider::SubscriberList::const_iterator i =
                    list.begin(); i != list.end(); ++i) {
            ClientProtocol::MessageTagProvider* const tagprov =
                static_cast<ClientProtocol::MessageTagProvider*>(*i);
            const ModResult res = tagprov->OnProcessTag(ServerInstance->FakeClient, tagkey,
                                  tagval);
            if (res == MOD_RES_ALLOW) {
                msg.AddTag(tagkey, tagprov, tagval);
            } else if (res == MOD_RES_DENY) {
                break;
            }
        }
    }

    void SendHistory(LocalUser* user, Channel* channel, HistoryList* list) {
        if (batchmanager) {
            batchmanager->Start(batch);
            batch.GetBatchStartMessage().PushParamRef(channel->name);
        }

        for (std::deque<HistoryItem>::iterator i = list->lines.begin();
                i != list->lines.end(); ++i) {
            HistoryItem& item = *i;
            ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy,
                                                  item.sourcemask, channel, item.text, item.type);
            for (HistoryTagMap::iterator iter = item.tags.begin(); iter != item.tags.end();
                    ++iter) {
                AddTag(msg, iter->first, iter->second);
            }
            if (servertimemanager) {
                servertimemanager->Set(msg, item.ts);
            }
            batch.AddToBatch(msg);
            user->Send(ServerInstance->GetRFCEvents().privmsg, msg);
        }

        if (batchmanager) {
            batchmanager->End(batch);
        }
    }

  public:
    ModuleChanHistory()
        : ServerProtocol::BroadcastEventListener(this)
        , historymode(this)
        , nohistorymode(this)
        , botmode(this, "bot")
        , batchcap(this)
        , batchmanager(this)
        , batch("chathistory")
        , servertimemanager(this)
        , tagevent(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("chanhistory");
        historymode.maxlines = tag->getUInt("maxlines", 50, 1);
        prefixmsg = tag->getBool("prefixmsg", tag->getBool("notice", true));
        dobots = tag->getBool("bots", true);
    }

    ModResult OnBroadcastMessage(Channel* channel,
                                 const Server* server) CXX11_OVERRIDE {
        return channel->IsModeSet(historymode) ? MOD_RES_ALLOW : MOD_RES_PASSTHRU;
    }

    void OnUserPostMessage(User* user, const MessageTarget& target,
                           const MessageDetails& details) CXX11_OVERRIDE {
        if (target.type != MessageTarget::TYPE_CHANNEL || target.status) {
            return;
        }

        std::string ctcpname;
        if (details.IsCTCP(ctcpname) && !irc::equals(ctcpname, "ACTION")) {
            return;
        }

        HistoryList* list = historymode.ext.get(target.Get<Channel>());
        if (!list) {
            return;
        }

        list->lines.push_back(HistoryItem(user, details));
        if (list->lines.size() > list->maxlen) {
            list->lines.pop_front();
        }
    }

    void OnPostJoin(Membership* memb) CXX11_OVERRIDE {
        LocalUser* localuser = IS_LOCAL(memb->user);
        if (!localuser) {
            return;
        }

        if (memb->user->IsModeSet(botmode) && !dobots) {
            return;
        }

        if (memb->user->IsModeSet(nohistorymode)) {
            return;
        }

        HistoryList* list = historymode.ext.get(memb->chan);
        if (!list || !list->Prune()) {
            return;
        }

        if ((prefixmsg) && (!batchcap.get(localuser))) {
            std::string message("Replaying up to " + ConvToStr(list->maxlen) +
                                " lines of pre-join history");
            if (list->maxtime > 0) {
                message.append(" from the last " + InspIRCd::DurationString(list->maxtime));
            }
            memb->WriteNotice(message);
        }

        SendHistory(localuser, memb->chan, list);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode H (history) which allows message history to be viewed on joining the channel.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleChanHistory)
