/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
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
#include "modules/ctctags.h"

class MsgIdTag final
	: public CTCTags::TagProvider
{
public:
	MsgIdTag(Module* mod)
		: CTCTags::TagProvider(mod)
	{
	}

	ModResult OnProcessTag(User* user, const std::string& tagname, std::string& tagvalue) override
	{
		if (!irc::equals(tagname, "msgid"))
			return MOD_RES_PASSTHRU;

		// We should only allow this tag if it is added by a remote server.
		return IS_LOCAL(user) ? MOD_RES_DENY : MOD_RES_ALLOW;
	}
};

class MsgIdGenerator final
{
	uint64_t counter = 0;
	std::string strid;
	const std::string::size_type baselen;

public:
	MsgIdGenerator()
		: strid(FMT::format("{}~{}~", ServerInstance->Config->ServerId, ServerInstance->startup_time))
		, baselen(strid.length())
	{
	}

	const std::string& GetNext()
	{
		strid.erase(baselen);
		strid.append(ConvToStr(counter++));
		return strid;
	}
};

class ModuleMsgId final
	: public Module
	, public CTCTags::EventListener
{
private:
	MsgIdTag tag;
	MsgIdGenerator generator;

	ModResult CopyMessageId(const ClientProtocol::TagMap& tags_in, ClientProtocol::TagMap& tags_out)
	{
		ClientProtocol::TagMap::const_iterator iter = tags_in.find("msgid");
		if (iter != tags_in.end())
		{
			// If the remote server has sent a message identifier we should use that as
			// identifiers need to be the same on all sides of the network.
			tags_out.insert(*iter);
			return MOD_RES_PASSTHRU;
		}

		// Otherwise, we can just create a new message identifier.
		tags_out.emplace("msgid", ClientProtocol::MessageTagData(&tag, generator.GetNext()));
		return MOD_RES_PASSTHRU;
	}

public:
	ModuleMsgId()
		: Module(VF_VENDOR, "Provides support for the IRCv3 Message IDs specification.")
		, CTCTags::EventListener(this)
		, tag(this)
	{
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		return CopyMessageId(details.tags_in, details.tags_out);
	}

	ModResult OnUserPreTagMessage(User* user, MessageTarget& target, CTCTags::TagMessageDetails& details) override
	{
		return CopyMessageId(details.tags_in, details.tags_out);
	}
};

MODULE_INIT(ModuleMsgId)
