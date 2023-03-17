/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2013, 2017-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2014, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Matt Smith <dz@inspircd.org>
 *   Copyright (C) 2008 John Brooks <special@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 jamie
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

enum {
    // From ircd-ratbox.
    RPL_HELPSTART = 704,
    RPL_HELPTXT = 705,
    RPL_ENDOFHELP = 706,

    // InspIRCd-specific?
    RPL_DCCALLOWSTART = 990,
    RPL_DCCALLOWLIST = 991,
    RPL_DCCALLOWEND = 992,
    RPL_DCCALLOWTIMED = 993,
    RPL_DCCALLOWPERMANENT = 994,
    RPL_DCCALLOWREMOVED = 995,
    ERR_DCCALLOWINVALID = 996,
    RPL_DCCALLOWEXPIRED = 997,
    ERR_UNKNOWNDCCALLOWCMD = 998
};

static const char* const helptext[] = {
    "You may allow DCCs from specific users by specifying a",
    "DCC allow for the user you want to receive DCCs from.",
    "For example, to allow the user Brain to send you inspircd.exe",
    "you would type:",
    "/DCCALLOW +Brain",
    "Brain would then be able to send you files. They would have to",
    "resend the file again if the server gave them an error message",
    "before you added them to your DCCALLOW list.",
    "DCCALLOW entries will be temporary. If you want to add",
    "them to your DCCALLOW list until you leave IRC, type:",
    "/DCCALLOW +Brain 0",
    "To remove the user from your DCCALLOW list, type:",
    "/DCCALLOW -Brain",
    "To see the users in your DCCALLOW list, type:",
    "/DCCALLOW LIST",
    "NOTE: If the user leaves IRC or changes their nickname",
    "  they will be removed from your DCCALLOW list.",
    "  Your DCCALLOW list will be deleted when you leave IRC."
};

class BannedFileList {
  public:
    std::string filemask;
    std::string action;
};

class DCCAllow {
  public:
    std::string nickname;
    std::string hostmask;
    time_t set_on;
    unsigned long length;

    DCCAllow() { }

    DCCAllow(const std::string& nick, const std::string& hm, time_t so,
             unsigned long ln)
        : nickname(nick)
        , hostmask(hm)
        , set_on(so)
        , length(ln) {
    }
};

typedef std::vector<User *> userlist;
userlist ul;
typedef std::vector<DCCAllow> dccallowlist;
dccallowlist* dl;
typedef std::vector<BannedFileList> bannedfilelist;
bannedfilelist bfl;

class DCCAllowExt : public SimpleExtItem<dccallowlist> {
  public:
    unsigned int maxentries;

    DCCAllowExt(Module* Creator)
        : SimpleExtItem<dccallowlist>("dccallow", ExtensionItem::EXT_USER, Creator) {
    }

    void FromInternal(Extensible* container,
                      const std::string& value) CXX11_OVERRIDE {
        LocalUser* user = IS_LOCAL(static_cast<User*>(container));
        if (!user) {
            return;
        }

        // Remove the old list and create a new one.
        unset(user);
        dccallowlist* list = new dccallowlist();

        irc::spacesepstream ts(value);
        while (!ts.StreamEnd()) {
            // Check we have space for another entry.
            if (list->size() >= maxentries) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                          "Oversized DCC allow list received for %s: %s",
                                          user->uuid.c_str(), value.c_str());
                delete list;
                return;
            }

            // Extract the fields.
            DCCAllow dccallow;
            if (!ts.GetToken(dccallow.nickname) ||
                    !ts.GetToken(dccallow.hostmask) ||
                    !ts.GetNumericToken(dccallow.set_on) ||
                    !ts.GetNumericToken(dccallow.length)) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                          "Malformed DCC allow list received for %s: %s",
                                          user->uuid.c_str(), value.c_str());
                delete list;
                return;
            }

            // Store the DCC allow entry.
            list->push_back(dccallow);
        }

        // The value was well formed.
        set(user, list);
    }

    std::string ToInternal(const Extensible* container,
                           void* item) const CXX11_OVERRIDE {
        dccallowlist* list = static_cast<dccallowlist*>(item);
        std::string buf;
        for (dccallowlist::const_iterator iter = list->begin(); iter != list->end();
                ++iter) {
            if (iter != list->begin()) {
                buf.push_back(' ');
            }

            buf.append(iter->nickname);
            buf.push_back(' ');
            buf.append(iter->hostmask);
            buf.push_back(' ');
            buf.append(ConvToStr(iter->set_on));
            buf.push_back(' ');
            buf.append(ConvToStr(iter->length));
        }
        return buf;
    }
};

class CommandDccallow : public Command {
  public:
    DCCAllowExt& ext;
    unsigned long defaultlength;
    CommandDccallow(Module* parent, DCCAllowExt& Ext)
        : Command(parent, "DCCALLOW", 0)
        , ext(Ext) {
        syntax = "[(+|-)<nick> [<time>]]|[LIST|HELP]";
        /* XXX we need to fix this so it can work with translation stuff (i.e. move +- into a separate param */
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        /* syntax: DCCALLOW [(+|-)<nick> [<time>]]|[LIST|HELP] */
        if (!parameters.size()) {
            // display current DCCALLOW list
            DisplayDCCAllowList(user);
            return CMD_FAILURE;
        } else if (parameters.size() > 0) {
            char action = *parameters[0].c_str();

            // if they didn't specify an action, this is probably a command
            if (action != '+' && action != '-') {
                if (irc::equals(parameters[0], "LIST")) {
                    // list current DCCALLOW list
                    DisplayDCCAllowList(user);
                    return CMD_FAILURE;
                } else if (irc::equals(parameters[0], "HELP")) {
                    // display help
                    DisplayHelp(user);
                    return CMD_FAILURE;
                } else {
                    user->WriteNumeric(ERR_UNKNOWNDCCALLOWCMD,
                                       "DCCALLOW command not understood. For help on DCCALLOW, type /DCCALLOW HELP");
                    return CMD_FAILURE;
                }
            }

            std::string nick(parameters[0], 1);
            User *target = ServerInstance->FindNickOnly(nick);

            if ((target) && (!target->quitting) && (target->registered == REG_ALL)) {

                if (action == '-') {
                    // check if it contains any entries
                    dl = ext.get(user);
                    if (dl) {
                        for (dccallowlist::iterator i = dl->begin(); i != dl->end(); ++i) {
                            // search through list
                            if (i->nickname == target->nick) {
                                dl->erase(i);
                                user->WriteNumeric(RPL_DCCALLOWREMOVED, user->nick,
                                                   InspIRCd::Format("Removed %s from your DCCALLOW list", target->nick.c_str()));
                                break;
                            }
                        }
                    }
                } else if (action == '+') {
                    if (target == user) {
                        user->WriteNumeric(ERR_DCCALLOWINVALID, user->nick,
                                           "You cannot add yourself to your own DCCALLOW list!");
                        return CMD_FAILURE;
                    }

                    dl = ext.get(user);
                    if (!dl) {
                        dl = new dccallowlist;
                        ext.set(user, dl);
                        // add this user to the userlist
                        ul.push_back(user);
                    }

                    if (dl->size() >= ext.maxentries) {
                        user->WriteNumeric(ERR_DCCALLOWINVALID, user->nick,
                                           "Too many nicks on DCCALLOW list");
                        return CMD_FAILURE;
                    }

                    for (dccallowlist::const_iterator k = dl->begin(); k != dl->end(); ++k) {
                        if (k->nickname == target->nick) {
                            user->WriteNumeric(ERR_DCCALLOWINVALID, user->nick,
                                               InspIRCd::Format("%s is already on your DCCALLOW list", target->nick.c_str()));
                            return CMD_FAILURE;
                        }
                    }

                    std::string mask = target->nick+"!"+target->ident+"@"
                                       +target->GetDisplayedHost();
                    unsigned long length;
                    if (parameters.size() < 2) {
                        length = defaultlength;
                    } else if (!InspIRCd::IsValidDuration(parameters[1])) {
                        user->WriteNumeric(ERR_DCCALLOWINVALID, user->nick,
                                           InspIRCd::Format("%s is not a valid DCCALLOW duration", parameters[1].c_str()));
                        return CMD_FAILURE;
                    } else {
                        if (!InspIRCd::Duration(parameters[1], length)) {
                            user->WriteNotice("*** Invalid duration for DCC allow");
                            return CMD_FAILURE;
                        }
                    }

                    if (!InspIRCd::IsValidMask(mask)) {
                        return CMD_FAILURE;
                    }

                    dl->push_back(DCCAllow(target->nick, mask, ServerInstance->Time(), length));

                    if (length > 0) {
                        user->WriteNumeric(RPL_DCCALLOWTIMED, user->nick,
                                           InspIRCd::Format("Added %s to DCCALLOW list for %s", target->nick.c_str(),
                                                            InspIRCd::DurationString(length).c_str()));
                    } else {
                        user->WriteNumeric(RPL_DCCALLOWPERMANENT, user->nick,
                                           InspIRCd::Format("Added %s to DCCALLOW list for this session",
                                                            target->nick.c_str()));
                    }

                    /* route it. */
                    return CMD_SUCCESS;
                }
            } else {
                // nick doesn't exist
                user->WriteNumeric(Numerics::NoSuchNick(nick));
                return CMD_FAILURE;
            }
        }
        return CMD_FAILURE;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_BROADCAST;
    }

    void DisplayHelp(User* user) {
        user->WriteNumeric(RPL_HELPSTART, "*",
                           "DCCALLOW [(+|-)<nick> [<time>]]|[LIST|HELP]");
        for (size_t i = 0; i < sizeof(helptext)/sizeof(helptext[0]); i++) {
            user->WriteNumeric(RPL_HELPTXT, "*", helptext[i]);
        }
        user->WriteNumeric(RPL_ENDOFHELP, "*", "End of DCCALLOW HELP");

        LocalUser* localuser = IS_LOCAL(user);
        if (localuser) {
            localuser->CommandFloodPenalty += 4000;
        }
    }

    void DisplayDCCAllowList(User* user) {
        // display current DCCALLOW list
        user->WriteNumeric(RPL_DCCALLOWSTART, "Users on your DCCALLOW list:");

        dl = ext.get(user);
        if (dl) {
            for (dccallowlist::const_iterator c = dl->begin(); c != dl->end(); ++c) {
                user->WriteNumeric(RPL_DCCALLOWLIST, user->nick, InspIRCd::Format("%s (%s)",
                                   c->nickname.c_str(), c->hostmask.c_str()));
            }
        }

        user->WriteNumeric(RPL_DCCALLOWEND, "End of DCCALLOW list");
    }

};

class ModuleDCCAllow : public Module {
    DCCAllowExt ext;
    CommandDccallow cmd;
    bool blockchat;
    std::string defaultaction;

  public:
    ModuleDCCAllow()
        : ext(this)
        , cmd(this, ext)
        , blockchat(false) {
    }

    void OnUserQuit(User* user, const std::string &reason,
                    const std::string &oper_message) CXX11_OVERRIDE {
        dccallowlist* udl = ext.get(user);

        // remove their DCCALLOW list if they have one
        if (udl) {
            stdalgo::erase(ul, user);
        }

        // remove them from any DCCALLOW lists
        // they are currently on
        RemoveNick(user);
    }

    void OnUserPostNick(User* user, const std::string &oldnick) CXX11_OVERRIDE {
        RemoveNick(user);
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        if (!IS_LOCAL(user)) {
            return MOD_RES_PASSTHRU;
        }

        if (target.type == MessageTarget::TYPE_USER) {
            User* u = target.Get<User>();

            /* Always allow a user to dcc themselves (although... why?) */
            if (user == u) {
                return MOD_RES_PASSTHRU;
            }

            std::string ctcpname;
            std::string ctcpbody;
            if (details.IsCTCP(ctcpname, ctcpbody)) {
                Expire();

                // :jamie!jamie@test-D4457903BA652E0F.silverdream.org PRIVMSG eimaj :DCC SEND m_dnsbl.cpp 3232235786 52650 9676
                // :jamie!jamie@test-D4457903BA652E0F.silverdream.org PRIVMSG eimaj :VERSION

                if (irc::equals(ctcpname, "DCC") && !ctcpbody.empty()) {
                    dl = ext.get(u);
                    if (dl && dl->size()) {
                        for (dccallowlist::const_iterator iter = dl->begin(); iter != dl->end(); ++iter)
                            if (InspIRCd::Match(user->GetFullHost(), iter->hostmask)) {
                                return MOD_RES_PASSTHRU;
                            }
                    }

                    size_t s = ctcpbody.find(' ');
                    if (s == std::string::npos) {
                        return MOD_RES_PASSTHRU;
                    }

                    const std::string type = ctcpbody.substr(0, s);

                    if (irc::equals(type, "SEND")) {
                        size_t first;

                        std::string buf = ctcpbody.substr(s + 1);

                        if (!buf.empty() && buf[0] == '"') {
                            s = buf.find('"', 1);

                            if (s == std::string::npos || s <= 1) {
                                return MOD_RES_PASSTHRU;
                            }

                            --s;
                            first = 1;
                        } else {
                            s = buf.find(' ');
                            first = 0;
                        }

                        if (s == std::string::npos) {
                            return MOD_RES_PASSTHRU;
                        }

                        std::string filename = buf.substr(first, s);

                        bool found = false;
                        for (unsigned int i = 0; i < bfl.size(); i++) {
                            if (InspIRCd::Match(filename, bfl[i].filemask, ascii_case_insensitive_map)) {
                                /* We have a matching badfile entry, override whatever the default action is */
                                if (stdalgo::string::equalsci(bfl[i].action, "allow")) {
                                    return MOD_RES_PASSTHRU;
                                } else {
                                    found = true;
                                    break;
                                }
                            }
                        }

                        /* only follow the default action if no badfile matches were found above */
                        if ((!found) && (defaultaction == "allow")) {
                            return MOD_RES_PASSTHRU;
                        }

                        user->WriteNotice("The user " + u->nick +
                                          " is not accepting DCC SENDs from you. Your file " + filename +
                                          " was not sent.");
                        u->WriteNotice(user->nick + " (" + user->ident + "@" + user->GetDisplayedHost()
                                       + ") attempted to send you a file named " + filename + ", which was blocked.");
                        u->WriteNotice("If you trust " + user->nick +
                                       " and were expecting this, you can type /DCCALLOW HELP for information on the DCCALLOW system.");
                        return MOD_RES_DENY;
                    } else if (blockchat && irc::equals(type, "CHAT")) {
                        user->WriteNotice("The user " + u->nick +
                                          " is not accepting DCC CHAT requests from you.");
                        u->WriteNotice(user->nick + " (" + user->ident + "@" + user->GetDisplayedHost()
                                       + ") attempted to initiate a DCC CHAT session, which was blocked.");
                        u->WriteNotice("If you trust " + user->nick +
                                       " and were expecting this, you can type /DCCALLOW HELP for information on the DCCALLOW system.");
                        return MOD_RES_DENY;
                    }
                }
            }
        }
        return MOD_RES_PASSTHRU;
    }

    void Expire() {
        for (userlist::iterator iter = ul.begin(); iter != ul.end();) {
            User* u = (User*)(*iter);
            dl = ext.get(u);
            if (dl) {
                if (dl->size()) {
                    dccallowlist::iterator iter2 = dl->begin();
                    while (iter2 != dl->end()) {
                        time_t expires = iter2->set_on + iter2->length;
                        if (iter2->length != 0 && expires <= ServerInstance->Time()) {
                            u->WriteNumeric(RPL_DCCALLOWEXPIRED, u->nick,
                                            InspIRCd::Format("DCCALLOW entry for %s has expired", iter2->nickname.c_str()));
                            iter2 = dl->erase(iter2);
                        } else {
                            ++iter2;
                        }
                    }
                }
                ++iter;
            } else {
                iter = ul.erase(iter);
            }
        }
    }

    void RemoveNick(User* user) {
        /* Iterate through all DCCALLOW lists and remove user */
        for (userlist::iterator iter = ul.begin(); iter != ul.end();) {
            User *u = (User*)(*iter);
            dl = ext.get(u);
            if (dl) {
                if (dl->size()) {
                    for (dccallowlist::iterator i = dl->begin(); i != dl->end(); ++i) {
                        if (i->nickname == user->nick) {

                            u->WriteNotice(i->nickname +
                                           " left the network or changed their nickname and has been removed from your DCCALLOW list");
                            u->WriteNumeric(RPL_DCCALLOWREMOVED, u->nick,
                                            InspIRCd::Format("Removed %s from your DCCALLOW list", i->nickname.c_str()));
                            dl->erase(i);
                            break;
                        }
                    }
                }
                ++iter;
            } else {
                iter = ul.erase(iter);
            }
        }
    }

    void RemoveFromUserlist(User *user) {
        // remove user from userlist
        for (userlist::iterator j = ul.begin(); j != ul.end(); ++j) {
            User* u = (User*)(*j);
            if (u == user) {
                ul.erase(j);
                break;
            }
        }
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        bannedfilelist newbfl;
        ConfigTagList tags = ServerInstance->Config->ConfTags("banfile");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            BannedFileList bf;
            bf.filemask = i->second->getString("pattern");
            bf.action = i->second->getString("action");
            newbfl.push_back(bf);
        }
        bfl.swap(newbfl);

        ConfigTag* tag = ServerInstance->Config->ConfValue("dccallow");
        cmd.ext.maxentries = tag->getUInt("maxentries", 20, 1);
        cmd.defaultlength = tag->getDuration("length", 0);
        blockchat = tag->getBool("blockchat");
        defaultaction = tag->getString("action");
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server administrator to configure what files are allowed to be sent via DCC SEND and allows users to configure who can send them DCC CHAT and DCC SEND requests.", VF_COMMON | VF_VENDOR);
    }
};

MODULE_INIT(ModuleDCCAllow)
