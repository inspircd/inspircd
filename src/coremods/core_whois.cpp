/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2017-2018, 2020-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/whois.h"

enum SplitWhoisState {
    // Don't split private/secret channels into a separate RPL_WHOISCHANNELS numeric.
    SPLITWHOIS_NONE,

    // Split private/secret channels into a separate RPL_WHOISCHANNELS numeric.
    SPLITWHOIS_SPLIT,

    // Split private/secret channels into a separate RPL_WHOISCHANNELS numeric with RPL_CHANNELSMSG to explain the split.
    SPLITWHOIS_SPLITMSG
};

class WhoisContextImpl : public Whois::Context {
    Events::ModuleEventProvider& lineevprov;

  public:
    WhoisContextImpl(LocalUser* sourceuser, User* targetuser,
                     Events::ModuleEventProvider& evprov)
        : Whois::Context(sourceuser, targetuser)
        , lineevprov(evprov) {
    }

    using Whois::Context::SendLine;
    void SendLine(Numeric::Numeric& numeric) CXX11_OVERRIDE;
};

void WhoisContextImpl::SendLine(Numeric::Numeric& numeric) {
    ModResult MOD_RESULT;
    FIRST_MOD_RESULT_CUSTOM(lineevprov, Whois::LineEventListener, OnWhoisLine,
                            MOD_RESULT, (*this, numeric));

    if (MOD_RESULT != MOD_RES_DENY) {
        source->WriteNumeric(numeric);
    }
}

/** Handle /WHOIS.
 */
class CommandWhois : public SplitCommand {
    ChanModeReference secretmode;
    ChanModeReference privatemode;
    UserModeReference snomaskmode;
    Events::ModuleEventProvider evprov;
    Events::ModuleEventProvider lineevprov;

    void DoWhois(LocalUser* user, User* dest, time_t signon, unsigned long idle);
    void SendChanList(WhoisContextImpl& whois);

  public:
    /** If true then all opers are shown with a generic 'is a server operator' line rather than the oper type. */
    bool genericoper;

    /** How to handle private/secret channels in the WHOIS response. */
    SplitWhoisState splitwhois;



    /** Constructor for whois.
     */
    CommandWhois(Module* parent)
        : SplitCommand(parent, "WHOIS", 1)
        , secretmode(parent, "secret")
        , privatemode(parent, "private")
        , snomaskmode(parent, "snomask")
        , evprov(parent, "event/whois")
        , lineevprov(parent, "event/whoisline") {
        Penalty = 2;
        syntax = "[<servername>] <nick>[,<nick>]+";
    }

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE;
    CmdResult HandleRemote(RemoteUser* target,
                           const Params& parameters) CXX11_OVERRIDE;
};

class WhoisNumericSink {
    WhoisContextImpl& whois;
  public:
    WhoisNumericSink(WhoisContextImpl& whoisref)
        : whois(whoisref) {
    }

    void operator()(Numeric::Numeric& numeric) const {
        whois.SendLine(numeric);
    }
};

class WhoisChanListNumericBuilder : public
    Numeric::GenericBuilder<' ', false, WhoisNumericSink> {
  public:
    WhoisChanListNumericBuilder(WhoisContextImpl& whois)
        : Numeric::GenericBuilder<' ', false, WhoisNumericSink>(WhoisNumericSink(whois),
                RPL_WHOISCHANNELS, false,
                whois.GetSource()->nick.size() + whois.GetTarget()->nick.size() + 1) {
        GetNumeric().push(whois.GetTarget()->nick).push(std::string());
    }
};

class WhoisChanList {
    const SplitWhoisState& splitwhois;
    WhoisChanListNumericBuilder num;
    WhoisChanListNumericBuilder secretnum;
    std::string prefixstr;

    void AddMember(Membership* memb, WhoisChanListNumericBuilder& out) {
        prefixstr.clear();
        const char prefix = memb->GetPrefixChar();
        if (prefix) {
            prefixstr.push_back(prefix);
        }
        out.Add(prefixstr, memb->chan->name);
    }

  public:
    WhoisChanList(WhoisContextImpl& whois, const SplitWhoisState& sws)
        : splitwhois(sws)
        , num(whois)
        , secretnum(whois) {
    }

    void AddVisible(Membership* memb) {
        AddMember(memb, num);
    }

    void AddHidden(Membership* memb) {
        AddMember(memb, splitwhois == SPLITWHOIS_NONE ? num : secretnum);
    }

    void Flush(WhoisContextImpl& whois) {
        num.Flush();
        if (!secretnum.IsEmpty() && splitwhois == SPLITWHOIS_SPLITMSG) {
            whois.SendLine(RPL_CHANNELSMSG, "is on private/secret channels:");
        }
        secretnum.Flush();
    }
};

void CommandWhois::SendChanList(WhoisContextImpl& whois) {
    WhoisChanList chanlist(whois, splitwhois);

    User* const target = whois.GetTarget();
    bool hasoperpriv = whois.GetSource()->HasPrivPermission("users/channel-spy");
    for (User::ChanList::iterator i = target->chans.begin();
            i != target->chans.end(); ++i) {
        Membership* memb = *i;
        Channel* c = memb->chan;

        // Anyone can view channels which are not private or secret.
        if (!c->IsModeSet(privatemode) && !c->IsModeSet(secretmode)) {
            chanlist.AddVisible(memb);
        }

        // Hidden channels are visible when the following conditions are true:
        // (1) The source user and the target user are the same.
        // (2) The source user is a member of the hidden channel.
        // (3) The source user is an oper with the users/channel-spy privilege.
        else if (whois.IsSelfWhois() || c->HasUser(whois.GetSource()) || hasoperpriv) {
            chanlist.AddHidden(memb);
        }
    }

    chanlist.Flush(whois);
}

void CommandWhois::DoWhois(LocalUser* user, User* dest, time_t signon,
                           unsigned long idle) {
    WhoisContextImpl whois(user, dest, lineevprov);

    whois.SendLine(RPL_WHOISUSER, dest->ident, dest->GetDisplayedHost(), '*',
                   dest->GetRealName());
    if (!dest->server->IsULine() && (whois.IsSelfWhois()
                                     || user->HasPrivPermission("users/auspex"))) {
        whois.SendLine(RPL_WHOISHOST, InspIRCd::Format("is connecting from %s@%s %s",
                       dest->ident.c_str(), dest->GetRealHost().c_str(), dest->GetIPString().c_str()));
    }

    SendChanList(whois);

    if (!whois.IsSelfWhois() && !ServerInstance->Config->HideServer.empty()
            && !user->HasPrivPermission("servers/auspex")) {
        whois.SendLine(RPL_WHOISSERVER, ServerInstance->Config->HideServer,
                       ServerInstance->Config->Network);
    } else {
        whois.SendLine(RPL_WHOISSERVER, dest->server->GetName(),
                       dest->server->GetDesc());
    }

    if (dest->IsAway()) {
        whois.SendLine(RPL_AWAY, dest->awaymsg);
    }

    if (dest->IsOper()) {
        if (genericoper) {
            whois.SendLine(RPL_WHOISOPERATOR,
                           dest->server->IsULine() ? "is a network service" : "is a server operator");
        } else {
            whois.SendLine(RPL_WHOISOPERATOR, InspIRCd::Format("is %s %s on %s",
                           (strchr("AEIOUaeiou",dest->oper->name[0]) ? "an" : "a"),
                           dest->oper->name.c_str(), ServerInstance->Config->Network.c_str()));
        }
    }

    if (whois.IsSelfWhois() || user->HasPrivPermission("users/auspex")) {
        if (dest->IsModeSet(snomaskmode)) {
            whois.SendLine(RPL_WHOISMODES, InspIRCd::Format("is using modes %s %s",
                           dest->GetModeLetters().c_str(), snomaskmode->GetUserParameter(dest).c_str()));
        } else {
            whois.SendLine(RPL_WHOISMODES, InspIRCd::Format("is using modes %s",
                           dest->GetModeLetters().c_str()));
        }
    }

    FOREACH_MOD_CUSTOM(evprov, Whois::EventListener, OnWhois, (whois));

    /*
     * We only send these if we've been provided them. That is, if hideserver is turned off, and user is local, or
     * if remote whois is queried, too. This is to keep the user hidden, and also since you can't reliably tell remote time. -- w00t
     */
    if ((idle) || (signon)) {
        whois.SendLine(RPL_WHOISIDLE, idle, signon, "seconds idle, signon time");
    }

    whois.SendLine(RPL_ENDOFWHOIS, "End of /WHOIS list.");
}

CmdResult CommandWhois::HandleRemote(RemoteUser* target,
                                     const Params& parameters) {
    if (parameters.size() < 2) {
        return CMD_FAILURE;
    }

    User* user = ServerInstance->FindUUID(parameters[0]);
    if (!user) {
        return CMD_FAILURE;
    }

    // User doing the whois must be on this server
    LocalUser* localuser = IS_LOCAL(user);
    if (!localuser) {
        return CMD_FAILURE;
    }

    unsigned long idle = ConvToNum<unsigned long>(parameters.back());
    DoWhois(localuser, target, target->signon, idle);

    return CMD_SUCCESS;
}

CmdResult CommandWhois::HandleLocal(LocalUser* user, const Params& parameters) {
    User *dest;
    unsigned int userindex = 0;
    unsigned long idle = 0;
    time_t signon = 0;

    if (CommandParser::LoopCall(user, this, parameters, 0)) {
        return CMD_SUCCESS;
    }

    /*
     * If 2 parameters are specified (/whois nick nick), ignore the first one like spanningtree
     * does, and use the second one, otherwise, use the only parameter. -- djGrrr
     */
    if (parameters.size() > 1) {
        userindex = 1;
    }

    if (parameters[userindex].empty()) {
        user->WriteNumeric(ERR_NONICKNAMEGIVEN, "No nickname given");
        return CMD_FAILURE;
    }

    dest = ServerInstance->FindNickOnly(parameters[userindex]);
    if ((dest) && (dest->registered == REG_ALL)) {
        /*
         * Okay. Umpteenth attempt at doing this, so let's re-comment...
         * For local users (/w localuser), we show idletime if hideserver is disabled
         * For local users (/w localuser localuser), we always show idletime, hence parameters.size() > 1 check.
         * For remote users (/w remoteuser), we do NOT show idletime
         * For remote users (/w remoteuser remoteuser), spanningtree will handle calling do_whois, so we can ignore this case.
         * Thanks to djGrrr for not being impatient while I have a crap day coding. :p -- w00t
         */
        LocalUser* localuser = IS_LOCAL(dest);
        if (localuser && (ServerInstance->Config->HideServer.empty()
                          || parameters.size() > 1)) {
            idle = labs((long)((localuser->idle_lastmsg)-ServerInstance->Time()));
            signon = dest->signon;
        }

        DoWhois(user,dest,signon,idle);
    } else {
        /* no such nick/channel */
        user->WriteNumeric(Numerics::NoSuchNick(parameters[userindex]));
        user->WriteNumeric(RPL_ENDOFWHOIS, parameters[userindex],
                           "End of /WHOIS list.");
        return CMD_FAILURE;
    }

    return CMD_SUCCESS;
}

class CoreModWhois : public Module {
  private:
    CommandWhois cmd;

  public:
    CoreModWhois()
        : cmd(this) {
    }

    void ReadConfig(ConfigStatus&) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("options");
        const std::string splitwhois = tag->getString("splitwhois", "no", 1);
        SplitWhoisState newsplitstate;
        if (stdalgo::string::equalsci(splitwhois, "no")) {
            newsplitstate = SPLITWHOIS_NONE;
        } else if (stdalgo::string::equalsci(splitwhois, "split")) {
            newsplitstate = SPLITWHOIS_SPLIT;
        } else if (stdalgo::string::equalsci(splitwhois, "splitmsg")) {
            newsplitstate = SPLITWHOIS_SPLITMSG;
        } else {
            throw ModuleException(splitwhois +
                                  " is an invalid <options:splitwhois> value, at " + tag->getTagLocation());
        }

        ConfigTag* security = ServerInstance->Config->ConfValue("security");
        cmd.genericoper = security->getBool("genericoper");
        cmd.splitwhois = newsplitstate;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the WHOIS command", VF_VENDOR|VF_CORE);
    }
};

MODULE_INIT(CoreModWhois)
