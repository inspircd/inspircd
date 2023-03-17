/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2017-2019, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
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

ListModeBase::ListModeBase(Module* Creator, const std::string& Name,
                           char modechar, const std::string& eolstr, unsigned int lnum,
                           unsigned int eolnum, bool autotidy)
    : ModeHandler(Creator, Name, modechar, PARAM_ALWAYS, MODETYPE_CHANNEL, MC_LIST)
    , listnumeric(lnum)
    , endoflistnumeric(eolnum)
    , endofliststring(eolstr)
    , tidy(autotidy)
    , extItem(name + "_mode_list", ExtensionItem::EXT_CHANNEL, Creator) {
    list = true;
}

void ListModeBase::DisplayList(User* user, Channel* channel) {
    ChanData* cd = extItem.get(channel);
    if (!cd || cd->list.empty()) {
        this->DisplayEmptyList(user, channel);
        return;
    }

    for (ModeList::const_iterator it = cd->list.begin(); it != cd->list.end();
            ++it) {
        user->WriteNumeric(listnumeric, channel->name, it->mask, it->setter, it->time);
    }
    user->WriteNumeric(endoflistnumeric, channel->name, endofliststring);
}

void ListModeBase::DisplayEmptyList(User* user, Channel* channel) {
    user->WriteNumeric(endoflistnumeric, channel->name, endofliststring);
}

void ListModeBase::RemoveMode(Channel* channel, Modes::ChangeList& changelist) {
    ChanData* cd = extItem.get(channel);
    if (cd) {
        for (ModeList::iterator it = cd->list.begin(); it != cd->list.end(); it++) {
            changelist.push_remove(this, it->mask);
        }
    }
}

void ListModeBase::DoRehash() {
    ConfigTagList tags = ServerInstance->Config->ConfTags("maxlist");
    limitlist newlimits;
    bool seen_default = false;
    for (ConfigIter i = tags.first; i != tags.second; i++) {
        ConfigTag* c = i->second;

        const std::string mname = c->getString("mode");
        if (!mname.empty() && !stdalgo::string::equalsci(mname, name)
                && !(mname.length() == 1 && GetModeChar() == mname[0])) {
            continue;
        }

        ListLimit limit(c->getString("chan", "*", 1), c->getUInt("limit",
                        DEFAULT_LIST_SIZE));

        if (limit.mask.empty()) {
            throw ModuleException(InspIRCd::Format("<maxlist:chan> is empty, at %s",
                                                   c->getTagLocation().c_str()));
        }

        if (limit.mask == "*" || limit.mask == "#*") {
            seen_default = true;
        }

        newlimits.push_back(limit);
    }

    // If no default limit has been specified then insert one.
    if (!seen_default) {
        ServerInstance->Logs->Log("MODE", LOG_DEBUG,
                                  "No default <maxlist> entry was found for the %s mode; defaulting to %u",
                                  name.c_str(), DEFAULT_LIST_SIZE);
        newlimits.push_back(ListLimit("*", DEFAULT_LIST_SIZE));
    }

    // Most of the time our settings are unchanged, so we can avoid iterating the chanlist
    if (chanlimits == newlimits) {
        return;
    }

    chanlimits.swap(newlimits);

    const chan_hash& chans = ServerInstance->GetChans();
    for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); ++i) {
        ChanData* cd = extItem.get(i->second);
        if (cd) {
            cd->maxitems = -1;
        }
    }
}

unsigned int ListModeBase::FindLimit(const std::string& channame) {
    for (limitlist::iterator it = chanlimits.begin(); it != chanlimits.end();
            ++it) {
        if (InspIRCd::Match(channame, it->mask)) {
            // We have a pattern matching the channel
            return it->limit;
        }
    }
    return 0;
}

unsigned int ListModeBase::GetLimitInternal(const std::string& channame,
        ChanData* cd) {
    if (cd->maxitems < 0) {
        cd->maxitems = FindLimit(channame);
    }
    return cd->maxitems;
}

unsigned int ListModeBase::GetLimit(Channel* channel) {
    ChanData* cd = extItem.get(channel);
    if (!cd) { // just find the limit
        return FindLimit(channel->name);
    }

    return GetLimitInternal(channel->name, cd);
}

unsigned int ListModeBase::GetLowerLimit() {
    if (chanlimits.empty()) {
        return DEFAULT_LIST_SIZE;
    }

    unsigned int limit = UINT_MAX;
    for (limitlist::iterator iter = chanlimits.begin(); iter != chanlimits.end();
            ++iter) {
        if (iter->limit < limit) {
            limit = iter->limit;
        }
    }
    return limit;
}

ModeAction ListModeBase::OnModeChange(User* source, User*, Channel* channel,
                                      std::string &parameter, bool adding) {
    // Try and grab the list
    ChanData* cd = extItem.get(channel);

    if (adding) {
        if (tidy) {
            ModeParser::CleanMask(parameter);
        }

        // If there was no list
        if (!cd) {
            // Make one
            cd = new ChanData;
            extItem.set(channel, cd);
        }

        // Check if the item already exists in the list
        for (ModeList::iterator it = cd->list.begin(); it != cd->list.end(); it++) {
            if (parameter == it->mask) {
                /* Give a subclass a chance to error about this */
                TellAlreadyOnList(source, channel, parameter);

                // it does, deny the change
                return MODEACTION_DENY;
            }
        }

        if ((IS_LOCAL(source))
                && (cd->list.size() >= GetLimitInternal(channel->name, cd))) {
            /* List is full, give subclass a chance to send a custom message */
            TellListTooLong(source, channel, parameter);
            return MODEACTION_DENY;
        }

        /* Ok, it *could* be allowed, now give someone subclassing us
         * a chance to validate the parameter.
         * The param is passed by reference, so they can both modify it
         * and tell us if we allow it or not.
         *
         * eg, the subclass could:
         * 1) allow
         * 2) 'fix' parameter and then allow
         * 3) deny
         */
        if (ValidateParam(source, channel, parameter)) {
            // And now add the mask onto the list...
            cd->list.push_back(ListItem(parameter, source->nick, ServerInstance->Time()));
            return MODEACTION_ALLOW;
        } else {
            /* If they deny it they have the job of giving an error message */
            return MODEACTION_DENY;
        }
    } else {
        // We're taking the mode off
        if (cd) {
            for (ModeList::iterator it = cd->list.begin(); it != cd->list.end(); ++it) {
                if (parameter == it->mask) {
                    stdalgo::vector::swaperase(cd->list, it);
                    return MODEACTION_ALLOW;
                }
            }
        }

        /* Tried to remove something that wasn't set */
        TellNotSet(source, channel, parameter);
        return MODEACTION_DENY;
    }
}

bool ListModeBase::ValidateParam(User*, Channel*, std::string&) {
    return true;
}

void ListModeBase::OnParameterMissing(User*, User*, Channel*) {
    // Intentionally left blank.
}

void ListModeBase::TellListTooLong(User* source, Channel* channel,
                                   std::string& parameter) {
    source->WriteNumeric(ERR_BANLISTFULL, channel->name, parameter, mode,
                         InspIRCd::Format("Channel %s list is full", name.c_str()));
}

void ListModeBase::TellAlreadyOnList(User* source, Channel* channel,
                                     std::string& parameter) {
    source->WriteNumeric(ERR_LISTMODEALREADYSET, channel->name, parameter, mode,
                         InspIRCd::Format("Channel %s list already contains %s", name.c_str(),
                                          parameter.c_str()));
}

void ListModeBase::TellNotSet(User* source, Channel* channel,
                              std::string& parameter) {
    source->WriteNumeric(ERR_LISTMODENOTSET, channel->name, parameter, mode,
                         InspIRCd::Format("Channel %s list does not contain %s", name.c_str(),
                                          parameter.c_str()));
}
