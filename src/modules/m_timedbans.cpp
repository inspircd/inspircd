/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2016, 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2017-2018, 2020-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2008 Craig Edwards <brain@inspircd.org>
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
#include "listmode.h"

// Holds a timed ban
class TimedBan {
  public:
    std::string mask;
    std::string setter;
    time_t expire;
    Channel* chan;
};

typedef std::vector<TimedBan> timedbans;
timedbans TimedBanList;

// Handle /TBAN
class CommandTban : public Command {
    ChanModeReference banmode;

    bool IsBanSet(Channel* chan, const std::string& mask) {
        ListModeBase* banlm = static_cast<ListModeBase*>(*banmode);
        if (!banlm) {
            return false;
        }

        const ListModeBase::ModeList* bans = banlm->GetList(chan);
        if (bans) {
            for (ListModeBase::ModeList::const_iterator i = bans->begin(); i != bans->end();
                    ++i) {
                const ListModeBase::ListItem& ban = *i;
                if (irc::equals(ban.mask, mask)) {
                    return true;
                }
            }
        }

        return false;
    }

  public:
    bool sendnotice;

    CommandTban(Module* Creator)
        : Command(Creator,"TBAN", 3)
        , banmode(Creator, "ban") {
        syntax = "<channel> <duration> <banmask>";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        Channel* channel = ServerInstance->FindChan(parameters[0]);
        if (!channel) {
            user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
            return CMD_FAILURE;
        }

        unsigned int cm = channel->GetPrefixValue(user);
        if (cm < HALFOP_VALUE) {
            user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(channel, HALFOP_VALUE,
                               "set timed bans"));
            return CMD_FAILURE;
        }

        TimedBan T;
        unsigned long duration;
        if (!InspIRCd::Duration(parameters[1], duration)) {
            user->WriteNotice("Invalid ban time");
            return CMD_FAILURE;
        }
        unsigned long expire = duration + ServerInstance->Time();

        std::string mask = parameters[2];
        bool isextban = ((mask.size() > 2) && (mask[1] == ':'));
        if (!isextban && !InspIRCd::IsValidMask(mask)) {
            mask.append("!*@*");
        }

        if (IsBanSet(channel, mask)) {
            user->WriteNotice("Ban already set");
            return CMD_FAILURE;
        }

        Modes::ChangeList setban;
        setban.push_add(*banmode, mask);
        // Pass the user (instead of ServerInstance->FakeClient) to ModeHandler::Process() to
        // make it so that the user sets the mode themselves
        ServerInstance->Modes->Process(user, channel, NULL, setban);
        if (ServerInstance->Modes->GetLastChangeList().empty()) {
            user->WriteNotice("Invalid ban mask");
            return CMD_FAILURE;
        }

        // Attempt to find the actual set ban mask.
        const Modes::ChangeList::List& list = ServerInstance->Modes->GetLastChangeList().getlist();
        for (Modes::ChangeList::List::const_iterator iter = list.begin(); iter != list.end(); ++iter) {
            const Modes::Change& mc = *iter;
            if (mc.mh == *banmode) {
                // We found the actual mask.
                mask = mc.param;
                break;
            }
        }

        T.mask = mask;
        T.setter = user->nick;
        T.expire = expire + (IS_REMOTE(user) ? 5 : 0);
        T.chan = channel;
        TimedBanList.push_back(T);

        if (sendnotice) {
            const std::string message =
                InspIRCd::Format("Timed ban %s added by %s on %s lasting for %s.",
                                 mask.c_str(), user->nick.c_str(), channel->name.c_str(),
                                 InspIRCd::DurationString(duration).c_str());
            // If halfop is loaded, send notice to halfops and above, otherwise send to ops and above
            PrefixMode* mh = ServerInstance->Modes.FindNearestPrefixMode(HALFOP_VALUE);
            char pfxchar = mh ? mh->GetPrefix() : '@';

            channel->WriteRemoteNotice(message, pfxchar);
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_BROADCAST;
    }
};

class BanWatcher : public ModeWatcher {
  public:
    BanWatcher(Module* parent)
        : ModeWatcher(parent, "ban", MODETYPE_CHANNEL) {
    }

    void AfterMode(User* source, User* dest, Channel* chan,
                   const std::string& banmask, bool adding) CXX11_OVERRIDE {
        if (adding) {
            return;
        }

        for (timedbans::iterator i = TimedBanList.begin(); i != TimedBanList.end(); ++i) {
            if (i->chan != chan) {
                continue;
            }

            const std::string& target = i->mask;
            if (irc::equals(banmask, target)) {
                TimedBanList.erase(i);
                break;
            }
        }
    }
};

class ChannelMatcher {
    Channel* const chan;

  public:
    ChannelMatcher(Channel* ch)
        : chan(ch) {
    }

    bool operator()(const TimedBan& tb) const {
        return (tb.chan == chan);
    }
};

class ModuleTimedBans : public Module {
  private:
    ChanModeReference banmode;
    CommandTban cmd;
    BanWatcher banwatcher;

  public:
    ModuleTimedBans()
        : banmode(this, "ban")
        , cmd(this)
        , banwatcher(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("timedbans");
        cmd.sendnotice = tag->getBool("sendnotice", true);
    }

    void OnBackgroundTimer(time_t curtime) CXX11_OVERRIDE {
        timedbans expired;
        for (timedbans::iterator i = TimedBanList.begin(); i != TimedBanList.end();) {
            if (curtime > i->expire) {
                expired.push_back(*i);
                i = TimedBanList.erase(i);
            } else {
                ++i;
            }
        }

        for (timedbans::iterator i = expired.begin(); i != expired.end(); i++) {
            const std::string mask = i->mask;
            Channel* cr = i->chan;

            if (cmd.sendnotice) {
                const std::string message =
                    InspIRCd::Format("Timed ban %s set by %s on %s has expired.",
                                     mask.c_str(), i->setter.c_str(), cr->name.c_str());
                // If halfop is loaded, send notice to halfops and above, otherwise send to ops and above
                PrefixMode* mh = ServerInstance->Modes.FindNearestPrefixMode(HALFOP_VALUE);
                char pfxchar = mh ? mh->GetPrefix() : '@';

                cr->WriteRemoteNotice(message, pfxchar);
            }

            Modes::ChangeList setban;
            setban.push_remove(*banmode, mask);
            ServerInstance->Modes->Process(ServerInstance->FakeClient, cr, NULL, setban);
        }
    }

    void OnChannelDelete(Channel* chan) CXX11_OVERRIDE {
        // Remove all timed bans affecting the channel from internal bookkeeping
        TimedBanList.erase(std::remove_if(TimedBanList.begin(), TimedBanList.end(), ChannelMatcher(chan)), TimedBanList.end());
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /TBAN command which allows channel operators to add bans which will be expired after the specified period.", VF_COMMON | VF_VENDOR);
    }
};

MODULE_INIT(ModuleTimedBans)
