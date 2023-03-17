/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 3
/// $ModDesc: Allows services to add custom tags to messages sent by clients.


#include "inspircd.h"
#include "modules/cap.h"
#include "modules/ctctags.h"
#include "modules/ircv3.h"
#include "modules/who.h"

typedef insp::flat_map<std::string, std::string, irc::insensitive_swo>
CustomTagMap;
typedef insp::flat_map<std::string, size_t, irc::insensitive_swo>
SpecialMessageMap;

class CustomTagsExtItem CXX11_FINAL
    : public SimpleExtItem<CustomTagMap> {
  private:
    CTCTags::CapReference& ctctagcap;
    ClientProtocol::EventProvider tagmsgprov;

  public:
    bool broadcastchanges;

    CustomTagsExtItem(Module* Creator, CTCTags::CapReference& capref)
        : SimpleExtItem<CustomTagMap>("custom-tags", ExtensionItem::EXT_USER, Creator)
        , ctctagcap(capref)
        , tagmsgprov(Creator, "TAGMSG") {
    }

    void FromNetwork(Extensible* container,
                     const std::string& value) CXX11_OVERRIDE {
        User* user = static_cast<User*>(container);
        if (!user) {
            return;
        }

        CustomTagMap* list = new CustomTagMap();
        irc::spacesepstream ts(value);
        while (!ts.StreamEnd()) {
            std::string tagname;
            std::string tagvalue;
            if (!ts.GetToken(tagname) || !ts.GetToken(tagvalue)) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                          "Malformed tag list received for %s: %s",
                                          user->uuid.c_str(), value.c_str());
                delete list;
                return;
            }

            list->insert(std::make_pair(tagname, tagvalue));
        }

        if (!list->empty()) {
            set(user, list);
            if (!broadcastchanges || !ctctagcap) {
                return;
            }

            ClientProtocol::TagMap tags;
            CTCTags::TagMessage tagmsg(user, "*", tags);
            ClientProtocol::Event tagev(tagmsgprov, tagmsg);
            IRCv3::WriteNeighborsWithCap(user, tagev, *ctctagcap, true);
        } else {
            unset(container);
            delete list;
        }
    }

    std::string ToNetwork(const Extensible* container,
                          void* item) const CXX11_OVERRIDE {
        CustomTagMap* list = static_cast<CustomTagMap*>(item);
        std::string buf;
        for (CustomTagMap::const_iterator iter = list->begin(); iter != list->end();
                ++iter) {
            if (iter != list->begin()) {
                buf.push_back(' ');
            }

            buf.append(iter->first);
            buf.push_back(' ');
            buf.append(iter->second);
        }
        return buf;
    }
};

class CustomTags CXX11_FINAL
    : public ClientProtocol::MessageTagProvider {
  private:
    CTCTags::CapReference ctctagcap;

    User* UserFromMsg(ClientProtocol::Message& msg) {
        SpecialMessageMap::const_iterator iter = specialmsgs.find(msg.GetCommand());
        if (iter == specialmsgs.end()) {
            return NULL;    // Not a special message.
        }

        size_t nick_index = iter->second;
        if (irc::equals(msg.GetCommand(), "354")) {
            // WHOX gets special treatment as the nick field isn't in a static position.
            if (whox_index == -1) {
                return NULL;    // No nick field.
            }

            nick_index = whox_index + 1;
        }

        if (msg.GetParams().size() <= nick_index) {
            return NULL;    // Not enough params.
        }

        return ServerInstance->FindNickOnly(msg.GetParams()[nick_index]);
    }

  public:
    CustomTagsExtItem ext;
    SpecialMessageMap specialmsgs;
    std::string vendor;
    int whox_index;

    CustomTags(Module* mod)
        : ClientProtocol::MessageTagProvider(mod)
        , ctctagcap(mod)
        , ext(mod, ctctagcap)
        , whox_index(-1) {
    }

    void OnPopulateTags(ClientProtocol::Message& msg) CXX11_OVERRIDE {
        User* user = msg.GetSourceUser();
        if (!user || IS_SERVER(user)) {
            user = UserFromMsg(msg);
            if (!user) {
                return;    // No such user.
            }
        }

        CustomTagMap* tags = ext.get(user);
        if (!tags) {
            return;
        }

        for (CustomTagMap::const_iterator iter = tags->begin(); iter != tags->end(); ++iter) {
            msg.AddTag(vendor + iter->first, this, iter->second);
        }
    }

    ModResult OnProcessTag(User* user, const std::string& tagname,
                           std::string& tagvalue) CXX11_OVERRIDE {
        // Check that the tag begins with the customtags vendor prefix.
        if (irc::find(tagname, vendor) == 0) {
            return MOD_RES_ALLOW;
        }

        return MOD_RES_PASSTHRU;
    }

    bool ShouldSendTag(LocalUser* user,
                       const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE {
        return ctctagcap.get(user);
    }
};

class ModuleCustomTags CXX11_FINAL
    : public Module
    , public CTCTags::EventListener
    , public Who::EventListener {
  private:
    CustomTags ctags;

    ModResult AddCustomTags(User* user, ClientProtocol::TagMap& tags) {
        CustomTagMap* tagmap = ctags.ext.get(user);
        if (!tagmap) {
            return MOD_RES_PASSTHRU;
        }

        for (CustomTagMap::const_iterator iter = tagmap->begin(); iter != tagmap->end();
                ++iter) {
            tags.insert(std::make_pair(ctags.vendor + iter->first,
                                       ClientProtocol::MessageTagData(&ctags, iter->second)));
        }
        return MOD_RES_PASSTHRU;
    }

  public:
    ModuleCustomTags()
        : CTCTags::EventListener(this)
        , Who::EventListener(this)
        , ctags(this) {
    }

    ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user,
                        Membership* memb, Numeric::Numeric& numeric) CXX11_OVERRIDE {
        size_t nick_index;
        ctags.whox_index = request.GetFieldIndex('n', nick_index) ? nick_index : -1;
        return MOD_RES_PASSTHRU;
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        return AddCustomTags(user, details.tags_out);
    }

    ModResult OnUserPreTagMessage(User* user, const MessageTarget& target,
                                  CTCTags::TagMessageDetails& details) CXX11_OVERRIDE {
        return AddCustomTags(user, details.tags_out);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        SpecialMessageMap specialmsgs;
        ConfigTagList tags = ServerInstance->Config->ConfTags("specialmsg");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;

            const std::string command = tag->getString("command");
            if (command.empty()) {
                throw ModuleException("<specialmsg:command> must be a S2C command name!");
            }

            specialmsgs[command] = tag->getUInt("index", 0, 0, 20);
        }
        std::swap(specialmsgs, ctags.specialmsgs);

        ConfigTag* tag = ServerInstance->Config->ConfValue("customtags");
        ctags.ext.broadcastchanges = tag->getBool("broadcastchanges");
        ctags.vendor = tag->getString("vendor", ServerInstance->Config->ServerName, 1) + "/";
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows services to add custom tags to messages sent by clients");
    }
};

MODULE_INIT(ModuleCustomTags)
