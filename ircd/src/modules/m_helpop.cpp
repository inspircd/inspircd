/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "modules/whois.h"

enum {
    // From ircd-ratbox.
    ERR_HELPNOTFOUND = 524,
    RPL_HELPSTART = 704,
    RPL_HELPTXT = 705,
    RPL_ENDOFHELP = 706
};

typedef std::vector<std::string> HelpMessage;

struct HelpTopic {
    // The body of the help topic.
    const HelpMessage body;

    // The title of the help topic.
    const std::string title;

    HelpTopic(const HelpMessage& Body, const std::string& Title)
        : body(Body)
        , title(Title) {
    }
};

typedef std::map<std::string, HelpTopic, irc::insensitive_swo> HelpMap;

class CommandHelpop : public Command {
  private:
    const std::string startkey;

  public:
    HelpMap help;
    std::string nohelp;

    CommandHelpop(Module* Creator)
        : Command(Creator, "HELPOP", 0)
        , startkey("start") {
        syntax = "<any-text>";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        const std::string& topic = parameters.empty() ? startkey : parameters[0];
        HelpMap::const_iterator titer = help.find(topic);
        if (titer == help.end()) {
            user->WriteNumeric(ERR_HELPNOTFOUND, topic, nohelp);
            return CMD_FAILURE;
        }

        const HelpTopic& entry = titer->second;
        user->WriteNumeric(RPL_HELPSTART, topic, entry.title);
        for (HelpMessage::const_iterator liter = entry.body.begin(); liter != entry.body.end(); ++liter) {
            user->WriteNumeric(RPL_HELPTXT, topic, *liter);
        }
        user->WriteNumeric(RPL_ENDOFHELP, topic, "End of /HELPOP.");
        return CMD_SUCCESS;
    }
};

class ModuleHelpop
    : public Module
    , public Whois::EventListener {
  private:
    CommandHelpop cmd;
    SimpleUserModeHandler ho;

  public:
    ModuleHelpop()
        : Whois::EventListener(this)
        , cmd(this)
        , ho(this, "helpop", 'h', true) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        size_t longestkey = 0;

        HelpMap newhelp;
        ConfigTagList tags = ServerInstance->Config->ConfTags("helpop");
        if (tags.first == tags.second) {
            throw ModuleException("You have loaded the helpop module but not configured any help topics!");
        }

        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;

            // Attempt to read the help key.
            const std::string key = tag->getString("key");
            if (key.empty()) {
                throw ModuleException(InspIRCd::Format("<helpop:key> is empty at %s",
                                                       tag->getTagLocation().c_str()));
            } else if (irc::equals(key, "index")) {
                throw ModuleException(
                    InspIRCd::Format("<helpop:key> is set to \"index\" which is reserved at %s",
                                     tag->getTagLocation().c_str()));
            } else if (key.length() > longestkey) {
                longestkey = key.length();
            }

            // Attempt to read the help value.
            std::string value;
            if (!tag->readString("value", value, true) || value.empty()) {
                throw ModuleException(InspIRCd::Format("<helpop:value> is empty at %s",
                                                       tag->getTagLocation().c_str()));
            }

            // Parse the help body. Empty lines are replaced with a single
            // space because some clients are unable to show blank lines.
            HelpMessage helpmsg;
            irc::sepstream linestream(value, '\n', true);
            for (std::string line; linestream.GetToken(line); ) {
                helpmsg.push_back(line.empty() ? " " : line);
            }

            // Read the help title and store the topic.
            const std::string title = tag->getString("title",
                                      InspIRCd::Format("*** Help for %s", key.c_str()), 1);
            if (!newhelp.insert(std::make_pair(key, HelpTopic(helpmsg, title))).second) {
                throw ModuleException(
                    InspIRCd::Format("<helpop> tag with duplicate key '%s' at %s",
                                     key.c_str(), tag->getTagLocation().c_str()));
            }
        }

        // The number of items we can fit on a page.
        HelpMessage indexmsg;
        size_t maxcolumns = 80 / (longestkey + 2);
        for (HelpMap::iterator iter = newhelp.begin(); iter != newhelp.end(); ) {
            std::string indexline;
            for (size_t column = 0; column != maxcolumns; ) {
                if (iter == newhelp.end()) {
                    break;
                }

                indexline.append(iter->first);
                if (++column != maxcolumns) {
                    indexline.append(longestkey - iter->first.length() + 2, ' ');
                }
                iter++;
            }
            indexmsg.push_back(indexline);
        }
        newhelp.insert(std::make_pair("index", HelpTopic(indexmsg, "List of help topics")));
        cmd.help.swap(newhelp);

        ConfigTag* tag = ServerInstance->Config->ConfValue("helpmsg");
        cmd.nohelp = tag->getString("nohelp", "There is no help for the topic you searched for. Please try again.", 1);
    }

    void OnWhois(Whois::Context& whois) CXX11_OVERRIDE {
        if (whois.GetTarget()->IsModeSet(ho)) {
            whois.SendLine(RPL_WHOISHELPOP, "is available for help.");
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /HELPOP command which allows users to view help on various topics and user mode h (helpop) which marks a server operator as being available for help.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleHelpop)
