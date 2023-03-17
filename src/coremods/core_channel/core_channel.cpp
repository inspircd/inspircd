/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2017-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2015, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "core_channel.h"
#include "invite.h"
#include "listmode.h"

namespace {
/** Hook that sends a MODE after a JOIN if the user in the JOIN has some modes prefix set.
 * This happens e.g. when modules such as operprefix explicitly set prefix modes on the joining
 * user, or when a member with prefix modes does a host cycle.
 */
class JoinHook : public ClientProtocol::EventHook {
    ClientProtocol::Messages::Mode modemsg;
    Modes::ChangeList modechangelist;
    const User* joininguser;

  public:
    /** If true, MODE changes after JOIN will be sourced from the user, rather than the server
     */
    bool modefromuser;

    JoinHook(Module* mod)
        : ClientProtocol::EventHook(mod, "JOIN") {
    }

    void OnEventInit(const ClientProtocol::Event& ev) CXX11_OVERRIDE {
        const ClientProtocol::Events::Join& join = static_cast<const ClientProtocol::Events::Join&>(ev);
        const Membership& memb = *join.GetMember();

        modechangelist.clear();
        for (std::string::const_iterator i = memb.modes.begin(); i != memb.modes.end(); ++i) {
            PrefixMode* const pm = ServerInstance->Modes.FindPrefixMode(*i);
            if (!pm) {
                continue;    // Shouldn't happen
            }
            modechangelist.push_add(pm, memb.user->nick);
        }

        if (modechangelist.empty()) {
            // Member got no modes on join
            joininguser = NULL;
            return;
        }

        joininguser = memb.user;

        // Prepare a mode protocol event that we can append to the message list in OnPreEventSend()
        modemsg.SetParams(memb.chan, NULL, modechangelist);
        if (modefromuser) {
            modemsg.SetSource(join);
        } else {
            modemsg.SetSourceUser(ServerInstance->FakeClient);
        }
    }

    ModResult OnPreEventSend(LocalUser* user, const ClientProtocol::Event& ev,
                             ClientProtocol::MessageList& messagelist) CXX11_OVERRIDE {
        // If joininguser is NULL then they didn't get any modes on join, skip.
        // Also don't show their own modes to them, they get that in the NAMES list not via MODE.
        if ((joininguser) && (user != joininguser)) {
            messagelist.push_back(&modemsg);
        }
        return MOD_RES_PASSTHRU;
    }
};

}

class CoreModChannel : public Module, public CheckExemption::EventListener {
    Invite::APIImpl invapi;
    CommandInvite cmdinvite;
    CommandJoin cmdjoin;
    CommandKick cmdkick;
    CommandNames cmdnames;
    CommandTopic cmdtopic;
    Events::ModuleEventProvider evprov;
    JoinHook joinhook;

    ModeChannelBan banmode;
    SimpleChannelModeHandler inviteonlymode;
    ModeChannelKey keymode;
    ModeChannelLimit limitmode;
    SimpleChannelModeHandler moderatedmode;
    SimpleChannelModeHandler noextmsgmode;
    ModeChannelOp opmode;
    SimpleChannelModeHandler privatemode;
    SimpleChannelModeHandler secretmode;
    SimpleChannelModeHandler topiclockmode;
    ModeChannelVoice voicemode;

    insp::flat_map<std::string, char> exemptions;

    ModResult IsInvited(User* user, Channel* chan) {
        LocalUser* localuser = IS_LOCAL(user);
        if ((localuser) && (invapi.IsInvited(localuser, chan))) {
            return MOD_RES_ALLOW;
        }
        return MOD_RES_PASSTHRU;
    }

  public:
    CoreModChannel()
        : CheckExemption::EventListener(this, UINT_MAX)
        , invapi(this)
        , cmdinvite(this, invapi)
        , cmdjoin(this)
        , cmdkick(this)
        , cmdnames(this)
        , cmdtopic(this)
        , evprov(this, "event/channel")
        , joinhook(this)
        , banmode(this)
        , inviteonlymode(this, "inviteonly", 'i')
        , keymode(this)
        , limitmode(this)
        , moderatedmode(this, "moderated", 'm')
        , noextmsgmode(this, "noextmsg", 'n')
        , opmode(this)
        , privatemode(this, "private", 'p')
        , secretmode(this, "secret", 's')
        , topiclockmode(this, "topiclock", 't')
        , voicemode(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* optionstag = ServerInstance->Config->ConfValue("options");

        std::string current;
        irc::spacesepstream defaultstream(optionstag->getString("exemptchanops"));
        insp::flat_map<std::string, char> exempts;
        while (defaultstream.GetToken(current)) {
            std::string::size_type pos = current.find(':');
            if (pos == std::string::npos || (pos + 2) > current.size()) {
                throw ModuleException("Invalid exemptchanops value '" + current + "' at " +
                                      optionstag->getTagLocation());
            }

            const std::string restriction = current.substr(0, pos);
            const char prefix = current[pos + 1];

            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Exempting prefix %c from %s",
                                      prefix, restriction.c_str());
            exempts[restriction] = prefix;
        }

        ConfigTag* securitytag = ServerInstance->Config->ConfValue("security");
        const std::string announceinvites = securitytag->getString("announceinvites", "dynamic", 1);
        Invite::AnnounceState newannouncestate;
        if (stdalgo::string::equalsci(announceinvites, "none")) {
            newannouncestate = Invite::ANNOUNCE_NONE;
        } else if (stdalgo::string::equalsci(announceinvites, "all")) {
            newannouncestate = Invite::ANNOUNCE_ALL;
        } else if (stdalgo::string::equalsci(announceinvites, "ops")) {
            newannouncestate = Invite::ANNOUNCE_OPS;
        } else if (stdalgo::string::equalsci(announceinvites, "dynamic")) {
            newannouncestate = Invite::ANNOUNCE_DYNAMIC;
        } else {
            throw ModuleException(announceinvites +
                                  " is an invalid <security:announceinvites> value, at " +
                                  securitytag->getTagLocation());
        }

        // Config is valid, apply it

        // Validates and applies <maxlist> tags, so do it first
        banmode.DoRehash();

        exemptions.swap(exempts);
        // In 2.0 we allowed limits of 0 to be set. This is non-standard behaviour
        // and will be removed in the next major release.
        limitmode.minlimit = optionstag->getBool("allowzerolimit", true) ? 0 : 1;;
        invapi.announceinvites = newannouncestate;
        joinhook.modefromuser = optionstag->getBool("cyclehostsfromuser");

        Implementation events[] = { I_OnCheckKey, I_OnCheckLimit, I_OnCheckChannelBan };
        if (optionstag->getBool("invitebypassmodes", true)) {
            ServerInstance->Modules.Attach(events, this,
                                           sizeof(events)/sizeof(Implementation));
        } else {
            for (unsigned int i = 0; i < sizeof(events)/sizeof(Implementation); i++) {
                ServerInstance->Modules.Detach(events[i], this);
            }
        }
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["KEYLEN"] = ConvToStr(ModeChannelKey::maxkeylen);

        std::vector<std::string> limits;
        std::string vlist;
        const ModeParser::ListModeList& listmodes = ServerInstance->Modes->GetListModes();
        for (ModeParser::ListModeList::const_iterator iter = listmodes.begin(); iter != listmodes.end(); ++iter) {
            ListModeBase* lm = *iter;
            limits.push_back(InspIRCd::Format("%c:%u", lm->GetModeChar(),
                                              lm->GetLowerLimit()));
            if (lm->HasVariableLength()) {
                vlist.push_back(lm->GetModeChar());
            }
        }

        std::sort(limits.begin(), limits.end());
        tokens["MAXLIST"] = stdalgo::string::join(limits, ',');

        if (!vlist.empty()) {
            tokens["VBANLIST"]; // deprecated
            tokens["VLIST"] = vlist;
        }
    }

    ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string&,
                            std::string&, const std::string& keygiven) CXX11_OVERRIDE {
        if (!chan) {
            return MOD_RES_PASSTHRU;
        }

        // Check whether the channel key is correct.
        const std::string ckey = chan->GetModeParameter(&keymode);
        if (!ckey.empty()) {
            ModResult MOD_RESULT;
            FIRST_MOD_RESULT(OnCheckKey, MOD_RESULT, (user, chan, keygiven));
            if (!MOD_RESULT.check(InspIRCd::TimingSafeCompare(ckey, keygiven))) {
                // If no key provided, or key is not the right one, and can't bypass +k (not invited or option not enabled)
                user->WriteNumeric(ERR_BADCHANNELKEY, chan->name,
                                   "Cannot join channel (incorrect channel key)");
                return MOD_RES_DENY;
            }
        }

        // Check whether the invite only mode is set.
        if (chan->IsModeSet(inviteonlymode)) {
            ModResult MOD_RESULT;
            FIRST_MOD_RESULT(OnCheckInvite, MOD_RESULT, (user, chan));
            if (MOD_RESULT != MOD_RES_ALLOW) {
                user->WriteNumeric(ERR_INVITEONLYCHAN, chan->name,
                                   "Cannot join channel (invite only)");
                return MOD_RES_DENY;
            }
        }

        // Check whether the limit would be exceeded by this user joining.
        if (chan->IsModeSet(limitmode)) {
            ModResult MOD_RESULT;
            FIRST_MOD_RESULT(OnCheckLimit, MOD_RESULT, (user, chan));
            if (!MOD_RESULT.check(chan->GetUserCounter() < static_cast<size_t>
                                  (limitmode.ext.get(chan)))) {
                user->WriteNumeric(ERR_CHANNELISFULL, chan->name,
                                   "Cannot join channel (channel is full)");
                return MOD_RES_DENY;
            }
        }

        // Everything looks okay.
        return MOD_RES_PASSTHRU;
    }

    void OnPostJoin(Membership* memb) CXX11_OVERRIDE {
        Channel* const chan = memb->chan;
        LocalUser* const localuser = IS_LOCAL(memb->user);
        if (localuser) {
            // Remove existing invite, if any
            invapi.Remove(localuser, chan);

            if (chan->topic.length()) {
                Topic::ShowTopic(localuser, chan);
            }

            // Show all members of the channel, including invisible (+i) users
            cmdnames.SendNames(localuser, chan, true);
        }
    }

    ModResult OnCheckKey(User* user, Channel* chan,
                         const std::string& keygiven) CXX11_OVERRIDE {
        // Hook only runs when being invited bypasses +bkl
        return IsInvited(user, chan);
    }

    ModResult OnCheckChannelBan(User* user, Channel* chan) CXX11_OVERRIDE {
        // Hook only runs when being invited bypasses +bkl
        return IsInvited(user, chan);
    }

    ModResult OnCheckLimit(User* user, Channel* chan) CXX11_OVERRIDE {
        // Hook only runs when being invited bypasses +bkl
        return IsInvited(user, chan);
    }

    ModResult OnCheckInvite(User* user, Channel* chan) CXX11_OVERRIDE {
        // Hook always runs
        return IsInvited(user, chan);
    }

    void OnUserDisconnect(LocalUser* user) CXX11_OVERRIDE {
        invapi.RemoveAll(user);
    }

    void OnChannelDelete(Channel* chan) CXX11_OVERRIDE {
        // Make sure the channel won't appear in invite lists from now on, don't wait for cull to unset the ext
        invapi.RemoveAll(chan);
    }

    ModResult OnCheckExemption(User* user, Channel* chan,
                               const std::string& restriction) CXX11_OVERRIDE {
        if (!exemptions.count(restriction)) {
            return MOD_RES_PASSTHRU;
        }

        unsigned int mypfx = chan->GetPrefixValue(user);
        char minmode = exemptions[restriction];

        PrefixMode* mh = ServerInstance->Modes->FindPrefixMode(minmode);
        if (mh && mypfx >= mh->GetPrefixRank()) {
            return MOD_RES_ALLOW;
        }
        if (mh || minmode == '*') {
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }

    void Prioritize() CXX11_OVERRIDE {
        ServerInstance->Modules.SetPriority(this, I_OnPostJoin, PRIORITY_FIRST);
        ServerInstance->Modules.SetPriority(this, I_OnUserPreJoin, PRIORITY_LAST);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the INVITE, JOIN, KICK, NAMES, and TOPIC commands", VF_VENDOR|VF_CORE);
    }
};

MODULE_INIT(CoreModChannel)
