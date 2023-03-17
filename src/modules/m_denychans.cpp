/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Craig Edwards <brain@inspircd.org>
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
    // InspIRCd-specific.
    ERR_BADCHANNEL = 926
};

struct BadChannel {
    bool allowopers;
    std::string name;
    std::string reason;
    std::string redirect;

    BadChannel(const std::string& Name, const std::string& Redirect,
               const std::string& Reason, bool AllowOpers)
        : allowopers(AllowOpers)
        , name(Name)
        , reason(Reason)
        , redirect(Redirect) {
    }
};

typedef std::vector<BadChannel> BadChannels;
typedef std::vector<std::string> GoodChannels;

class ModuleDenyChannels : public Module {
  private:
    BadChannels badchannels;
    GoodChannels goodchannels;
    UserModeReference antiredirectmode;
    ChanModeReference redirectmode;

  public:
    ModuleDenyChannels()
        : antiredirectmode(this, "antiredirect")
        , redirectmode(this, "redirect") {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        GoodChannels goodchans;
        ConfigTagList tags = ServerInstance->Config->ConfTags("goodchan");
        for (ConfigIter iter = tags.first; iter != tags.second; ++iter) {
            ConfigTag* tag = iter->second;

            // Ensure that we have the <goodchan:name> parameter.
            const std::string name = tag->getString("name");
            if (name.empty()) {
                throw ModuleException("<goodchan:name> is a mandatory field, at " +
                                      tag->getTagLocation());
            }

            goodchans.push_back(name);
        }

        BadChannels badchans;
        tags = ServerInstance->Config->ConfTags("badchan");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;

            // Ensure that we have the <badchan:name> parameter.
            const std::string name = tag->getString("name");
            if (name.empty()) {
                throw ModuleException("<badchan:name> is a mandatory field, at " +
                                      tag->getTagLocation());
            }

            // Ensure that we have the <badchan:reason> parameter.
            const std::string reason = tag->getString("reason");
            if (reason.empty()) {
                throw ModuleException("<badchan:reason> is a mandatory field, at " +
                                      tag->getTagLocation());
            }

            const std::string redirect = tag->getString("redirect");
            if (!redirect.empty()) {
                // Ensure that <badchan:redirect> contains a channel name.
                if (!ServerInstance->IsChannel(redirect)) {
                    throw ModuleException("<badchan:redirect> is not a valid channel name, at " +
                                          tag->getTagLocation());
                }

                // We defer the rest of the validation of the redirect channel until we have
                // finished parsing all of the badchans.
            }

            badchans.push_back(BadChannel(name, redirect, reason,
                                          tag->getBool("allowopers")));
        }

        // Now we have all of the badchan information recorded we can check that all redirect
        // channels can actually be redirected to.
        for (BadChannels::const_iterator i = badchans.begin(); i != badchans.end(); ++i) {
            const BadChannel& badchan = *i;

            // If there is no redirect channel we have nothing to do.
            if (badchan.redirect.empty()) {
                continue;
            }

            // If the redirect channel is whitelisted then it is okay.
            bool whitelisted = false;
            for (GoodChannels::const_iterator j = goodchans.begin(); j != goodchans.end();
                    ++j) {
                if (InspIRCd::Match(badchan.redirect, *j)) {
                    whitelisted = true;
                    break;
                }
            }

            if (whitelisted) {
                continue;
            }

            // If the redirect channel is not blacklisted then it is okay.
            for (BadChannels::const_iterator j = badchans.begin(); j != badchans.end(); ++j)
                if (InspIRCd::Match(badchan.redirect, j->name)) {
                    throw ModuleException("<badchan:redirect> cannot be a blacklisted channel name");
                }
        }

        // The config file contained no errors so we can apply the new configuration.
        badchannels.swap(badchans);
        goodchannels.swap(goodchans);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server administrator to prevent users from joining channels matching a glob.", VF_VENDOR);
    }


    ModResult OnUserPreJoin(LocalUser* user, Channel* chan,
                            const std::string& cname, std::string& privs,
                            const std::string& keygiven) CXX11_OVERRIDE {
        for (BadChannels::const_iterator j = badchannels.begin(); j != badchannels.end(); ++j) {
            const BadChannel& badchan = *j;

            // If the channel does not match the current entry we have nothing else to do.
            if (!InspIRCd::Match(cname, badchan.name)) {
                continue;
            }

            // If the user is an oper and opers are allowed to enter this blacklisted channel
            // then allow the join.
            if (user->IsOper() && badchan.allowopers) {
                return MOD_RES_PASSTHRU;
            }

            // If the channel matches a whitelist then allow the join.
            for (GoodChannels::const_iterator i = goodchannels.begin();
                    i != goodchannels.end(); ++i)
                if (InspIRCd::Match(cname, *i)) {
                    return MOD_RES_PASSTHRU;
                }

            // If there is no redirect chan, the user has enabled the antiredirect mode, or
            // the target channel redirects elsewhere we just tell the user and deny the join.
            Channel* target = NULL;
            if (badchan.redirect.empty() || user->IsModeSet(antiredirectmode)
                    || ((target = ServerInstance->FindChan(badchan.redirect))
                        && target->IsModeSet(redirectmode))) {
                user->WriteNumeric(ERR_BADCHANNEL, cname,
                                   InspIRCd::Format("Channel %s is forbidden: %s",
                                                    cname.c_str(), badchan.reason.c_str()));
                return MOD_RES_DENY;
            }

            // Redirect the user to the target channel.
            user->WriteNumeric(ERR_BADCHANNEL, cname,
                               InspIRCd::Format("Channel %s is forbidden, redirecting to %s: %s",
                                                cname.c_str(), badchan.redirect.c_str(), badchan.reason.c_str()));
            Channel::JoinUser(user, badchan.redirect);
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleDenyChannels)
