/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

#include "commands.h"
#include "treeserver.h"
#include "utils.h"
#include "main.h"

class RemoteOperAccount final
	: public OperAccount
{
private:
	static void ReadModes(ModeParser::ModeStatus& modes, const ClientProtocol::TagMap& tags, const std::string& tag)
	{
		auto it = tags.find(tag);
		if (it != tags.end())
		{
			for (const auto chr : ClientProtocol::Message::UnescapeTag(it->second.value))
			{
				size_t idx = ModeParser::GetModeIndex(chr);
				if (idx != ModeParser::MODEID_MAX)
					modes.set(idx);
			}
			return;
		}

		// Probably a legacy server.
		modes.set();
	}

	static void ReadTokens(TokenList& tokens, const ClientProtocol::TagMap& tags, const std::string& tag)
	{
		auto it = tags.find(tag);
		if (it != tags.end())
		{
			tokens.AddList(ClientProtocol::Message::UnescapeTag(it->second.value));
			return;
		}

		// Probably a legacy server.
		tokens.Add("*");
	}

public:
	RemoteOperAccount(const std::string& n, const ClientProtocol::TagMap& tags)
		: OperAccount(n, nullptr, ServerInstance->Config->EmptyTag)
	{
		auto it = tags.find("~name");
		if (it != tags.end())
			name = ClientProtocol::Message::UnescapeTag(it->second.value);

		ReadModes(chanmodes, tags, "~chanmodes");
		ReadModes(usermodes, tags, "~usermodes");
		ReadModes(snomasks, tags, "~snomasks");

		ReadTokens(commands, tags, "~commands");
		ReadTokens(privileges, tags, "~privileges");
	}
};

CmdResult CommandOpertype::HandleRemote(RemoteUser* u, CommandBase::Params& params)
{
	// Remote servers might be using entirely different oper privileges to us
	// so instead of looking up the remote oper type we just create a new tag
	// with the details sent by the remote. For legacy servers that don't send
	// the oper details we instead just assume they have access to everything
	// as was the default until 1206.
	bool automatic = params.GetTags().find("~automatic") != params.GetTags().end();
	u->OperLogin(std::make_shared<RemoteOperAccount>(params.back(), params.GetTags()), automatic, true);

	if (Utils->quiet_bursts)
	{
		/*
		 * If quiet bursts are enabled, and server is bursting or a silent services server
		 * then do nothing. -- w00t
		 */
		TreeServer* remoteserver = TreeServer::Get(u);
		if (remoteserver->IsBehindBursting() || remoteserver->IsSilentService())
			return CmdResult::SUCCESS;
	}

	std::string extra;
	if (params.GetTags().find("~name") != params.GetTags().end())
	{
		extra += fmt::format(" ({}using account \x02{}\x02)", automatic ? "automatically " : "",
			u->oper->GetName());
	}

	ServerInstance->SNO.WriteToSnoMask('O', "From {}: {} ({}) [{}] is now a server operator of type \x02{}\x02{}.",
		u->server->GetName(), u->nick, u->GetRealUserHost(), u->GetAddress(), u->oper->GetType(), extra);
	return CmdResult::SUCCESS;
}

CommandOpertype::Builder::Builder(User* user, const std::shared_ptr<OperAccount>& oper, bool automatic)
	: CmdBuilder(user, "OPERTYPE")
{
	push_tags({
		{ "~name",       { &Utils->Creator->servertags, ClientProtocol::Message::EscapeTag(oper->GetName())                        } },
		{ "~chanmodes",  { &Utils->Creator->servertags, ClientProtocol::Message::EscapeTag(oper->GetModes(MODETYPE_CHANNEL, true)) } },
		{ "~usermodes",  { &Utils->Creator->servertags, ClientProtocol::Message::EscapeTag(oper->GetModes(MODETYPE_USER, true))    } },
		{ "~snomasks",   { &Utils->Creator->servertags, ClientProtocol::Message::EscapeTag(oper->GetSnomasks(true))                } },
		{ "~commands",   { &Utils->Creator->servertags, ClientProtocol::Message::EscapeTag(oper->GetCommands(true))                } },
		{ "~privileges", { &Utils->Creator->servertags, ClientProtocol::Message::EscapeTag(oper->GetPrivileges())                  } },
	});

	if (automatic)
	{
		push_tags({
			{ "~automatic", { &Utils->Creator->servertags, "" } },
		});
	}
	push_last(oper->GetType());
}
