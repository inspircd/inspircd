/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

enum
{
	// From RFC 1459.
	RPL_LUSERCLIENT = 251,
	RPL_LUSERUNKNOWN = 253,
	RPL_LUSERCHANNELS = 254,
	RPL_LUSERME = 255,

	// From ircd-ratbox?
	RPL_LOCALUSERS = 265,
	RPL_GLOBALUSERS = 266,
};

struct LusersCounters final
{
	size_t max_local;
	size_t max_global;
	size_t invisible = 0;

	LusersCounters(UserModeReference& invisiblemode)
		: max_local(ServerInstance->Users.LocalUserCount())
		, max_global(ServerInstance->Users.GlobalUserCount())
	{
		for (const auto& [_, u] : ServerInstance->Users.GetUsers())
		{
			if (!u->server->IsService() && u->IsModeSet(invisiblemode))
				invisible++;
		}
	}

	inline void UpdateMaxUsers()
	{
		size_t current = ServerInstance->Users.LocalUserCount();
		if (current > max_local)
			max_local = current;

		current = ServerInstance->Users.GlobalUserCount();
		if (current > max_global)
			max_global = current;
	}
};

class CommandLusers final
	: public Command
{
private:
	LusersCounters& counters;

public:
	CommandLusers(Module* parent, LusersCounters& Counters)
		: Command(parent, "LUSERS")
		, counters(Counters)
	{
	}

	CmdResult Handle(User* user, const Params& parameters) override;
};

CmdResult CommandLusers::Handle(User* user, const Params& parameters)
{
	size_t n_users = ServerInstance->Users.GlobalUserCount();
	ProtocolInterface::ServerList serverlist;
	ServerInstance->PI->GetServerList(serverlist);
	size_t n_serv = serverlist.size();
	size_t n_local_servs = 0;

	for (const auto& server : serverlist)
	{
		if (server.parentname == ServerInstance->Config->ServerName)
			n_local_servs++;
	}
	// fix for default GetServerList not returning us
	if (!n_serv)
		n_serv = 1;

	counters.UpdateMaxUsers();

	user->WriteNumeric(RPL_LUSERCLIENT, fmt::format("There are {} users and {} invisible on {} servers",
			n_users - counters.invisible, counters.invisible, n_serv));

	size_t opercount = ServerInstance->Users.all_opers.size();
	if (opercount)
		user->WriteNumeric(RPL_LUSEROP, opercount, "operator(s) online");

	if (ServerInstance->Users.UnknownUserCount())
		user->WriteNumeric(RPL_LUSERUNKNOWN, ServerInstance->Users.UnknownUserCount(), "unknown connections");

	user->WriteNumeric(RPL_LUSERCHANNELS, ServerInstance->Channels.GetChans().size(), "channels formed");
	user->WriteNumeric(RPL_LUSERME, fmt::format("I have {} clients and {} servers", ServerInstance->Users.LocalUserCount(), n_local_servs));
	user->WriteNumeric(RPL_LOCALUSERS, fmt::format("Current local users: {}  Max: {}", ServerInstance->Users.LocalUserCount(), counters.max_local));
	user->WriteNumeric(RPL_GLOBALUSERS, fmt::format("Current global users: {}  Max: {}", n_users, counters.max_global));
	return CmdResult::SUCCESS;
}

class InvisibleWatcher final
	: public ModeWatcher
{
	size_t& invisible;
public:
	InvisibleWatcher(Module* mod, size_t& Invisible)
		: ModeWatcher(mod, "invisible", MODETYPE_USER)
		, invisible(Invisible)
	{
	}

	void AfterMode(User* source, User* dest, Channel* channel, const Modes::Change& change) override
	{
		if (!dest->IsFullyConnected())
			return;

		if (dest->server->IsService())
			return;

		if (change.adding)
			invisible++;
		else
			invisible--;
	}
};

class ModuleLusers final
	: public Module
{
	UserModeReference invisiblemode;
	LusersCounters counters;
	CommandLusers cmd;
	InvisibleWatcher mw;

public:
	ModuleLusers()
		: Module(VF_CORE | VF_VENDOR, "Provides the LUSERS command")
		, invisiblemode(this, "invisible")
		, counters(invisiblemode)
		, cmd(this, counters)
		, mw(this, counters.invisible)
	{
	}

	void OnPostConnect(User* user) override
	{
		counters.UpdateMaxUsers();
		if (!user->server->IsService() && user->IsModeSet(invisiblemode))
			counters.invisible++;
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) override
	{
		if (!user->server->IsService() && user->IsModeSet(invisiblemode))
			counters.invisible--;
	}
};

MODULE_INIT(ModuleLusers)
