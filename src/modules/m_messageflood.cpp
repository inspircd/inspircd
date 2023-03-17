/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/ctctags.h"
#include "modules/exemption.h"

/** Holds flood settings and state for mode +f
 */
class floodsettings {
  public:
    bool ban;
    unsigned int secs;
    unsigned int lines;
    time_t reset;
    insp::flat_map<User*, double> counters;

    floodsettings(bool a, unsigned int b, unsigned int c)
        : ban(a)
        , secs(b)
        , lines(c) {
        reset = ServerInstance->Time() + secs;
    }

    bool addmessage(User* who, double weight) {
        if (ServerInstance->Time() > reset) {
            counters.clear();
            reset = ServerInstance->Time() + secs;
        }

        counters[who] += weight;
        return (counters[who] >= this->lines);
    }

    void clear(User* who) {
        counters.erase(who);
    }
};

/** Handles channel mode +f
 */
class MsgFlood : public ParamMode<MsgFlood, SimpleExtItem<floodsettings> > {
  public:
    MsgFlood(Module* Creator)
        : ParamMode<MsgFlood, SimpleExtItem<floodsettings> >(Creator, "flood", 'f') {
        syntax = "[*]<messages>:<seconds>";
    }

    ModeAction OnSet(User* source, Channel* channel,
                     std::string& parameter) CXX11_OVERRIDE {
        std::string::size_type colon = parameter.find(':');
        if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos)) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }

        /* Set up the flood parameters for this channel */
        bool ban = (parameter[0] == '*');
        unsigned int nlines = ConvToNum<unsigned int>(parameter.substr(ban ? 1 : 0, ban ? colon-1 : colon));
        unsigned int nsecs = ConvToNum<unsigned int>(parameter.substr(colon+1));

        if ((nlines<2) || (nsecs<1)) {
            source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
            return MODEACTION_DENY;
        }

        ext.set(channel, new floodsettings(ban, nsecs, nlines));
        return MODEACTION_ALLOW;
    }

    void SerializeParam(Channel* chan, const floodsettings* fs, std::string& out) {
        if (fs->ban) {
            out.push_back('*');
        }
        out.append(ConvToStr(fs->lines)).push_back(':');
        out.append(ConvToStr(fs->secs));
    }
};

class ModuleMsgFlood
    : public Module
    , public CTCTags::EventListener {
  private:
    ChanModeReference banmode;
    CheckExemption::EventProvider exemptionprov;
    MsgFlood mf;
    double notice;
    double privmsg;
    double tagmsg;

  public:
    ModuleMsgFlood()
        : CTCTags::EventListener(this)
        , banmode(this, "ban")
        , exemptionprov(this)
        , mf(this) {
    }

    void ReadConfig(ConfigStatus&) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("messageflood");
        notice = tag->getFloat("notice", 1.0);
        privmsg = tag->getFloat("privmsg", 1.0);
        tagmsg = tag->getFloat("tagmsg", 0.2);
    }

    ModResult HandleMessage(User* user, const MessageTarget& target,
                            double weight) {
        if (target.type != MessageTarget::TYPE_CHANNEL) {
            return MOD_RES_PASSTHRU;
        }

        Channel* dest = target.Get<Channel>();
        if ((!IS_LOCAL(user)) || !dest->IsModeSet(mf)) {
            return MOD_RES_PASSTHRU;
        }

        ModResult res = CheckExemption::Call(exemptionprov, user, dest, "flood");
        if (res == MOD_RES_ALLOW) {
            return MOD_RES_PASSTHRU;
        }

        floodsettings *f = mf.ext.get(dest);
        if (f) {
            if (f->addmessage(user, weight)) {
                /* You're outttta here! */
                f->clear(user);
                if (f->ban) {
                    Modes::ChangeList changelist;
                    changelist.push_add(*banmode,
                                        "*!" + user->GetBanIdent() + "@" + user->GetDisplayedHost());
                    ServerInstance->Modes->Process(ServerInstance->FakeClient, dest, NULL,
                                                   changelist);
                }

                const std::string kickMessage = "Channel flood triggered (trigger is " +
                                                ConvToStr(f->lines) +
                                                " lines in " + ConvToStr(f->secs) + " secs)";

                dest->KickUser(ServerInstance->FakeClient, user, kickMessage);

                return MOD_RES_DENY;
            }
        }

        return MOD_RES_PASSTHRU;
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        return HandleMessage(user, target, (details.type == MSG_PRIVMSG ? privmsg : notice));
    }

    ModResult OnUserPreTagMessage(User* user, const MessageTarget& target,
                                  CTCTags::TagMessageDetails& details) CXX11_OVERRIDE {
        return HandleMessage(user, target, tagmsg);
    }

    void Prioritize() CXX11_OVERRIDE {
        // we want to be after all modules that might deny the message (e.g. m_muteban, m_noctcp, m_blockcolor, etc.)
        ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode f (flood) which helps protect against spammers which mass-message channels.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleMsgFlood)
