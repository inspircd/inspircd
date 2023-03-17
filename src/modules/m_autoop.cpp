/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011 jackmcbarn <jackmcbarn@inspircd.org>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

enum {
    // InspIRCd-specific.
    RPL_ACCESSLIST = 910,
    RPL_ENDOFACCESSLIST = 911
};

class AutoOpList : public ListModeBase {
  public:
    AutoOpList(Module* Creator)
        : ListModeBase(Creator, "autoop", 'w', "End of Channel Access List",
                       RPL_ACCESSLIST, RPL_ENDOFACCESSLIST, true) {
        ranktoset = ranktounset = OP_VALUE;
        syntax = "<prefix>:<mask>";
        tidy = false;
    }

    PrefixMode* FindMode(const std::string& mid) {
        if (mid.length() == 1) {
            return ServerInstance->Modes->FindPrefixMode(mid[0]);
        }

        ModeHandler* mh = ServerInstance->Modes->FindMode(mid, MODETYPE_CHANNEL);
        return mh ? mh->IsPrefixMode() : NULL;
    }

    ModResult AccessCheck(User* source, Channel* channel, std::string &parameter,
                          bool adding) CXX11_OVERRIDE {
        std::string::size_type pos = parameter.find(':');
        if (pos == 0 || pos == std::string::npos) {
            return adding ? MOD_RES_DENY : MOD_RES_PASSTHRU;
        }
        unsigned int mylevel = channel->GetPrefixValue(source);
        std::string mid(parameter, 0, pos);
        PrefixMode* mh = FindMode(mid);

        if (adding && !mh) {
            source->WriteNumeric(ERR_UNKNOWNMODE, mid,
                                 InspIRCd::Format("Cannot find prefix mode '%s' for autoop", mid.c_str()));
            return MOD_RES_DENY;
        } else if (!mh) {
            return MOD_RES_PASSTHRU;
        }

        std::string dummy;
        if (mh->AccessCheck(source, channel, dummy, true) == MOD_RES_DENY) {
            return MOD_RES_DENY;
        }
        if (mh->GetLevelRequired(adding) > mylevel) {
            source->WriteNumeric(ERR_CHANOPRIVSNEEDED, channel->name,
                                 InspIRCd::Format("You must be able to %s mode %c (%s) to %s an autoop containing it",
                                                  adding ? "set" : "unset", mh->GetModeChar(), mh->name.c_str(),
                                                  adding ? "add" : "remove"));
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }
};

class ModuleAutoOp : public Module {
    AutoOpList mh;

  public:
    ModuleAutoOp() : mh(this) {
    }

    void OnPostJoin(Membership *memb) CXX11_OVERRIDE {
        if (!IS_LOCAL(memb->user)) {
            return;
        }

        ListModeBase::ModeList* list = mh.GetList(memb->chan);
        if (list) {
            Modes::ChangeList changelist;
            for (ListModeBase::ModeList::iterator it = list->begin(); it != list->end();
                    it++) {
                std::string::size_type colon = it->mask.find(':');
                if (colon == std::string::npos) {
                    continue;
                }
                if (memb->chan->CheckBan(memb->user, it->mask.substr(colon+1))) {
                    PrefixMode* given = mh.FindMode(it->mask.substr(0, colon));
                    if (given) {
                        changelist.push_add(given, memb->user->nick);
                    }
                }
            }
            ServerInstance->Modes->Process(ServerInstance->FakeClient, memb->chan, NULL,
                                           changelist);
        }
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        mh.DoRehash();
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode w (autoop) which allows channel operators to define an access list which gives status ranks to users on join.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleAutoOp)
