/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2017-2019 Sadie Powell <sadie@witchery.services>
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


/** Handle /CONNECT.
 */
class CommandConnect : public Command
{
 public:
	/** Constructor for connect.
	 */
	CommandConnect(Module* parent)
		: Command(parent, "CONNECT", 1)
	{
		flags_needed = 'o';
		syntax = "<servermask>";
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		/*
		 * This is handled by the server linking module, if necessary. Do not remove this stub.
		 */
		user->WriteNotice("Look into loading a linking module (like m_spanningtree) if you want this to do anything useful.");
		return CMD_SUCCESS;
	}
};

/** Handle /LINKS.
 */
class CommandLinks : public Command
{
 public:
	/** Constructor for links.
	 */
	CommandLinks(Module* parent)
		: Command(parent, "LINKS", 0, 0)
	{
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		user->WriteNumeric(RPL_LINKS, ServerInstance->Config->ServerName, ServerInstance->Config->ServerName, InspIRCd::Format("0 %s", ServerInstance->Config->ServerDesc.c_str()));
		user->WriteNumeric(RPL_ENDOFLINKS, '*', "End of /LINKS list.");
		return CMD_SUCCESS;
	}
};

/** Handle /SERVER.
 */
class CommandServer : public Command
{
 public:
	/** Constructor for server.
	 */
	CommandServer(Module* parent)
		: Command(parent, "SERVER")
	{
		works_before_reg = true;
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (user->registered == REG_ALL)
		{
			user->WriteNumeric(ERR_ALREADYREGISTERED, "You are already registered. (Perhaps your IRC client does not have a /SERVER command).");
		}
		else
		{
			user->WriteNumeric(ERR_NOTREGISTERED, "SERVER", "You may not register as a server (servers have separate ports from clients, change your config)");
		}
		return CMD_FAILURE;
	}
};

/** Handle /SQUIT.
 */
class CommandSquit : public Command
{
 public:
	/** Constructor for squit.
	 */
	CommandSquit(Module* parent)
		: Command(parent, "SQUIT", 1, 2)
	{
		flags_needed = 'o';
		syntax = "<servermask>";
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		user->WriteNotice("Look into loading a linking module (like m_spanningtree) if you want this to do anything useful.");
		return CMD_FAILURE;
	}
};

class CommandSummon
	: public SplitCommand
{
 public:
	CommandSummon(Module* Creator)
		: SplitCommand(Creator, "SUMMON", 1)
	{
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		user->WriteNumeric(ERR_SUMMONDISABLED, "SUMMON has been disabled");
		return CMD_SUCCESS;
	}
};

class CommandUsers
	: public SplitCommand
{
 public:
	CommandUsers(Module* Creator)
		: SplitCommand(Creator, "USERS")
	{
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		user->WriteNumeric(ERR_USERSDISABLED, "USERS has been disabled");
		return CMD_SUCCESS;
	}
};

class CoreModStub : public Module
{
	CommandConnect cmdconnect;
	CommandLinks cmdlinks;
	CommandServer cmdserver;
	CommandSquit cmdsquit;
	CommandSummon cmdsummon;
	CommandUsers cmdusers;

 public:
	CoreModStub()
		: cmdconnect(this)
		, cmdlinks(this)
		, cmdserver(this)
		, cmdsquit(this)
		, cmdsummon(this)
		, cmdusers(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides stubs for unimplemented commands", VF_VENDOR|VF_CORE);
	}
};

MODULE_INIT(CoreModStub)
