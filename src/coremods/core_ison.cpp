/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/** Handle /ISON.
 */
class CommandIson : public SplitCommand
{
 public:
	/** Constructor for ison.
	 */
	CommandIson(Module* parent)
		: SplitCommand(parent, "ISON", 1)
	{
		syntax = "<nick> [<nick>]+";
	}
	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override;
};

class IsonReplyBuilder : public Numeric::Builder<' ', true>
{
 public:
	IsonReplyBuilder(LocalUser* user)
		: Numeric::Builder<' ', true>(user, RPL_ISON)
	{
	}

	void AddNick(const std::string& nickname)
	{
		User* const user = ServerInstance->FindNickOnly(nickname);
		if ((user) && (user->registered == REG_ALL))
			Add(user->nick);
	}
};

/** Handle /ISON
 */
CmdResult CommandIson::HandleLocal(LocalUser* user, const Params& parameters)
{
	IsonReplyBuilder reply(user);

	for (std::vector<std::string>::const_iterator i = parameters.begin(); i != parameters.end()-1; ++i)
	{
		const std::string& targetstr = *i;
		reply.AddNick(targetstr);
	}

	// Last parameter can be a space separated list
	irc::spacesepstream ss(parameters.back());
	for (std::string token; ss.GetToken(token); )
		reply.AddNick(token);

	reply.Flush();
	return CMD_SUCCESS;
}

class CoreModIson : public Module
{
 private:
	CommandIson cmd;

 public:
	CoreModIson()
		: cmd(this)
	{
	}

	Version GetVersion() override
	{
		return Version("Provides the ISON command", VF_CORE | VF_VENDOR);
	}
};

MODULE_INIT(CoreModIson)
