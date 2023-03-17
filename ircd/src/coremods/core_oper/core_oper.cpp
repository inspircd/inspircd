/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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
#include "core_oper.h"

class CoreModOper : public Module {
    std::string powerhash;

    CommandDie cmddie;
    CommandKill cmdkill;
    CommandOper cmdoper;
    CommandRehash cmdrehash;
    CommandRestart cmdrestart;

  public:
    CoreModOper()
        : cmddie(this, powerhash)
        , cmdkill(this)
        , cmdoper(this)
        , cmdrehash(this)
        , cmdrestart(this, powerhash) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("power");

        // The hash method for *BOTH* the die and restart passwords
        powerhash = tag->getString("hash");

        cmddie.password = tag->getString("diepass", ServerInstance->Config->ServerName, 1);
        cmdrestart.password = tag->getString("restartpass", ServerInstance->Config->ServerName, 1);

        ConfigTag* security = ServerInstance->Config->ConfValue("security");
        cmdkill.hidenick = security->getString("hidekills");
        cmdkill.hideuline = security->getBool("hideulinekills");
    }

    void OnPostOper(User* user, const std::string&,
                    const std::string&) CXX11_OVERRIDE {
        LocalUser* luser = IS_LOCAL(user);
        if (!luser) {
            return;
        }

        const std::string vhost = luser->oper->getConfig("vhost");
        if (!vhost.empty()) {
            luser->ChangeDisplayedHost(vhost);
        }

        const std::string klass = luser->oper->getConfig("class");
        if (!klass.empty()) {
            luser->SetClass(klass);
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the DIE, KILL, OPER, REHASH, and RESTART commands", VF_VENDOR | VF_CORE);
    }
};

MODULE_INIT(CoreModOper)
