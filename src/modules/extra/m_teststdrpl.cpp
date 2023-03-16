/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015-2016 Sadie Powell <sadie@witchery.services>
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
/// $ModDesc: Adds the STDRPL command for testing client standard reply implementations.


#include "inspircd.h"
#include "modules/ircv3_replies.h"

class CommandStdRpl : public SplitCommand
{
 private:
	IRCv3::Replies::Fail failrpl;
	IRCv3::Replies::Warn warnrpl;
	IRCv3::Replies::Note noterpl;

 public:
	CommandStdRpl(Module* Creator)
		: SplitCommand(Creator, "STDRPL")
		, failrpl(Creator)
		, warnrpl(Creator)
		, noterpl(Creator)
	{
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		failrpl.Send(user, NULL, "EXAMPLE", "FAIL with no command name.");
		warnrpl.Send(user, NULL, "EXAMPLE", "WARN with no command name.");
		noterpl.Send(user, NULL, "EXAMPLE", "NOTE with a command name.");

		failrpl.Send(user, this, "EXAMPLE", "FAIL with a command name.");
		warnrpl.Send(user, this, "EXAMPLE", "FAIL with a command name.");
		noterpl.Send(user, this, "EXAMPLE", "NOTE with a command name.");

		failrpl.Send(user, this, "EXAMPLE", 123, "FAIL with variable parameters.");
		warnrpl.Send(user, this, "EXAMPLE", 123, "FAIL with variable parameters.");
		noterpl.Send(user, this, "EXAMPLE", 123, "NOTE with variable parameters.");

		return CMD_SUCCESS;
	}
};

class ModuleStdRpl : public Module
{
 private:
	CommandStdRpl cmd;

 public:
	ModuleStdRpl()
		: cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds the STDRPL command for testing client standard reply implementations.");
	}
};

MODULE_INIT(ModuleStdRpl)
