/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2024 Sadie Powell <sadie@witchery.services>
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
#include "modules/server.h"

enum
{
	// From ircu.
	RPL_STATSCONN = 250,

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
	size_t max_conns;
	size_t invisible = 0;
	size_t global_servers;
	size_t local_servers;

	LusersCounters(UserModeReference& invisiblemode)
		: max_local(ServerInstance->Users.LocalUserCount())
		, max_global(ServerInstance->Users.GlobalUserCount())
	{
		for (const auto& [_, u] : ServerInstance->Users.GetUsers())
		{
			if (u->IsModeSet(invisiblemode))
				invisible++;
		}
		UpdateServerCount();
		max_conns = local_servers + max_local;
	}

	inline void UpdateMaxUsers()
	{
		size_t current = ServerInstance->Users.LocalUserCount();
		if (current > max_local)
			max_local = current;

		current = ServerInstance->Users.GlobalUserCount();
		if (current > max_global)
			max_global = current;

		current = local_servers + max_local;
		if (current > max_conns)
			max_conns = current;
	}

	inline void UpdateServerCount()
	{
		// XXX: Currently we don't have a way to find out whether a
		// server is directly connected from a Server object so we
		// have to do this via the protocol interface.
		ProtocolInterface::ServerList serverlist;
		ServerInstance->PI->GetServerList(serverlist);

		// If spanningtree is not loaded GetServerList does nothing.
		global_servers = std::max<size_t>(serverlist.size(), 1);
		local_servers = 0;

		for (const auto& server : serverlist)
		{
			if (server.parentname == ServerInstance->Config->ServerName)
				local_servers++;
		}
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
	counters.UpdateMaxUsers();

	user->WriteNumeric(RPL_LUSERCLIENT, INSP_FORMAT("There are {} users and {} invisible on {} servers",
			ServerInstance->Users.GlobalUserCount() - counters.invisible, counters.invisible,
			counters.global_servers));

	size_t opercount = ServerInstance->Users.all_opers.size();
	if (opercount)
		user->WriteNumeric(RPL_LUSEROP, opercount, "operator(s) online");

	if (ServerInstance->Users.UnknownUserCount())
		user->WriteNumeric(RPL_LUSERUNKNOWN, ServerInstance->Users.UnknownUserCount(), "unknown connections");

	user->WriteNumeric(RPL_LUSERCHANNELS, ServerInstance->Channels.GetChans().size(), "channels formed");
	user->WriteNumeric(RPL_LUSERME, INSP_FORMAT("I have {} clients and {} servers", ServerInstance->Users.LocalUserCount(), counters.local_servers));
	user->WriteNumeric(RPL_LOCALUSERS, INSP_FORMAT("Current local users: {}  Max: {}", ServerInstance->Users.LocalUserCount(), counters.max_local));
	user->WriteNumeric(RPL_GLOBALUSERS, INSP_FORMAT("Current global users: {}  Max: {}", ServerInstance->Users.GlobalUserCount(), counters.max_global));
	user->WriteNumeric(RPL_STATSCONN, INSP_FORMAT("Highest connection count: {} ({} clients) ({} connections received)", counters.max_conns, counters.max_local, ServerInstance->Stats.Connects));
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

		if (change.adding)
			invisible++;
		else
			invisible--;
	}
};

class ModuleLusers final
	: public Module
	, public ServerProtocol::LinkEventListener
{
	UserModeReference invisiblemode;
	LusersCounters counters;
	CommandLusers cmd;
	InvisibleWatcher mw;

public:
	ModuleLusers()
		: Module(VF_CORE | VF_VENDOR, "Provides the LUSERS command")
		, ServerProtocol::LinkEventListener(this)
		, invisiblemode(this, "invisible")
		, counters(invisiblemode)
		, cmd(this, counters)
		, mw(this, counters.invisible)
	{
	}

	void OnPostConnect(User* user) override
	{
		counters.UpdateMaxUsers();
		if (user->IsModeSet(invisiblemode))
			counters.invisible++;
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) override
	{
		if (user->IsModeSet(invisiblemode))
			counters.invisible--;
	}

	void OnServerLink(const Server* server) override
	{
		counters.UpdateServerCount();
	}

	void OnServerSplit(const Server* server, bool error) override
	{
		counters.UpdateServerCount();
	}
};

MODULE_INIT(ModuleLusers)
