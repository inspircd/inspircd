/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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
#include "main.h"
#include "treesocket.h"
#include "treeserver.h"

static std::string newline("\n");

void TreeSocket::WriteLineNoCompat(const std::string& line) {
    ServerInstance->Logs->Log(MODNAME, LOG_RAWIO, "S[%d] O %s", this->GetFd(),
                              line.c_str());
    this->WriteData(line);
    this->WriteData(newline);
}

void TreeSocket::WriteLine(const std::string& original_line) {
    if (LinkState == CONNECTED) {
        if (proto_version != PROTO_NEWEST) {
            std::string line = original_line;
            std::string::size_type a = line.find(' ');
            if (line[0] == '@') {
                // The line contains tags which the 1202 protocol can't handle.
                line.erase(0, a + 1);
                a = line.find(' ');
            }
            std::string::size_type b = line.find(' ', a + 1);
            std::string command(line, a + 1, b-a-1);
            // now try to find a translation entry
            if (proto_version < PROTO_INSPIRCD_30) {
                if (command == "IJOIN") {
                    // Convert
                    // :<uid> IJOIN <chan> <membid> [<ts> [<flags>]]
                    // to
                    // :<sid> FJOIN <chan> <ts> + [<flags>],<uuid>
                    std::string::size_type c = line.find(' ', b + 1);
                    if (c == std::string::npos) {
                        return;
                    }

                    std::string::size_type d = line.find(' ', c + 1);
                    // Erase membership id first
                    line.erase(c, d-c);
                    if (d == std::string::npos) {
                        // No TS or modes in the command
                        // :22DAAAAAB IJOIN #chan
                        const std::string channame(line, b+1, c-b-1);
                        Channel* chan = ServerInstance->FindChan(channame);
                        if (!chan) {
                            return;
                        }

                        line.push_back(' ');
                        line.append(ConvToStr(chan->age));
                        line.append(" + ,");
                    } else {
                        d = line.find(' ', c + 1);
                        if (d == std::string::npos) {
                            // TS present, no modes
                            // :22DAAAAAC IJOIN #chan 12345
                            line.append(" + ,");
                        } else {
                            // Both TS and modes are present
                            // :22DAAAAAC IJOIN #chan 12345 ov
                            std::string::size_type e = line.find(' ', d + 1);
                            if (e != std::string::npos) {
                                line.erase(e);
                            }

                            line.insert(d, " +");
                            line.push_back(',');
                        }
                    }

                    // Move the uuid to the end and replace the I with an F
                    line.append(line.substr(1, 9));
                    line.erase(4, 6);
                    line[5] = 'F';
                } else if (command == "RESYNC") {
                    return;
                } else if (command == "METADATA") {
                    // Drop TS for channel METADATA, translate METADATA operquit into an OPERQUIT command
                    // :sid METADATA #target TS extname ...
                    //     A        B       C  D
                    if (b == std::string::npos) {
                        return;
                    }

                    std::string::size_type c = line.find(' ', b + 1);
                    if (c == std::string::npos) {
                        return;
                    }

                    std::string::size_type d = line.find(' ', c + 1);
                    if (d == std::string::npos) {
                        return;
                    }

                    if (line[b + 1] == '#') {
                        // We're sending channel metadata
                        line.erase(c, d-c);
                    } else if (!line.compare(c, d-c, " operquit", 9)) {
                        // ":22D METADATA 22DAAAAAX operquit :message" -> ":22DAAAAAX OPERQUIT :message"
                        line = ":" + line.substr(b+1, c-b) + "OPERQUIT" + line.substr(d);
                    }
                } else if (command == "FTOPIC") {
                    // Drop channel TS for FTOPIC
                    // :sid FTOPIC #target TS TopicTS setter :newtopic
                    //     A      B       C  D       E      F
                    // :uid FTOPIC #target TS TopicTS :newtopic
                    //     A      B       C  D       E
                    if (b == std::string::npos) {
                        return;
                    }

                    std::string::size_type c = line.find(' ', b + 1);
                    if (c == std::string::npos) {
                        return;
                    }

                    std::string::size_type d = line.find(' ', c + 1);
                    if (d == std::string::npos) {
                        return;
                    }

                    std::string::size_type e = line.find(' ', d + 1);
                    if (line[e+1] == ':') {
                        line.erase(c, e-c);
                        line.erase(a+1, 1);
                    } else {
                        line.erase(c, d-c);
                    }
                } else if ((command == "PING") || (command == "PONG")) {
                    // :22D PING 20D
                    if (line.length() < 13) {
                        return;
                    }

                    // Insert the source SID (and a space) between the command and the first parameter
                    line.insert(10, line.substr(1, 4));
                } else if (command == "OPERTYPE") {
                    std::string::size_type colon = line.find(':', b);
                    if (colon != std::string::npos) {
                        for (std::string::iterator i = line.begin()+colon; i != line.end(); ++i) {
                            if (*i == ' ') {
                                *i = '_';
                            }
                        }
                        line.erase(colon, 1);
                    }
                } else if (command == "INVITE") {
                    // :22D INVITE 22DAAAAAN #chan TS ExpirationTime
                    //     A      B         C     D  E
                    if (b == std::string::npos) {
                        return;
                    }

                    std::string::size_type c = line.find(' ', b + 1);
                    if (c == std::string::npos) {
                        return;
                    }

                    std::string::size_type d = line.find(' ', c + 1);
                    if (d == std::string::npos) {
                        return;
                    }

                    std::string::size_type e = line.find(' ', d + 1);
                    // If there is no expiration time then everything will be erased from 'd'
                    line.erase(d, e-d);
                } else if (command == "FJOIN") {
                    // Strip membership ids
                    // :22D FJOIN #chan 1234 +f 4:3 :o,22DAAAAAB:15 o,22DAAAAAA:15
                    // :22D FJOIN #chan 1234 +f 4:3 o,22DAAAAAB:15
                    // :22D FJOIN #chan 1234 +Pf 4:3 :

                    // If the last parameter is prefixed by a colon then it's a userlist which may have 0 or more users;
                    // if it isn't, then it is a single member
                    std::string::size_type spcolon = line.find(" :");
                    if (spcolon != std::string::npos) {
                        spcolon++;
                        // Loop while there is a ':' in the userlist, this is never true if the channel is empty
                        std::string::size_type pos = std::string::npos;
                        while ((pos = line.rfind(':', pos-1)) > spcolon) {
                            // Find the next space after the ':'
                            std::string::size_type sp = line.find(' ', pos);
                            // Erase characters between the ':' and the next space after it, including the ':' but not the space;
                            // if there is no next space, everything will be erased between pos and the end of the line
                            line.erase(pos, sp-pos);
                        }
                    } else {
                        // Last parameter is a single member
                        std::string::size_type sp = line.rfind(' ');
                        std::string::size_type colon = line.find(':', sp);
                        line.erase(colon);
                    }
                } else if (command == "KICK") {
                    // Strip membership id if the KICK has one
                    if (b == std::string::npos) {
                        return;
                    }

                    std::string::size_type c = line.find(' ', b + 1);
                    if (c == std::string::npos) {
                        return;
                    }

                    std::string::size_type d = line.find(' ', c + 1);
                    if ((d < line.size()-1) && (original_line[d+1] != ':')) {
                        // There is a third parameter which doesn't begin with a colon, erase it
                        std::string::size_type e = line.find(' ', d + 1);
                        line.erase(d, e-d);
                    }
                } else if (command == "SINFO") {
                    // :22D SINFO version :InspIRCd-3.0
                    //     A     B       C
                    std::string::size_type c = line.find(' ', b + 1);
                    if (c == std::string::npos) {
                        return;
                    }

                    // Only translating SINFO version, discard everything else
                    if (line.compare(b, 9, " version ", 9)) {
                        return;
                    }

                    line = line.substr(0, 5) + "VERSION" + line.substr(c);
                } else if (command == "SERVER") {
                    // :001 SERVER inspircd.test 002 [<anything> ...] :description
                    //     A      B             C
                    std::string::size_type c = line.find(' ', b + 1);
                    if (c == std::string::npos) {
                        return;
                    }

                    std::string::size_type d = c + 4;
                    std::string::size_type spcolon = line.find(" :", d);
                    if (spcolon == std::string::npos) {
                        return;
                    }

                    line.erase(d, spcolon-d);
                    line.insert(c, " * 0");

                    if (burstsent) {
                        WriteLineNoCompat(line);

                        // Synthesize a :<newserver> BURST <time> message
                        spcolon = line.find(" :");

                        TreeServer* const source = Utils->FindServerID(line.substr(spcolon-3, 3));
                        if (!source) {
                            return;
                        }

                        line = CmdBuilder(source, "BURST").push_int(ServerInstance->Time()).str();
                    }
                } else if (command == "NUM") {
                    // :<sid> NUM <numeric source sid> <target uuid> <3 digit number> <params>
                    // Translate to
                    // :<sid> PUSH <target uuid> :<numeric source name> <3 digit number> <target nick> <params>

                    TreeServer* const numericsource = Utils->FindServerID(line.substr(9, 3));
                    if (!numericsource) {
                        return;
                    }

                    // The nick of the target is necessary for building the PUSH message
                    User* const target = ServerInstance->FindUUID(line.substr(13,
                                         UIDGenerator::UUID_LENGTH));
                    if (!target) {
                        return;
                    }

                    std::string push = InspIRCd::Format(":%.*s PUSH %s ::%s %.*s %s", 3,
                                                        line.c_str()+1, target->uuid.c_str(), numericsource->GetName().c_str(), 3,
                                                        line.c_str()+23, target->nick.c_str());
                    push.append(line, 26, std::string::npos);
                    push.swap(line);
                } else if (command == "TAGMSG") {
                    // Drop IRCv3 tag messages as v2 has no message tag support.
                    return;
                }
            }
            WriteLineNoCompat(line);
            return;
        }
    }

    WriteLineNoCompat(original_line);
}

namespace {
bool InsertCurrentChannelTS(CommandBase::Params& params,
                            unsigned int chanindex = 0, unsigned int pos = 1) {
    Channel* chan = ServerInstance->FindChan(params[chanindex]);
    if (!chan) {
        return false;
    }

    // Insert the current TS of the channel after the pos-th parameter
    params.insert(params.begin()+pos, ConvToStr(chan->age));
    return true;
}
}

bool TreeSocket::PreProcessOldProtocolMessage(User*& who, std::string& cmd,
        CommandBase::Params& params) {
    if ((cmd == "METADATA") && (params.size() >= 3) && (params[0][0] == '#')) {
        // :20D METADATA #channel extname :extdata
        return InsertCurrentChannelTS(params);
    } else if ((cmd == "FTOPIC") && (params.size() >= 4)) {
        // :20D FTOPIC #channel 100 Attila :topic text
        return InsertCurrentChannelTS(params);
    } else if ((cmd == "PING") || (cmd == "PONG")) {
        if (params.size() == 1) {
            // If it's a PING with 1 parameter, reply with a PONG now, if it's a PONG with 1 parameter (weird), do nothing
            if (cmd[1] == 'I') {
                this->WriteData(":" + ServerInstance->Config->GetSID() + " PONG " + params[0] +
                                newline);
            }

            // Don't process this message further
            return false;
        }

        // :20D PING 20D 22D
        // :20D PONG 20D 22D
        // Drop the first parameter
        params.erase(params.begin());

        // If the target is a server name, translate it to a SID
        if (!InspIRCd::IsSID(params[0])) {
            TreeServer* server = Utils->FindServer(params[0]);
            if (!server) {
                // We've no idea what this is, log and stop processing
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "Received a " + cmd + " with an unknown target: \"" + params[0] +
                                          "\", command dropped");
                return false;
            }

            params[0] = server->GetId();
        }
    } else if ((cmd == "GLINE") || (cmd == "KLINE") || (cmd == "ELINE")
               || (cmd == "ZLINE") || (cmd == "QLINE")) {
        // Fix undocumented protocol usage: translate GLINE, ZLINE, etc. into ADDLINE or DELLINE
        if ((params.size() != 1) && (params.size() != 3)) {
            return false;
        }

        CommandBase::Params p;
        p.push_back(cmd.substr(0, 1));
        p.push_back(params[0]);

        if (params.size() == 3) {
            cmd = "ADDLINE";
            p.push_back(who->nick);
            p.push_back(ConvToStr(ServerInstance->Time()));
            p.push_back(ConvToStr(InspIRCd::Duration(params[1])));
            p.push_back(params[2]);
        } else {
            cmd = "DELLINE";
        }

        params.swap(p);
    } else if (cmd == "SVSMODE") {
        cmd = "MODE";
    } else if (cmd == "OPERQUIT") {
        // Translate OPERQUIT into METADATA
        if (params.empty()) {
            return false;
        }

        cmd = "METADATA";
        params.insert(params.begin(), who->uuid);
        params.insert(params.begin()+1, "operquit");
        who = MyRoot->ServerUser;
    } else if ((cmd == "TOPIC") && (params.size() >= 2)) {
        // :20DAAAAAC TOPIC #chan :new topic
        cmd = "FTOPIC";
        if (!InsertCurrentChannelTS(params)) {
            return false;
        }

        params.insert(params.begin()+2, ConvToStr(ServerInstance->Time()));
    } else if (cmd == "MODENOTICE") {
        // MODENOTICE is always supported by 2.0 but it's optional in 3.0.
        params.insert(params.begin(), "*");
        params.insert(params.begin()+1, cmd);
        cmd = "ENCAP";
    } else if (cmd == "RULES") {
        return false;
    } else if (cmd == "INVITE") {
        // :20D INVITE 22DAAABBB #chan
        // :20D INVITE 22DAAABBB #chan 123456789
        // Insert channel timestamp after the channel name; the 3rd parameter, if there, is the invite expiration time
        return InsertCurrentChannelTS(params, 1, 2);
    } else if (cmd == "VERSION") {
        // :20D VERSION :InspIRCd-2.0
        // change to
        // :20D SINFO version :InspIRCd-2.0
        cmd = "SINFO";
        params.insert(params.begin(), "version");
    } else if (cmd == "JOIN") {
        // 2.0 allows and forwards legacy JOINs but we don't, so translate them to FJOINs before processing
        if ((params.size() != 1) || (IS_SERVER(who))) {
            return false;    // Huh?
        }

        cmd = "FJOIN";
        Channel* chan = ServerInstance->FindChan(params[0]);
        params.push_back(ConvToStr(chan ? chan->age : ServerInstance->Time()));
        params.push_back("+");
        params.push_back(",");
        params.back().append(who->uuid);
        who = TreeServer::Get(who)->ServerUser;
    } else if ((cmd == "FMODE") && (params.size() >= 2)) {
        // Translate user mode changes with timestamp to MODE
        if (params[0][0] != '#') {
            User* user = ServerInstance->FindUUID(params[0]);
            if (!user) {
                return false;
            }

            // Emulate the old nonsensical behavior
            if (user->age < ServerCommand::ExtractTS(params[1])) {
                return false;
            }

            cmd = "MODE";
            params.erase(params.begin()+1);
        }
    } else if ((cmd == "SERVER") && (params.size() > 4)) {
        // This does not affect the initial SERVER line as it is sent before the link state is CONNECTED
        // :20D SERVER <name> * 0 <sid> <desc>
        // change to
        // :20D SERVER <name> <sid> <desc>

        params[1].swap(params[3]);
        params.erase(params.begin()+2, params.begin()+4);

        // If the source of this SERVER message or any of its parents are bursting, then new servers it
        // introduces are not bursting.
        bool bursting = false;
        for (TreeServer* server = TreeServer::Get(who); server;
                server = server->GetParent()) {
            if (server->IsBursting()) {
                bursting = true;
                break;
            }
        }

        if (!bursting) {
            params.insert(params.begin()+2,
                          "burst=" + ConvToStr(((uint64_t)ServerInstance->Time())*1000));
        }
    } else if (cmd == "BURST") {
        // A server is introducing another one, drop unnecessary BURST
        return false;
    } else if (cmd == "SVSWATCH") {
        // SVSWATCH was removed because nothing was using it, but better be sure
        return false;
    } else if (cmd == "SVSSILENCE") {
        // SVSSILENCE was removed because nothing was using it, but better be sure
        return false;
    } else if (cmd == "PUSH") {
        if ((params.size() != 2) || (!this->MyRoot)) {
            return false;    // Huh?
        }

        irc::tokenstream ts(params.back());

        std::string srcstr;
        ts.GetMiddle(srcstr);
        srcstr.erase(0, 1);

        std::string token;
        ts.GetMiddle(token);

        // See if it's a numeric being sent to the target via PUSH
        unsigned int numeric_number = 0;
        if (token.length() == 3) {
            numeric_number = ConvToNum<unsigned int>(token);
        }

        if ((numeric_number > 0) && (numeric_number < 1000)) {
            // It's a numeric, translate to NUM

            // srcstr must be a valid server name
            TreeServer* const numericsource = Utils->FindServer(srcstr);
            if (!numericsource) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                          "Unable to translate PUSH numeric %s to user %s from 1202 protocol server %s: source \"%s\" doesn't exist",
                                          token.c_str(), params[0].c_str(), this->MyRoot->GetName().c_str(),
                                          srcstr.c_str());
                return false;
            }

            cmd = "NUM";

            // Second parameter becomes the target uuid
            params[0].swap(params[1]);
            // Replace first param (now the PUSH payload, not needed) with the source sid
            params[0] = numericsource->GetId();

            params.push_back(InspIRCd::Format("%03u", numeric_number));

            // Ignore the nickname in the numeric in PUSH
            ts.GetMiddle(token);

            // Rest of the tokens are the numeric parameters, add them to NUM
            while (ts.GetTrailing(token)) {
                params.push_back(token);
            }
        } else if ((token == "PRIVMSG") || (token == "NOTICE")) {
            // Command is a PRIVMSG/NOTICE
            cmd.swap(token);

            // Check if the PRIVMSG/NOTICE target is a nickname
            ts.GetMiddle(token);
            if (token.c_str()[0] == '#') {
                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                          "Unable to translate PUSH %s to user %s from 1202 protocol server %s, target \"%s\"",
                                          cmd.c_str(), params[0].c_str(), this->MyRoot->GetName().c_str(), token.c_str());
                return false;
            }

            // Replace second parameter with the message
            ts.GetTrailing(params[1]);
        } else {
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                      "Unable to translate PUSH to user %s from 1202 protocol server %s",
                                      params[0].c_str(), this->MyRoot->GetName().c_str());
            return false;
        }

        return true;
    }

    return true; // Passthru
}
