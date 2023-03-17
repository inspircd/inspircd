/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2017, 2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014 Shawn Smith <ShawnSmith0828@gmail.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004, 2006 Craig Edwards <brain@inspircd.org>
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

/** Handle channel mode +L
 */
class Redirect : public ParamMode<Redirect, LocalStringExt> {
  public:
    Redirect(Module* Creator)
        : ParamMode<Redirect, LocalStringExt>(Creator, "redirect", 'L') {
        syntax = "<target>";
    }

    ModeAction OnSet(User* source, Channel* channel,
                     std::string& parameter) CXX11_OVERRIDE {
        if (IS_LOCAL(source)) {
            if (!ServerInstance->IsChannel(parameter)) {
                source->WriteNumeric(Numerics::NoSuchChannel(parameter));
                return MODEACTION_DENY;
            }
        }

        if (IS_LOCAL(source) && !source->IsOper()) {
            Channel* c = ServerInstance->FindChan(parameter);
            if (!c) {
                source->WriteNumeric(690,
                                     InspIRCd::Format("Target channel %s must exist to be set as a redirect.",
                                                      parameter.c_str()));
                return MODEACTION_DENY;
            } else if (c->GetPrefixValue(source) < OP_VALUE) {
                source->WriteNumeric(690,
                                     InspIRCd::Format("You must be opped on %s to set it as a redirect.",
                                                      parameter.c_str()));
                return MODEACTION_DENY;
            }
        }

        /*
         * We used to do some checking for circular +L here, but there is no real need for this any more especially as we
         * now catch +L looping in PreJoin. Remove it, since O(n) logic makes me sad, and we catch it anyway. :) -- w00t
         */
        ext.set(channel, parameter);
        return MODEACTION_ALLOW;
    }

    void SerializeParam(Channel* chan, const std::string* str, std::string& out) {
        out += *str;
    }
};

class ModuleRedirect : public Module {
    Redirect re;
    SimpleUserModeHandler antiredirectmode;
    ChanModeReference limitmode;

  public:
    ModuleRedirect()
        : re(this)
        , antiredirectmode(this, "antiredirect", 'L')
        , limitmode(this, "limit") {
    }

    ModResult OnUserPreJoin(LocalUser* user, Channel* chan,
                            const std::string& cname, std::string& privs,
                            const std::string& keygiven) CXX11_OVERRIDE {
        if (chan) {
            if (chan->IsModeSet(re) && chan->IsModeSet(limitmode)) {
                if (chan->GetUserCounter() >= ConvToNum<size_t>(chan->GetModeParameter(
                            limitmode))) {
                    const std::string& channel = *re.ext.get(chan);

                    /* sometimes broken ulines can make circular or chained +L, avoid this */
                    Channel* destchan = ServerInstance->FindChan(channel);
                    if (destchan && destchan->IsModeSet(re)) {
                        user->WriteNumeric(470, cname, '*',
                                           "You may not join this channel. A redirect is set, but you may not be redirected as it is a circular loop.");
                        return MOD_RES_DENY;
                    }

                    if (user->IsModeSet(antiredirectmode)) {
                        user->WriteNumeric(470, cname, channel, "Force redirection stopped.");
                        return MOD_RES_DENY;
                    } else {
                        user->WriteNumeric(470, cname, channel,
                                           "You may not join this channel, so you are automatically being transferred to the redirected channel.");
                        Channel::JoinUser(user, channel);
                        return MOD_RES_DENY;
                    }
                }
            }
        }
        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows users to be redirected to another channel when the user limit is reached.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleRedirect)
