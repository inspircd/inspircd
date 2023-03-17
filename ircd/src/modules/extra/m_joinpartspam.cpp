/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016-2020 Matt Schatz <genius3000@g3k.solutions>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <joinpartspam allowredirect="no" freeredirect="no" modechar="x">
/// $ModDepends: core 3
/// $ModDesc: Adds channel mode +x to block a user after x per y joins and parts/quits (join/part spam)

/* Configuration and defaults:
 * allowredirect: Whether channel redirection is allowed or not. Default: no
 * freeredirect:  Skip the channel operator checks for redirection. Default: no
 * TIP: You can remove a block on a user with a /INVITE
 *
 * Helpop Lines for the CHMODES section:
 * Find: '<helpop key="chmodes" title="Channel Modes" value="'
 * Place just above the 'z     Blocks non-SSL...' line:
 x <cycles>:<sec>:<block>[:#channel] Blocks a user after the set
                    number of Join and Part/Quit cycles in the set
                    seconds for the given block duration (seconds)
                    (requires the joinpartspam contrib module).
                    An optional redirect channel can be set.
 */


#include "inspircd.h"

enum {
    RPL_CHANBLOCKED = 545
};

class joinpartspamsettings {
    struct Tracking {
        time_t reset;
        unsigned int counter;
        Tracking() : reset(0), counter(0) { }
    };

    std::map<std::string, Tracking> cycler;
    std::map<std::string, time_t> blocked;
    time_t lastcleanup;

  public:
    unsigned int cycles;
    unsigned int secs;
    unsigned int block;
    std::string redirect;

    joinpartspamsettings(unsigned int c, unsigned int s, unsigned int b,
                         std::string& r)
        : lastcleanup(ServerInstance->Time())
        , cycles(c)
        , secs(s)
        , block(b)
        , redirect(r) {
    }

    // Called by PostJoin to possibly reset a cycler's Tracking and increment the counter
    void addcycle(const std::string& mask) {
        /* If mask isn't already tracked, set reset time
         * If tracked and reset time is up, reset counter and reset time
         * Also assume another server blocked, with the block timing out or a
         * user removed it if counter >= cycles, reset counter and reset time
         */
        Tracking& tracking = cycler[mask];

        if (tracking.reset == 0) {
            tracking.reset = ServerInstance->Time() + secs;
        } else if (ServerInstance->Time() > tracking.reset
                   || tracking.counter >= cycles) {
            tracking.counter = 0;
            tracking.reset = ServerInstance->Time() + secs;
        }

        ++tracking.counter;

        this->cleanup();
    }

    /* Called by PreJoin to check if a cycler's counter exceeds the set cycles,
     * adds them to the blocked list if so.
     * Will first clear a cycler if their reset time is up.
     */
    bool zapme(const std::string& mask) {
        // Only check reset time and counter if they are already tracked as a cycler
        std::map<std::string, Tracking>::iterator it = cycler.find(mask);
        if (it == cycler.end()) {
            return false;
        }

        const Tracking& tracking = it->second;

        if (ServerInstance->Time() > tracking.reset) {
            cycler.erase(it);
        } else if (tracking.counter >= cycles) {
            blocked[mask] = ServerInstance->Time() + block;
            cycler.erase(it);
            return true;
        }

        return false;
    }

    // Check if a joining user is blocked, clear them if blocktime is up
    bool isblocked(const std::string& mask) {
        std::map<std::string, time_t>::iterator it = blocked.find(mask);
        if (it == blocked.end()) {
            return false;
        }

        if (ServerInstance->Time() > it->second) {
            blocked.erase(it);
        } else {
            return true;
        }

        return false;
    }

    void removeblock(const std::string& mask) {
        std::map<std::string, time_t>::iterator it = blocked.find(mask);
        if (it != blocked.end()) {
            blocked.erase(it);
        }
    }

    // Clear expired entries of non cyclers and blocked cyclers
    void cleanup() {
        // 10 minutes should be a reasonable wait time
        if (ServerInstance->Time() - lastcleanup < 600) {
            return;
        }

        lastcleanup = ServerInstance->Time();

        for (std::map<std::string, Tracking>::iterator it = cycler.begin();
                it != cycler.end(); ) {
            const Tracking& tracking = it->second;

            if (ServerInstance->Time() > tracking.reset) {
                cycler.erase(it++);
            } else {
                ++it;
            }
        }

        for (std::map<std::string, time_t>::iterator i = blocked.begin();
                i != blocked.end(); ) {
            if (ServerInstance->Time() > i->second) {
                blocked.erase(i++);
            } else {
                ++i;
            }
        }
    }
};

class JoinPartSpam : public
    ParamMode<JoinPartSpam, SimpleExtItem<joinpartspamsettings> > {
    bool& allowredirect;
    bool& freeredirect;

    bool ParseCycles(irc::sepstream& stream, unsigned int& cycles) {
        std::string strcycles;
        if (!stream.GetToken(strcycles)) {
            return false;
        }

        unsigned int result = ConvToNum<unsigned int>(strcycles);
        if (result < 2 || result > 20) {
            return false;
        }

        cycles = result;
        return true;
    }

    bool ParseSeconds(irc::sepstream& stream, unsigned int& seconds) {
        std::string strseconds;
        if (!stream.GetToken(strseconds)) {
            return false;
        }

        unsigned int result = ConvToNum<unsigned int>(strseconds);
        if (result < 1 || result > 43200) {
            return false;
        }

        seconds = result;
        return true;
    }

    bool ParseRedirect(irc::sepstream& stream, std::string& redirect, User* source,
                       Channel* chan) {
        std::string strredirect;
        // This parameter is optional
        if (!stream.GetToken(strredirect)) {
            return true;
        }

        if (!allowredirect) {
            source->WriteNumeric(Numerics::InvalidModeParameter(chan, this, strredirect,
                                 "Invalid join/part spam mode parameter, the server admin has disabled channel redirection."));
            return false;
        }

        if (!ServerInstance->IsChannel(strredirect)) {
            source->WriteNumeric(Numerics::InvalidModeParameter(chan, this, strredirect,
                                 "Invalid join/part spam mode parameter, redirect channel needs to be a valid channel name."));
            return false;
        }

        if (irc::equals(chan->name, strredirect)) {
            source->WriteNumeric(Numerics::InvalidModeParameter(chan, this, strredirect,
                                 "Invalid join/part spam mode parameter, cannot redirect to myself."));
            return false;
        }

        // Channel is at least valid now.
        redirect = strredirect;

        // If a server is setting the mode, skip the exists/access checks.
        // This lets m_permchannels and a syncing server set the mode.
        if (IS_SERVER(source)) {
            return true;
        }

        Channel* c = ServerInstance->FindChan(strredirect);
        if (!c) {
            source->WriteNumeric(Numerics::InvalidModeParameter(chan, this, strredirect,
                                 "Invalid join/part spam mode parameter, redirect channel must exist."));
            return false;
        }

        if (!freeredirect && c->GetPrefixValue(source) < HALFOP_VALUE) {
            source->WriteNumeric(Numerics::InvalidModeParameter(chan, this, strredirect,
                                 "Invalid join/part spam mode parameter, you need at least halfop in the redirect channel."));
            return false;
        }

        return true;
    }

  public:
    JoinPartSpam(Module* Creator, bool& allow, bool& free)
        : ParamMode<JoinPartSpam, SimpleExtItem<joinpartspamsettings> >(Creator,
                "joinpartspam",
                ServerInstance->Config->ConfValue("joinpartspam")->getString("modechar", "x", 1,
                        1)[0])
        , allowredirect(allow)
        , freeredirect(free) {
#if defined INSPIRCD_VERSION_SINCE && INSPIRCD_VERSION_SINCE(3, 2)
        syntax = "<cycles>:<seconds>:<block-time>";
#endif
    }

    ModeAction OnSet(User* source, Channel* chan,
                     std::string& parameter) CXX11_OVERRIDE {
        irc::sepstream stream(parameter, ':');
        unsigned int cycles, secs, block;
        cycles = secs = block = 0;
        std::string redirect;

        if (!ParseCycles(stream, cycles)) {
            source->WriteNumeric(Numerics::InvalidModeParameter(chan, this, parameter,
                                 "Invalid join/part spam mode parameter, 'cycles' needs to be between 2 and 20."));
            return MODEACTION_DENY;
        }
        if (!ParseSeconds(stream, secs) || !ParseSeconds(stream, block)) {
            source->WriteNumeric(Numerics::InvalidModeParameter(chan, this, parameter,
                                 "Invalid join/part spam mode parameter, 'duration' and 'block time' need to be between 1 and 43200."));
            return MODEACTION_DENY;
        }
        // Error message is sent from ParseRedirect()
        if (!ParseRedirect(stream, redirect, source, chan)) {
            return MODEACTION_DENY;
        }

        ext.set(chan, new joinpartspamsettings(cycles, secs, block, redirect));
        return MODEACTION_ALLOW;
    }

    void SerializeParam(Channel* chan, joinpartspamsettings* jpss,
                        std::string& out) {
        out.append(ConvToStr(jpss->cycles)).push_back(':');
        out.append(ConvToStr(jpss->secs)).push_back(':');
        out.append(ConvToStr(jpss->block));
        if (!jpss->redirect.empty()) {
            out.push_back(':');
            out.append(jpss->redirect);
        }
    }
};

class ModuleJoinPartSpam : public Module {
    bool allowredirect;
    bool freeredirect;
    JoinPartSpam jps;

  public:
    ModuleJoinPartSpam()
        : allowredirect(false)
        , freeredirect(false)
        , jps(this, allowredirect, freeredirect) {
    }

    void Prioritize() CXX11_OVERRIDE {
        // Let bans, etc. stop the join first
        ServerInstance->Modules->SetPriority(this, I_OnUserPreJoin, PRIORITY_LAST);
    }

    void ReadConfig(ConfigStatus&) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("joinpartspam");
        allowredirect = tag->getBool("allowredirect");
        freeredirect = tag->getBool("freeredirect");
    }

    // Check if the user is blocked or should now be blocked
    bool BlockJoin(LocalUser* user, Channel* chan, bool quiet = false) {
        joinpartspamsettings* jpss = jps.ext.get(chan);
        if (!jpss) {
            return false;
        }

        const std::string& mask(user->MakeHost());

        if (jpss->isblocked(mask)) {
            if (quiet) {
                return true;
            }

            user->WriteNumeric(RPL_CHANBLOCKED, chan->name,
                               InspIRCd::Format("Channel join/part spam triggered (limit is %u cycles in %u secs). Please try again later.",
                                                jpss->cycles, jpss->secs));

            return true;
        } else if (jpss->zapme(mask)) {
            if (quiet) {
                return true;
            }

            // The user is now in the blocked list, deny the join, and if
            // redirect is wanted and allowed, we join the user to that channel.
            user->WriteNumeric(RPL_CHANBLOCKED, chan->name,
                               InspIRCd::Format("Channel join/part spam triggered (limit is %u cycles in %u secs). Please try again in %u seconds.",
                                                jpss->cycles, jpss->secs, jpss->block));

            if (allowredirect && !jpss->redirect.empty()) {
                Channel::JoinUser(user, jpss->redirect);
            }
            return true;
        }

        return false;
    }

    // Catch /CYCLE, deny if the user is going to be blocked
    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        if (!validated || command != "CYCLE" || user->IsOper()) {
            return MOD_RES_PASSTHRU;
        }

        Channel* chan = ServerInstance->FindChan(parameters[0]);
        if (!chan || !chan->IsModeSet(&jps)) {
            return MOD_RES_PASSTHRU;
        }

        bool deny = BlockJoin(user, chan, true);
        if (!deny) {
            return MOD_RES_PASSTHRU;
        }

        user->WriteNotice(InspIRCd::Format("*** You may not cycle, as you would then trigger the join/part spam protection on channel %s",
                                           chan->name.c_str()));
        return MOD_RES_DENY;
    }

    // Block the join if the user is blocked (already or as of now)
    ModResult OnUserPreJoin(LocalUser* user, Channel* chan,
                            const std::string& cname, std::string& privs,
                            const std::string& keygiven) CXX11_OVERRIDE {
        if (!chan || !chan->IsModeSet(&jps)) {
            return MOD_RES_PASSTHRU;
        }
        if (user->IsOper()) {
            return MOD_RES_PASSTHRU;
        }

        return BlockJoin(user, chan) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
    }

    // Only count successful joins
    void OnUserJoin(Membership* memb, bool sync, bool created,
                    CUList& excepts) CXX11_OVERRIDE {
        if (sync) {
            return;
        }
        if (created || !memb->chan->IsModeSet(&jps)) {
            return;
        }
        if (memb->user->IsOper()) {
            return;
        }

        joinpartspamsettings* jpss = jps.ext.get(memb->chan);
        if (jpss) {
            const std::string& mask(memb->user->MakeHost());
            jpss->addcycle(mask);
        }
    }

    // Remove a block on a user on a successful invite
    void OnUserInvite(User*, User* user, Channel* chan, time_t, unsigned int,
                      CUList&) CXX11_OVERRIDE {
        if (!chan->IsModeSet(&jps)) {
            return;
        }

        joinpartspamsettings* jpss = jps.ext.get(chan);
        if (!jpss) {
            return;
        }

        const std::string& mask(user->MakeHost());
        jpss->removeblock(mask);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides channel mode +" + ConvToStr(jps.GetModeChar()) + " for blocking Join/Part spammers.", VF_OPTCOMMON);
    }
};

MODULE_INIT(ModuleJoinPartSpam)
