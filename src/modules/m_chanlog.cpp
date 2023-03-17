/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2018, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

class ModuleChanLog : public Module {
    /*
     * Multimap so people can redirect a snomask to multiple channels.
     */
    typedef insp::flat_multimap<char, std::string> ChanLogTargets;
    ChanLogTargets logstreams;

  public:
    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ChanLogTargets newlogs;

        ConfigTagList tags = ServerInstance->Config->ConfTags("chanlog");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;

            const std::string channel = tag->getString("channel");
            if (!ServerInstance->IsChannel(channel)) {
                throw ModuleException("<chanlog:channel> must be set to a channel name, at " +
                                      tag->getTagLocation());
            }

            const std::string snomasks = tag->getString("snomasks");
            if (snomasks.empty()) {
                throw ModuleException("<chanlog:snomasks> must not be empty, at " +
                                      tag->getTagLocation());
            }

            for (std::string::const_iterator it = snomasks.begin(); it != snomasks.end();
                    it++) {
                newlogs.insert(std::make_pair(*it, channel));
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Logging %c to %s", *it,
                                          channel.c_str());
            }
        }

        logstreams.swap(newlogs);
    }

    ModResult OnSendSnotice(char &sno, std::string &desc,
                            const std::string &msg) CXX11_OVERRIDE {
        std::pair<ChanLogTargets::const_iterator, ChanLogTargets::const_iterator> itpair = logstreams.equal_range(sno);
        if (itpair.first == itpair.second) {
            return MOD_RES_PASSTHRU;
        }

        const std::string snotice = "\002" + desc + "\002: " + msg;

        for (ChanLogTargets::const_iterator it = itpair.first; it != itpair.second; ++it) {
            Channel *c = ServerInstance->FindChan(it->second);
            if (c) {
                ClientProtocol::Messages::Privmsg privmsg(
                    ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->Config->ServerName,
                    c, snotice);
                c->Write(ServerInstance->GetRFCEvents().privmsg, privmsg);
                ServerInstance->PI->SendMessage(c, 0, snotice);
            }
        }

        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows messages sent to snomasks to be logged to a channel.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleChanLog)
