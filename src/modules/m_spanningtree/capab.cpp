/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014, 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

#include "treeserver.h"
#include "utils.h"
#include "link.h"
#include "main.h"

struct CompatMod {
    const char* name;
    ModuleFlags listflag;
};

static CompatMod compatmods[] = {
    { "m_watch.so", VF_OPTCOMMON }
};

std::string TreeSocket::MyModules(int filter) {
    const ModuleManager::ModuleMap& modlist = ServerInstance->Modules->GetModules();

    std::string capabilities;
    for (ModuleManager::ModuleMap::const_iterator i = modlist.begin();
            i != modlist.end(); ++i) {
        Module* const mod = i->second;
        // 3.0 advertises its settings for the benefit of services
        // 2.0 would bork on this
        if (proto_version < PROTO_INSPIRCD_30
                && mod->ModuleSourceFile == "m_kicknorejoin.so") {
            continue;
        }

        bool do_compat_include = false;
        if (proto_version < PROTO_INSPIRCD_30) {
            for (size_t j = 0; j < sizeof(compatmods)/sizeof(compatmods[0]); j++) {
                if ((compatmods[j].listflag & filter)
                        && (mod->ModuleSourceFile == compatmods[j].name)) {
                    do_compat_include = true;
                    break;
                }
            }
        }

        Version v = mod->GetVersion();
        if ((!do_compat_include) && (!(v.Flags & filter))) {
            continue;
        }

        capabilities.push_back(' ');
        capabilities.append(i->first);
        if (!v.link_data.empty()) {
            capabilities.push_back('=');
            capabilities.append(v.link_data);
        }
    }

    // If we are linked in a 2.0 server and have an ascii casemapping
    // advertise it as m_ascii.so from inspircd-extras
    if ((filter & VF_COMMON) && ServerInstance->Config->CaseMapping == "ascii"
            && proto_version == PROTO_INSPIRCD_20) {
        capabilities.append(" m_ascii.so");
    }

    if (capabilities.empty()) {
        return capabilities;
    }

    return capabilities.substr(1);
}

std::string TreeSocket::BuildModeList(ModeType mtype) {
    std::vector<std::string> modes;
    const ModeParser::ModeHandlerMap& mhs = ServerInstance->Modes.GetModes(mtype);
    for (ModeParser::ModeHandlerMap::const_iterator i = mhs.begin(); i != mhs.end();
            ++i) {
        const ModeHandler* const mh = i->second;
        const PrefixMode* const pm = mh->IsPrefixMode();
        std::string mdesc;
        if (proto_version >= PROTO_INSPIRCD_30) {
            if (pm) {
                mdesc.append("prefix:").append(ConvToStr(pm->GetPrefixRank())).push_back(':');
            } else if (mh->IsListMode()) {
                mdesc.append("list:");
            } else if (mh->NeedsParam(true)) {
                mdesc.append(mh->NeedsParam(false) ? "param:" : "param-set:");
            } else {
                mdesc.append("simple:");
            }
        }
        mdesc.append(mh->name);
        mdesc.push_back('=');
        if (pm) {
            if (pm->GetPrefix()) {
                mdesc.push_back(pm->GetPrefix());
            }
        }
        mdesc.push_back(mh->GetModeChar());
        modes.push_back(mdesc);
    }
    std::sort(modes.begin(), modes.end());
    return stdalgo::string::join(modes);
}

void TreeSocket::SendCapabilities(int phase) {
    if (capab->capab_phase >= phase) {
        return;
    }

    if (capab->capab_phase < 1 && phase >= 1) {
        WriteLine("CAPAB START " + ConvToStr(PROTO_NEWEST));
    }

    capab->capab_phase = phase;
    if (phase < 2) {
        return;
    }

    const char sep = ' ';
    irc::sepstream modulelist(MyModules(VF_COMMON), sep);
    irc::sepstream optmodulelist(MyModules(VF_OPTCOMMON), sep);
    /* Send module names, split at 509 length */
    std::string item;
    std::string line = "CAPAB MODULES :";
    while (modulelist.GetToken(item)) {
        if (line.length() + item.length() + 1 > 509) {
            this->WriteLine(line);
            line = "CAPAB MODULES :";
        }

        if (line != "CAPAB MODULES :") {
            line.push_back(sep);
        }

        line.append(item);
    }
    if (line != "CAPAB MODULES :") {
        this->WriteLine(line);
    }

    line = "CAPAB MODSUPPORT :";
    while (optmodulelist.GetToken(item)) {
        if (line.length() + item.length() + 1 > 509) {
            this->WriteLine(line);
            line = "CAPAB MODSUPPORT :";
        }

        if (line != "CAPAB MODSUPPORT :") {
            line.push_back(sep);
        }

        line.append(item);
    }
    if (line != "CAPAB MODSUPPORT :") {
        this->WriteLine(line);
    }

    WriteLine("CAPAB CHANMODES :" + BuildModeList(MODETYPE_CHANNEL));
    WriteLine("CAPAB USERMODES :" + BuildModeList(MODETYPE_USER));

    std::string extra;
    /* Do we have sha256 available? If so, we send a challenge */
    if (ServerInstance->Modules->FindService(SERVICE_DATA, "hash/sha256")) {
        SetOurChallenge(ServerInstance->GenRandomStr(20));
        extra = " CHALLENGE=" + this->GetOurChallenge();
    }

    // 2.0 needs these keys.
    if (proto_version == PROTO_INSPIRCD_20) {
        extra.append(" PROTOCOL="+ConvToStr(proto_version))
        .append(" MAXGECOS="+ConvToStr(ServerInstance->Config->Limits.MaxReal))
        .append(" CHANMODES="+ServerInstance->Modes->GiveModeList(MODETYPE_CHANNEL))
        .append(" USERMODES="+ServerInstance->Modes->GiveModeList(MODETYPE_USER))
        .append(" PREFIX="+ ServerInstance->Modes->BuildPrefixes());
    }

    // HACK: Allow services to know what extbans exist. This will be
    // replaced by CAPAB EXTBANS in the next protocol version.
    std::map<std::string, std::string> tokens;
    FOREACH_MOD(On005Numeric, (tokens));
    std::map<std::string, std::string>::const_iterator eiter =
        tokens.find("EXTBAN");
    if (eiter != tokens.end()) {
        extra.append(" EXTBANS=" + eiter->second);
    }

    this->WriteLine("CAPAB CAPABILITIES " /* Preprocessor does this one. */
                    ":NICKMAX="+ConvToStr(ServerInstance->Config->Limits.NickMax)+
                    " CHANMAX="+ConvToStr(ServerInstance->Config->Limits.ChanMax)+
                    " MAXMODES="+ConvToStr(ServerInstance->Config->Limits.MaxModes)+
                    " IDENTMAX="+ConvToStr(ServerInstance->Config->Limits.IdentMax)+
                    " MAXQUIT="+ConvToStr(ServerInstance->Config->Limits.MaxQuit)+
                    " MAXTOPIC="+ConvToStr(ServerInstance->Config->Limits.MaxTopic)+
                    " MAXKICK="+ConvToStr(ServerInstance->Config->Limits.MaxKick)+
                    " MAXREAL="+ConvToStr(ServerInstance->Config->Limits.MaxReal)+
                    " MAXAWAY="+ConvToStr(ServerInstance->Config->Limits.MaxAway)+
                    " MAXHOST="+ConvToStr(ServerInstance->Config->Limits.MaxHost)+
                    " MAXLINE="+ConvToStr(ServerInstance->Config->Limits.MaxLine)+
                    extra+
                    " CASEMAPPING="+ServerInstance->Config->CaseMapping+
                    // XXX: Advertise the presence or absence of m_globops in CAPAB CAPABILITIES.
                    // Services want to know about it, and since m_globops was not marked as VF_(OPT)COMMON
                    // in 2.0, we advertise it here to not break linking to previous versions.
                    // Protocol version 1201 (1.2) does not have this issue because we advertise m_globops
                    // to 1201 protocol servers irrespectively of its module flags.
                    (ServerInstance->Modules->Find("m_globops.so") != NULL ? " GLOBOPS=1" :
                     " GLOBOPS=0")
                   );

    this->WriteLine("CAPAB END");
}

/* Isolate and return the elements that are different between two comma separated lists */
void TreeSocket::ListDifference(const std::string &one, const std::string &two,
                                char sep,
                                std::string& mleft, std::string& mright) {
    std::set<std::string> values;
    irc::sepstream sepleft(one, sep);
    irc::sepstream sepright(two, sep);
    std::string item;
    while (sepleft.GetToken(item)) {
        values.insert(item);
    }
    while (sepright.GetToken(item)) {
        if (!values.erase(item)) {
            mright.push_back(sep);
            mright.append(item);
        }
    }
    for(std::set<std::string>::iterator i = values.begin(); i != values.end();
            ++i) {
        mleft.push_back(sep);
        mleft.append(*i);
    }
}

bool TreeSocket::Capab(const CommandBase::Params& params) {
    if (params.size() < 1) {
        this->SendError("Invalid number of parameters for CAPAB - Mismatched version");
        return false;
    }
    if (params[0] == "START") {
        capab->ModuleList.clear();
        capab->OptModuleList.clear();
        capab->CapKeys.clear();
        if (params.size() > 1) {
            proto_version = ConvToNum<unsigned int>(params[1]);
        }

        if (proto_version < PROTO_OLDEST) {
            SendError("CAPAB negotiation failed: Server is using protocol version "
                      + (proto_version ? ConvToStr(proto_version) : "1201 or older")
                      + " which is too old to link with this server (protocol versions "
                      + ConvToStr(PROTO_OLDEST) + " to " + ConvToStr(PROTO_NEWEST) +
                      " are supported)");
            return false;
        }

        // We don't support the 2.1 protocol.
        if (proto_version == PROTO_INSPIRCD_21_A0
                || proto_version == PROTO_INSPIRCD_21_B2) {
            SendError("CAPAB negotiation failed: InspIRCd 2.1 beta is not supported");
            return false;
        }

        SendCapabilities(2);
    } else if (params[0] == "END") {
        std::string reason;
        /* Compare ModuleList and check CapKeys */
        if ((this->capab->ModuleList != this->MyModules(VF_COMMON))
                && (this->capab->ModuleList.length())) {
            std::string diffIneed, diffUneed;
            ListDifference(this->capab->ModuleList, this->MyModules(VF_COMMON), ' ',
                           diffIneed, diffUneed);
            if (diffIneed.length() || diffUneed.length()) {
                reason = "Modules incorrectly matched on these servers.";
                if (diffIneed.length()) {
                    reason += " Not loaded here:" + diffIneed;
                }
                if (diffUneed.length()) {
                    reason += " Not loaded there:" + diffUneed;
                }
                this->SendError("CAPAB negotiation failed: "+reason);
                return false;
            }
        }

        if (this->capab->OptModuleList != this->MyModules(VF_OPTCOMMON)
                && this->capab->OptModuleList.length()) {
            std::string diffIneed, diffUneed;
            ListDifference(this->capab->OptModuleList, this->MyModules(VF_OPTCOMMON), ' ',
                           diffIneed, diffUneed);
            if (diffIneed.length() || diffUneed.length()) {
                if (Utils->AllowOptCommon) {
                    ServerInstance->SNO->WriteToSnoMask('l',
                                                        "Optional module lists do not match, some commands may not work globally.%s%s%s%s",
                                                        diffIneed.length() ? " Not loaded here:" : "", diffIneed.c_str(),
                                                        diffUneed.length() ? " Not loaded there:" : "", diffUneed.c_str());
                } else {
                    reason = "Optional modules incorrectly matched on these servers and <options:allowmismatch> is not enabled.";
                    if (diffIneed.length()) {
                        reason += " Not loaded here:" + diffIneed;
                    }
                    if (diffUneed.length()) {
                        reason += " Not loaded there:" + diffUneed;
                    }
                    this->SendError("CAPAB negotiation failed: "+reason);
                    return false;
                }
            }
        }

        if (!capab->ChanModes.empty()) {
            if (capab->ChanModes != BuildModeList(MODETYPE_CHANNEL)) {
                std::string diffIneed, diffUneed;
                ListDifference(capab->ChanModes, BuildModeList(MODETYPE_CHANNEL), ' ',
                               diffIneed, diffUneed);
                if (diffIneed.length() || diffUneed.length()) {
                    reason = "Channel modes not matched on these servers.";
                    if (diffIneed.length()) {
                        reason += " Not loaded here:" + diffIneed;
                    }
                    if (diffUneed.length()) {
                        reason += " Not loaded there:" + diffUneed;
                    }
                }
            }
        } else if (proto_version == PROTO_INSPIRCD_20) {
            if (this->capab->CapKeys.find("CHANMODES") != this->capab->CapKeys.end()) {
                if (this->capab->CapKeys.find("CHANMODES")->second !=
                        ServerInstance->Modes->GiveModeList(MODETYPE_CHANNEL)) {
                    reason = "One or more of the channel modes on the remote server are invalid on this server.";
                }
            }

            else if (this->capab->CapKeys.find("PREFIX") != this->capab->CapKeys.end()) {
                if (this->capab->CapKeys.find("PREFIX")->second !=
                        ServerInstance->Modes->BuildPrefixes()) {
                    reason = "One or more of the prefixes on the remote server are invalid on this server.";
                }
            }
        }

        if (!reason.empty()) {
            this->SendError("CAPAB negotiation failed: " + reason);
            return false;
        }

        if (!capab->UserModes.empty()) {
            if (capab->UserModes != BuildModeList(MODETYPE_USER)) {
                std::string diffIneed, diffUneed;
                ListDifference(capab->UserModes, BuildModeList(MODETYPE_USER), ' ', diffIneed,
                               diffUneed);
                if (diffIneed.length() || diffUneed.length()) {
                    reason = "User modes not matched on these servers.";
                    if (diffIneed.length()) {
                        reason += " Not loaded here:" + diffIneed;
                    }
                    if (diffUneed.length()) {
                        reason += " Not loaded there:" + diffUneed;
                    }
                }
            }
        } else if (proto_version == PROTO_INSPIRCD_20
                   && this->capab->CapKeys.find("USERMODES") != this->capab->CapKeys.end()) {
            if (this->capab->CapKeys.find("USERMODES")->second !=
                    ServerInstance->Modes->GiveModeList(MODETYPE_USER)) {
                reason = "One or more of the user modes on the remote server are invalid on this server.";
            }
        }

        if (!reason.empty()) {
            this->SendError("CAPAB negotiation failed: " + reason);
            return false;
        }

        if (this->capab->CapKeys.find("CASEMAPPING") != this->capab->CapKeys.end()) {
            const std::string casemapping =
                this->capab->CapKeys.find("CASEMAPPING")->second;
            if (casemapping != ServerInstance->Config->CaseMapping) {
                reason = "The casemapping of the remote server differs to that of the local server."
                         " Local casemapping: " + ServerInstance->Config->CaseMapping +
                         " Remote casemapping: " + casemapping;
                this->SendError("CAPAB negotiation failed: " + reason);
                return false;
            }
        }

        /* Challenge response, store their challenge for our password */
        std::map<std::string,std::string>::iterator n =
            this->capab->CapKeys.find("CHALLENGE");
        if ((n != this->capab->CapKeys.end())
                && (ServerInstance->Modules->FindService(SERVICE_DATA, "hash/sha256"))) {
            /* Challenge-response is on now */
            this->SetTheirChallenge(n->second);
            if (!this->GetTheirChallenge().empty() && (this->LinkState == CONNECTING)) {
                this->SendCapabilities(2);
                this->WriteLine("SERVER "+ServerInstance->Config->ServerName+" "+this->MakePass(
                                    capab->link->SendPass, capab->theirchallenge)+" 0 "
                                +ServerInstance->Config->GetSID()+" :"+ServerInstance->Config->ServerDesc);
            }
        } else {
            // They didn't specify a challenge or we don't have sha256, we use plaintext
            if (this->LinkState == CONNECTING) {
                this->SendCapabilities(2);
                this->WriteLine("SERVER "+ServerInstance->Config->ServerName+" "
                                +capab->link->SendPass+" 0 "+ServerInstance->Config->GetSID()+" :"
                                +ServerInstance->Config->ServerDesc);
            }
        }
    } else if ((params[0] == "MODULES") && (params.size() == 2)) {
        if (!capab->ModuleList.length()) {
            capab->ModuleList = params[1];
        } else {
            capab->ModuleList.push_back(' ');
            capab->ModuleList.append(params[1]);
        }
    } else if ((params[0] == "MODSUPPORT") && (params.size() == 2)) {
        if (!capab->OptModuleList.length()) {
            capab->OptModuleList = params[1];
        } else {
            capab->OptModuleList.push_back(' ');
            capab->OptModuleList.append(params[1]);
        }
    } else if ((params[0] == "CHANMODES") && (params.size() == 2)) {
        capab->ChanModes = params[1];
    } else if ((params[0] == "USERMODES") && (params.size() == 2)) {
        capab->UserModes = params[1];
    } else if ((params[0] == "CAPABILITIES") && (params.size() == 2)) {
        irc::spacesepstream capabs(params[1]);
        std::string item;
        while (capabs.GetToken(item)) {
            /* Process each key/value pair */
            std::string::size_type equals = item.find('=');
            if (equals != std::string::npos) {
                std::string var(item, 0, equals);
                std::string value(item, equals+1);
                capab->CapKeys[var] = value;
            }
        }
    }
    return true;
}
