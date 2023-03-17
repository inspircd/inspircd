/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 John Brooks <special@inspircd.org>
 *   Copyright (C) 2008-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/callerid.h"
#include "modules/ctctags.h"

enum {
    RPL_ACCEPTLIST = 281,
    RPL_ENDOFACCEPT = 282,
    ERR_ACCEPTFULL = 456,
    ERR_ACCEPTEXIST = 457,
    ERR_ACCEPTNOT = 458,
    ERR_TARGUMODEG = 716,
    RPL_TARGNOTIFY = 717,
    RPL_UMODEGMSG = 718
};

class callerid_data {
  public:
    typedef insp::flat_set<User*> UserSet;
    typedef std::vector<callerid_data*> CallerIdDataSet;

    time_t lastnotify;

    /** Users I accept messages from
     */
    UserSet accepting;

    /** Users who list me as accepted
     */
    CallerIdDataSet wholistsme;

    callerid_data() : lastnotify(0) { }

    std::string ToString(bool human) const {
        std::ostringstream oss;
        oss << lastnotify;
        for (UserSet::const_iterator i = accepting.begin(); i != accepting.end(); ++i) {
            User* u = *i;
            if (human) {
                oss << ' ' << u->nick;
            } else {
                oss << ',' << u->uuid;
            }
        }
        return oss.str();
    }
};

struct CallerIDExtInfo : public ExtensionItem {
    CallerIDExtInfo(Module* parent)
        : ExtensionItem("callerid_data", ExtensionItem::EXT_USER, parent) {
    }

    std::string ToHuman(const Extensible* container,
                        void* item) const CXX11_OVERRIDE {
        callerid_data* dat = static_cast<callerid_data*>(item);
        return dat->ToString(true);
    }

    std::string ToInternal(const Extensible* container,
                           void* item) const CXX11_OVERRIDE {
        callerid_data* dat = static_cast<callerid_data*>(item);
        return dat->ToString(false);
    }

    void FromInternal(Extensible* container,
                      const std::string& value) CXX11_OVERRIDE {
        void* old = get_raw(container);
        if (old) {
            this->free(NULL, old);
        }
        callerid_data* dat = new callerid_data;
        set_raw(container, dat);

        irc::commasepstream s(value);
        std::string tok;
        if (s.GetToken(tok)) {
            dat->lastnotify = ConvToNum<time_t>(tok);
        }

        while (s.GetToken(tok)) {
            User *u = ServerInstance->FindNick(tok);
            if ((u) && (u->registered == REG_ALL) && (!u->quitting)) {
                if (dat->accepting.insert(u).second) {
                    callerid_data* other = this->get(u, true);
                    other->wholistsme.push_back(dat);
                }
            }
        }
    }

    callerid_data* get(User* user, bool create) {
        callerid_data* dat = static_cast<callerid_data*>(get_raw(user));
        if (create && !dat) {
            dat = new callerid_data;
            set_raw(user, dat);
        }
        return dat;
    }

    void free(Extensible* container, void* item) CXX11_OVERRIDE {
        callerid_data* dat = static_cast<callerid_data*>(item);

        // We need to walk the list of users on our accept list, and remove ourselves from their wholistsme.
        for (callerid_data::UserSet::iterator it = dat->accepting.begin(); it != dat->accepting.end(); ++it) {
            callerid_data* target = this->get(*it, false);
            if (!target) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "ERROR: Inconsistency detected in callerid state, please report (1)");
                continue; // shouldn't happen, but oh well.
            }

            if (!stdalgo::vector::swaperase(target->wholistsme, dat)) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "ERROR: Inconsistency detected in callerid state, please report (2)");
            }
        }
        delete dat;
    }
};

class CommandAccept : public Command {
    /** Pair: first is the target, second is true to add, false to remove
     */
    typedef std::pair<User*, bool> ACCEPTAction;

    static ACCEPTAction GetTargetAndAction(std::string& tok, User* cmdfrom = NULL) {
        bool remove = (tok[0] == '-');
        if ((remove) || (tok[0] == '+')) {
            tok.erase(tok.begin());
        }

        User* target;
        if (!cmdfrom || !IS_LOCAL(cmdfrom)) {
            target = ServerInstance->FindNick(tok);
        } else {
            target = ServerInstance->FindNickOnly(tok);
        }

        if ((!target) || (target->registered != REG_ALL) || (target->quitting)) {
            target = NULL;
        }

        return std::make_pair(target, !remove);
    }

  public:
    CallerIDExtInfo extInfo;
    unsigned int maxaccepts;
    CommandAccept(Module* Creator) : Command(Creator, "ACCEPT", 1),
        extInfo(Creator) {
        allow_empty_last_param = false;
        syntax = "*|(+|-)<nick>[,(+|-)<nick>]+";
        TRANSLATE1(TR_CUSTOM);
    }

    void EncodeParameter(std::string& parameter,
                         unsigned int index) CXX11_OVERRIDE {
        // Send lists as-is (part of 2.0 compat)
        if (parameter.find(',') != std::string::npos) {
            return;
        }

        // Convert a (+|-)<nick> into a [-]<uuid>
        ACCEPTAction action = GetTargetAndAction(parameter);
        if (!action.first) {
            return;
        }

        parameter = (action.second ? "" : "-") + action.first->uuid;
    }

    /** Will take any number of nicks (up to MaxTargets), which can be separated by commas.
     * - in front of any nick removes, and an * lists. This effectively means you can do:
     * /accept nick1,nick2,nick3,*
     * to add 3 nicks and then show your list
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        if (CommandParser::LoopCall(user, this, parameters, 0)) {
            return CMD_SUCCESS;
        }

        /* Even if callerid mode is not set, we let them manage their ACCEPT list so that if they go +g they can
         * have a list already setup. */

        if (parameters[0] == "*") {
            ListAccept(user);
            return CMD_SUCCESS;
        }

        std::string tok = parameters[0];
        ACCEPTAction action = GetTargetAndAction(tok, user);
        if (!action.first) {
            user->WriteNumeric(Numerics::NoSuchNick(tok));
            return CMD_FAILURE;
        }

        if ((!IS_LOCAL(user)) && (!IS_LOCAL(action.first)))
            // Neither source nor target is local, forward the command to the server of target
        {
            return CMD_SUCCESS;
        }

        // The second item in the pair is true if the first char is a '+' (or nothing), false if it's a '-'
        if (action.second) {
            return (AddAccept(user, action.first) ? CMD_SUCCESS : CMD_FAILURE);
        } else {
            return (RemoveAccept(user, action.first) ? CMD_SUCCESS : CMD_FAILURE);
        }
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        // There is a list in parameters[0] in two cases:
        // Either when the source is remote, this happens because 2.0 servers send comma separated uuid lists,
        // we don't split those but broadcast them, as before.
        //
        // Or if the source is local then LoopCall() runs OnPostCommand() after each entry in the list,
        // meaning the linking module has sent an ACCEPT already for each entry in the list to the
        // appropriate server and the ACCEPT with the list of nicks (this) doesn't need to be sent anywhere.
        if ((!IS_LOCAL(user)) && (parameters[0].find(',') != std::string::npos)) {
            return ROUTE_BROADCAST;
        }

        // Find the target
        std::string targetstring = parameters[0];
        ACCEPTAction action = GetTargetAndAction(targetstring, user);
        if (!action.first)
            // Target is a "*" or source is local and the target is a list of nicks
        {
            return ROUTE_LOCALONLY;
        }

        // Route to the server of the target
        return ROUTE_UNICAST(action.first->server);
    }

    void ListAccept(User* user) {
        callerid_data* dat = extInfo.get(user, false);
        if (dat) {
            for (callerid_data::UserSet::iterator i = dat->accepting.begin();
                    i != dat->accepting.end(); ++i) {
                user->WriteNumeric(RPL_ACCEPTLIST, (*i)->nick);
            }
        }
        user->WriteNumeric(RPL_ENDOFACCEPT, "End of ACCEPT list");
    }

    bool AddAccept(User* user, User* whotoadd) {
        // Add this user to my accept list first, so look me up..
        callerid_data* dat = extInfo.get(user, true);
        if (dat->accepting.size() >= maxaccepts) {
            user->WriteNumeric(ERR_ACCEPTFULL,
                               InspIRCd::Format("Accept list is full (limit is %d)", maxaccepts));
            return false;
        }
        if (!dat->accepting.insert(whotoadd).second) {
            user->WriteNumeric(ERR_ACCEPTEXIST, whotoadd->nick,
                               "is already on your accept list");
            return false;
        }

        // Now, look them up, and add me to their list
        callerid_data* target = extInfo.get(whotoadd, true);
        target->wholistsme.push_back(dat);

        user->WriteNotice(whotoadd->nick + " is now on your accept list");
        return true;
    }

    bool RemoveAccept(User* user, User* whotoremove) {
        // Remove them from my list, so look up my list..
        callerid_data* dat = extInfo.get(user, false);
        if (!dat) {
            user->WriteNumeric(ERR_ACCEPTNOT, whotoremove->nick,
                               "is not on your accept list");
            return false;
        }
        if (!dat->accepting.erase(whotoremove)) {
            user->WriteNumeric(ERR_ACCEPTNOT, whotoremove->nick,
                               "is not on your accept list");
            return false;
        }

        // Look up their list to remove me.
        callerid_data *dat2 = extInfo.get(whotoremove, false);
        if (!dat2) {
            // How the fuck is this possible.
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                      "ERROR: Inconsistency detected in callerid state, please report (3)");
            return false;
        }

        if (!stdalgo::vector::swaperase(dat2->wholistsme, dat)) {
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                      "ERROR: Inconsistency detected in callerid state, please report (4)");
        }


        user->WriteNotice(whotoremove->nick + " is no longer on your accept list");
        return true;
    }
};

class CallerIDAPIImpl : public CallerID::APIBase {
  private:
    CallerIDExtInfo& ext;

  public:
    CallerIDAPIImpl(Module* Creator, CallerIDExtInfo& Ext)
        : CallerID::APIBase(Creator)
        , ext(Ext) {
    }

    bool IsOnAcceptList(User* source, User* target) CXX11_OVERRIDE {
        callerid_data* dat = ext.get(target, true);
        return dat->accepting.count(source);
    }
};


class ModuleCallerID
    : public Module
    , public CTCTags::EventListener {
    CommandAccept cmd;
    CallerIDAPIImpl api;
    SimpleUserModeHandler myumode;

    // Configuration variables:
    bool tracknick; // Allow ACCEPT entries to update with nick changes.
    unsigned int notify_cooldown; // Seconds between notifications.

    /** Removes a user from all accept lists
     * @param who The user to remove from accepts
     */
    void RemoveFromAllAccepts(User* who) {
        // First, find the list of people who have me on accept
        callerid_data *userdata = cmd.extInfo.get(who, false);
        if (!userdata) {
            return;
        }

        // Iterate over the list of people who accept me, and remove all entries
        for (callerid_data::CallerIdDataSet::iterator it = userdata->wholistsme.begin();
                it != userdata->wholistsme.end(); ++it) {
            callerid_data *dat = *(it);

            // Find me on their callerid list
            if (!dat->accepting.erase(who)) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "ERROR: Inconsistency detected in callerid state, please report (5)");
            }
        }

        userdata->wholistsme.clear();
    }

  public:
    ModuleCallerID()
        : CTCTags::EventListener(this)
        , cmd(this)
        , api(this, cmd.extInfo)
        , myumode(this, "callerid", 'g') {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides user mode g (callerid) which allows users to require that other users are on their whitelist before messaging them.", VF_COMMON | VF_VENDOR);
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["ACCEPT"] = ConvToStr(cmd.maxaccepts);
        tokens["CALLERID"] = ConvToStr(myumode.GetModeChar());
    }

    ModResult HandleMessage(User* user, const MessageTarget& target) {
        if (!IS_LOCAL(user) || target.type != MessageTarget::TYPE_USER) {
            return MOD_RES_PASSTHRU;
        }

        User* dest = target.Get<User>();
        if (!dest->IsModeSet(myumode) || (user == dest)) {
            return MOD_RES_PASSTHRU;
        }

        if (user->HasPrivPermission("users/ignore-callerid")) {
            return MOD_RES_PASSTHRU;
        }

        callerid_data* dat = cmd.extInfo.get(dest, true);
        if (!dat->accepting.count(user)) {
            time_t now = ServerInstance->Time();
            /* +g and *not* accepted */
            user->WriteNumeric(ERR_TARGUMODEG, dest->nick,
                               "is in +g mode (server-side ignore).");
            if (now > (dat->lastnotify + (time_t)notify_cooldown)) {
                user->WriteNumeric(RPL_TARGNOTIFY, dest->nick,
                                   "has been informed that you messaged them.");
                dest->WriteRemoteNumeric(RPL_UMODEGMSG, user->nick, InspIRCd::Format("%s@%s",
                                         user->ident.c_str(), user->GetDisplayedHost().c_str()),
                                         InspIRCd::Format("is messaging you, and you have user mode +g set. Use /ACCEPT +%s to allow.",
                                                 user->nick.c_str()));
                dat->lastnotify = now;
            }
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        return HandleMessage(user, target);
    }

    ModResult OnUserPreTagMessage(User* user, const MessageTarget& target,
                                  CTCTags::TagMessageDetails& details) CXX11_OVERRIDE {
        return HandleMessage(user, target);
    }

    void OnUserPostNick(User* user, const std::string& oldnick) CXX11_OVERRIDE {
        if (!tracknick) {
            RemoveFromAllAccepts(user);
        }
    }

    void OnUserQuit(User* user, const std::string& message,
                    const std::string& oper_message) CXX11_OVERRIDE {
        RemoveFromAllAccepts(user);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("callerid");
        cmd.maxaccepts = tag->getUInt("maxaccepts", 30);
        tracknick = tag->getBool("tracknick");
        notify_cooldown = tag->getDuration("cooldown", 60);
    }

    void Prioritize() CXX11_OVERRIDE {
        // Want to be after modules like silence or services_account
        ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
    }
};

MODULE_INIT(ModuleCallerID)
