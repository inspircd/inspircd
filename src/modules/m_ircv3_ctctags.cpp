/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018-2022 Sadie Powell <sadie@witchery.services>
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
#include "modules/cap.h"
#include "modules/ctctags.h"

class CommandTagMsg : public Command {
  private:
    Cap::Capability& cap;
    Events::ModuleEventProvider tagevprov;
    ClientProtocol::EventProvider msgevprov;

    bool FirePreEvents(User* source, MessageTarget& msgtarget,
                       CTCTags::TagMessageDetails& msgdetails) {
        // Inform modules that a TAGMSG wants to be sent.
        ModResult modres;
        FIRST_MOD_RESULT_CUSTOM(tagevprov, CTCTags::EventListener, OnUserPreTagMessage,
                                modres, (source, msgtarget, msgdetails));
        if (modres == MOD_RES_DENY) {
            // Inform modules that a module blocked the TAGMSG.
            FOREACH_MOD_CUSTOM(tagevprov, CTCTags::EventListener, OnUserTagMessageBlocked,
                               (source, msgtarget, msgdetails));
            return false;
        }

        // Check whether a module zapped the message tags.
        if (msgdetails.tags_out.empty()) {
            source->WriteNumeric(ERR_NOTEXTTOSEND, "No tags to send");
            return false;
        }

        // Inform modules that a TAGMSG is about to be sent.
        FOREACH_MOD_CUSTOM(tagevprov, CTCTags::EventListener, OnUserTagMessage, (source,
                           msgtarget, msgdetails));
        return true;
    }

    CmdResult FirePostEvent(User* source, const MessageTarget& msgtarget,
                            const CTCTags::TagMessageDetails& msgdetails) {
        // If the source is local then update its idle time.
        LocalUser* lsource = IS_LOCAL(source);
        if (lsource && msgdetails.update_idle) {
            lsource->idle_lastmsg = ServerInstance->Time();
        }

        // Inform modules that a TAGMSG was sent.
        FOREACH_MOD_CUSTOM(tagevprov, CTCTags::EventListener, OnUserPostTagMessage,
                           (source, msgtarget, msgdetails));
        return CMD_SUCCESS;
    }

    CmdResult HandleChannelTarget(User* source, const Params& parameters,
                                  const char* target, PrefixMode* pm) {
        Channel* chan = ServerInstance->FindChan(target);
        if (!chan) {
            // The target channel does not exist.
            source->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
            return CMD_FAILURE;
        }

        // Fire the pre-message events.
        MessageTarget msgtarget(chan, pm ? pm->GetPrefix() : 0);
        CTCTags::TagMessageDetails msgdetails(parameters.GetTags());
        if (!FirePreEvents(source, msgtarget, msgdetails)) {
            return CMD_FAILURE;
        }

        unsigned int minrank = pm ? pm->GetPrefixRank() : 0;
        CTCTags::TagMessage message(source, chan, msgdetails.tags_out,
                                    msgtarget.status);
        message.SetSideEffect(true);
        const Channel::MemberMap& userlist = chan->GetUsers();
        for (Channel::MemberMap::const_iterator iter = userlist.begin();
                iter != userlist.end(); ++iter) {
            LocalUser* luser = IS_LOCAL(iter->first);

            // Don't send to remote users or the user who is the source.
            if (!luser || luser == source) {
                continue;
            }

            // Don't send to unprivileged or exempt users.
            if (iter->second->getRank() < minrank || msgdetails.exemptions.count(luser)) {
                continue;
            }

            // Send to users if they have the capability.
            if (cap.get(luser)) {
                luser->Send(msgevprov, message);
            }
        }
        return FirePostEvent(source, msgtarget, msgdetails);
    }

    CmdResult HandleServerTarget(User* source, const Params& parameters) {
        // If the source isn't allowed to mass message users then reject
        // the attempt to mass-message users.
        if (!source->HasPrivPermission("users/mass-message")) {
            source->WriteNumeric(ERR_NOPRIVILEGES,
                                 "Permission Denied - You do not have the required operator privileges");
            return CMD_FAILURE;
        }

        // Extract the server glob match from the target parameter.
        std::string servername(parameters[0], 1);

        // Fire the pre-message events.
        MessageTarget msgtarget(&servername);
        CTCTags::TagMessageDetails msgdetails(parameters.GetTags());
        if (!FirePreEvents(source, msgtarget, msgdetails)) {
            return CMD_FAILURE;
        }

        // If the current server name matches the server name glob then send
        // the message out to the local users.
        if (InspIRCd::Match(ServerInstance->Config->ServerName, servername)) {
            CTCTags::TagMessage message(source, "$*", msgdetails.tags_out);
            message.SetSideEffect(true);
            const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
            for (UserManager::LocalList::const_iterator iter = list.begin();
                    iter != list.end(); ++iter) {
                LocalUser* luser = IS_LOCAL(*iter);

                // Don't send to unregistered users or the user who is the source.
                if (luser->registered != REG_ALL || luser == source) {
                    continue;
                }

                // Don't send to exempt users.
                if (msgdetails.exemptions.count(luser)) {
                    continue;
                }

                // Send to users if they have the capability.
                if (cap.get(luser)) {
                    luser->Send(msgevprov, message);
                }
            }
        }

        // Fire the post-message event.
        return FirePostEvent(source, msgtarget, msgdetails);
    }

    CmdResult HandleUserTarget(User* source, const Params& parameters) {
        User* target;
        if (IS_LOCAL(source)) {
            // Local sources can specify either a nick or a nick@server mask as the target.
            const char* targetserver = strchr(parameters[0].c_str(), '@');
            if (targetserver) {
                // The target is a user on a specific server (e.g. jto@tolsun.oulu.fi).
                target = ServerInstance->FindNickOnly(parameters[0].substr(0,
                                                      targetserver - parameters[0].c_str()));
                if (target
                        && strcasecmp(target->server->GetPublicName().c_str(), targetserver + 1)) {
                    target = NULL;
                }
            } else {
                // If the source is a local user then we only look up the target by nick.
                target = ServerInstance->FindNickOnly(parameters[0]);

                // Drop attempts to send a tag message to a server. This usually happens when the
                // server is started in debug mode and a client tries to send a typing notification
                // to a query window created by the debug message.
                if (!target
                        && irc::equals(parameters[0], ServerInstance->FakeClient->GetFullHost())) {
                    return CMD_FAILURE;
                }
            }
        } else {
            // Remote users can only specify a nick or UUID as the target.
            target = ServerInstance->FindNick(parameters[0]);
        }

        if (!target || target->registered != REG_ALL) {
            // The target user does not exist or is not fully registered.
            source->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
            return CMD_FAILURE;
        }

        // Fire the pre-message events.
        MessageTarget msgtarget(target);
        CTCTags::TagMessageDetails msgdetails(parameters.GetTags());
        if (!FirePreEvents(source, msgtarget, msgdetails)) {
            return CMD_FAILURE;
        }

        LocalUser* const localtarget = IS_LOCAL(target);
        if (localtarget && cap.get(localtarget)) {
            // Send to the target if they have the capability and are a local user.
            CTCTags::TagMessage message(source, localtarget, msgdetails.tags_out);
            message.SetSideEffect(true);
            localtarget->Send(msgevprov, message);
        }

        // Fire the post-message event.
        return FirePostEvent(source, msgtarget, msgdetails);
    }

  public:
    CommandTagMsg(Module* Creator, Cap::Capability& Cap)
        : Command(Creator, "TAGMSG", 1)
        , cap(Cap)
        , tagevprov(Creator, "event/tagmsg")
        , msgevprov(Creator, "TAGMSG") {
        allow_empty_last_param = false;
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        if (CommandParser::LoopCall(user, this, parameters, 0)) {
            return CMD_SUCCESS;
        }

        // Check that the source has the message tags capability.
        if (IS_LOCAL(user) && !cap.get(user)) {
            return CMD_FAILURE;
        }

        // The specified message tags were empty.
        if (parameters.GetTags().empty()) {
            user->WriteNumeric(ERR_NOTEXTTOSEND, "No tags to send");
            return CMD_FAILURE;
        }

        // The target is a server glob.
        if (parameters[0][0] == '$') {
            return HandleServerTarget(user, parameters);
        }

        // If the message begins with one or more status characters then look them up.
        const char* target = parameters[0].c_str();
        PrefixMode* targetpfx = NULL;
        for (PrefixMode* pfx; (pfx = ServerInstance->Modes->FindPrefix(target[0])); ++target) {
            // We want the lowest ranked prefix specified.
            if (!targetpfx || pfx->GetPrefixRank() < targetpfx->GetPrefixRank()) {
                targetpfx = pfx;
            }
        }

        if (!target[0]) {
            // The target consisted solely of prefix modes.
            user->WriteNumeric(ERR_NORECIPIENT, "No recipient given");
            return CMD_FAILURE;
        }

        // The target is a channel name.
        if (*target == '#') {
            return HandleChannelTarget(user, parameters, target, targetpfx);
        }

        // The target is a nickname.
        return HandleUserTarget(user, parameters);
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        if (IS_LOCAL(user))
            // This is handled by the OnUserPostTagMessage hook to split the LoopCall pieces
        {
            return ROUTE_LOCALONLY;
        } else {
            return ROUTE_MESSAGE(parameters[0]);
        }
    }
};

class C2CTags : public ClientProtocol::MessageTagProvider {
  private:
    Cap::Capability& cap;

  public:
    bool allowclientonlytags;
    C2CTags(Module* Creator, Cap::Capability& Cap)
        : ClientProtocol::MessageTagProvider(Creator)
        , cap(Cap) {
    }

    ModResult OnProcessTag(User* user, const std::string& tagname,
                           std::string& tagvalue) CXX11_OVERRIDE {
        // A client-only tag is prefixed with a plus sign (+) and otherwise conforms
        // to the format specified in IRCv3.2 tags.
        if (tagname[0] != '+' || tagname.length() < 2 || !allowclientonlytags) {
            return MOD_RES_PASSTHRU;
        }

        // If the user is local then we check whether they have the message-tags cap
        // enabled. If not then we reject all client-only tags originating from them.
        LocalUser* lu = IS_LOCAL(user);
        if (lu && !cap.get(lu)) {
            return MOD_RES_DENY;
        }

        // Remote users have their client-only tags checked by their local server.
        return MOD_RES_ALLOW;
    }

    bool ShouldSendTag(LocalUser* user,
                       const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE {
        return cap.get(user);
    }
};

class ModuleIRCv3CTCTags
    : public Module
    , public CTCTags::EventListener {
  private:
    Cap::Capability cap;
    CommandTagMsg cmd;
    C2CTags c2ctags;
    ChanModeReference moderatedmode;
    ChanModeReference noextmsgmode;

    ModResult CopyClientTags(const ClientProtocol::TagMap& tags_in,
                             ClientProtocol::TagMap& tags_out) {
        for (ClientProtocol::TagMap::const_iterator i = tags_in.begin();
                i != tags_in.end(); ++i) {
            const ClientProtocol::MessageTagData& tagdata = i->second;
            if (tagdata.tagprov == &c2ctags) {
                tags_out.insert(*i);
            }
        }
        return MOD_RES_PASSTHRU;
    }

  public:
    ModuleIRCv3CTCTags()
        : CTCTags::EventListener(this)
        , cap(this, "message-tags")
        , cmd(this, cap)
        , c2ctags(this, cap)
        , moderatedmode(this, "moderated")
        , noextmsgmode(this, "noextmsg") {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        c2ctags.allowclientonlytags = ServerInstance->Config->ConfValue("ctctags")->getBool("allowclientonlytags", true);
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        if (!c2ctags.allowclientonlytags) {
            tokens["CLIENTTAGDENY"] = "*";
        }
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        return CopyClientTags(details.tags_in, details.tags_out);
    }

    ModResult OnUserPreTagMessage(User* user, const MessageTarget& target,
                                  CTCTags::TagMessageDetails& details) CXX11_OVERRIDE {
        if (IS_LOCAL(user) && target.type == MessageTarget::TYPE_CHANNEL) {
            Channel* chan = target.Get<Channel>();
            if (chan->IsModeSet(noextmsgmode) && !chan->HasUser(user)) {
                // The noextmsg mode is set and the user is not in the channel.
                user->WriteNumeric(Numerics::CannotSendTo(chan, "external messages",
                                   *noextmsgmode));
                return MOD_RES_DENY;
            }

            bool no_chan_priv = chan->GetPrefixValue(user) < VOICE_VALUE;
            if (no_chan_priv && chan->IsModeSet(moderatedmode)) {
                // The moderated mode is set and the user has no status rank.
                user->WriteNumeric(Numerics::CannotSendTo(chan, "messages", *noextmsgmode));
                return MOD_RES_DENY;
            }

            if (no_chan_priv
                    && ServerInstance->Config->RestrictBannedUsers != ServerConfig::BUT_NORMAL
                    && chan->IsBanned(user)) {
                // The user is banned in the channel and restrictbannedusers is enabled.
                if (ServerInstance->Config->RestrictBannedUsers ==
                        ServerConfig::BUT_RESTRICT_NOTIFY) {
                    user->WriteNumeric(Numerics::CannotSendTo(chan,
                                       "You cannot send messages to this channel whilst banned."));
                }
                return MOD_RES_DENY;
            }
        }

        return CopyClientTags(details.tags_in, details.tags_out);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the IRCv3 message-tags client capability.", VF_VENDOR | VF_COMMON);
    }
};

MODULE_INIT(ModuleIRCv3CTCTags)
