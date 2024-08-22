/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2016 Attila Molnar <attilamolnar@hush.com>
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
	ERR_SUMMONDISABLED = 445,
	ERR_USERSDISABLED = 446
};

class CommandCapab final
	: public Command
{
public:
	CommandCapab(Module* parent)
		: Command(parent, "CAPAB")
	{
		works_before_reg = true;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (user->connected == User::CONN_NONE)
		{
			// The CAPAB command is used in the server protocol for negotiating
			// the protocol version when initiating a server connection. There
			// is no legitimate reason for a user to send this so we disconnect
			// users who sent it in order to help out server admins who have
			// misconfigured their server.
			ServerInstance->Users.QuitUser(user, "You can not connect a server to a client port. Read " INSPIRCD_DOCS "modules/spanningtree for docs on how to link a server.");
		}
		return CmdResult::FAILURE;
	}
};

class CommandConnect final
	: public Command
{
public:
	CommandConnect(Module* parent)
		: Command(parent, "CONNECT", 1)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<servermask>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		/*
		 * This is handled by the server linking module, if necessary. Do not remove this stub.
		 */
		user->WriteNotice("Look into loading a linking module (like m_spanningtree) if you want this to do anything useful.");
		return CmdResult::SUCCESS;
	}
};

class CommandLinks final
	: public Command
{
public:
	CommandLinks(Module* parent)
		: Command(parent, "LINKS")
	{
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		user->WriteNumeric(RPL_LINKS, ServerInstance->Config->GetServerName(), ServerInstance->Config->GetServerName(), FMT::format("0 {}", ServerInstance->Config->GetServerDesc()));
		user->WriteNumeric(RPL_ENDOFLINKS, '*', "End of /LINKS list.");
		return CmdResult::SUCCESS;
	}
};

class CommandSquit final
	: public Command
{
public:
	CommandSquit(Module* parent)
		: Command(parent, "SQUIT", 1, 2)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<servermask>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		user->WriteNotice("Look into loading a linking module (like m_spanningtree) if you want this to do anything useful.");
		return CmdResult::FAILURE;
	}
};

class CommandSummon final
	: public SplitCommand
{
public:
	CommandSummon(Module* Creator)
		: SplitCommand(Creator, "SUMMON", 1)
	{
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		user->WriteNumeric(ERR_SUMMONDISABLED, "SUMMON has been disabled");
		return CmdResult::SUCCESS;
	}
};

class CommandUsers final
	: public SplitCommand
{
public:
	CommandUsers(Module* Creator)
		: SplitCommand(Creator, "USERS")
	{
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		user->WriteNumeric(ERR_USERSDISABLED, "USERS has been disabled");
		return CmdResult::SUCCESS;
	}
};

class CoreModStub final
	: public Module
{
private:
	CommandCapab cmdcapab;
	CommandConnect cmdconnect;
	CommandLinks cmdlinks;
	CommandSquit cmdsquit;
	CommandSummon cmdsummon;
	CommandUsers cmdusers;

public:
	CoreModStub()
		: Module(VF_CORE | VF_VENDOR, "Provides stubs for unimplemented commands")
		, cmdcapab(this)
		, cmdconnect(this)
		, cmdlinks(this)
		, cmdsquit(this)
		, cmdsummon(this)
		, cmdusers(this)
	{
	}
};

MODULE_INIT(CoreModStub)
