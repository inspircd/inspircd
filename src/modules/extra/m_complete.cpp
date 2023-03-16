/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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
/// $ModConfig: <complete maxsuggestions="10" minlength="3">
/// $ModDepends: core 3
/// $ModDesc: Allows clients to automatically complete commands.


#include "inspircd.h"
#include "modules/ircv3_replies.h"

class CommandComplete : public SplitCommand
{
 private:
	Cap::Reference cap;
	ClientProtocol::EventProvider evprov;
	IRCv3::Replies::Fail failrpl;

 public:
	size_t maxsuggestions;
	size_t minlength;

	CommandComplete(Module* Creator)
		: SplitCommand(Creator, "COMPLETE", 1)
		, cap(Creator, "labeled-response")
		, evprov(Creator, "COMPLETE")
		, failrpl(Creator)
	{
		allow_empty_last_param = false;
		Penalty = 3;
		syntax = "<partial-command> [<max>]";
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (!cap.get(user))
			return CMD_FAILURE;

		if (parameters[0].length() < minlength)
		{
			failrpl.Send(user, this, "NEED_MORE_CHARS", parameters[0], minlength, "You must specify more characters to complete.");
			return CMD_FAILURE;
		}

		size_t max = SIZE_MAX;
		if (parameters.size() > 1)
		{
			max = ConvToNum<size_t>(parameters[1]);
			if (!max || max > maxsuggestions)
			{
				failrpl.Send(user, this, "INVALID_MAX_SUGGESTIONS", parameters[1], maxsuggestions, "The number of suggestions you requested was invalid.");
				return CMD_FAILURE;
			}
		}

		size_t maxsent = 0;
		const CommandParser::CommandMap& commands = ServerInstance->Parser.GetCommands();
		for (CommandParser::CommandMap::const_iterator iter = commands.begin(); iter != commands.end(); ++iter)
		{
			if (!irc::find(iter->first, parameters[0]))
			{
				ClientProtocol::Message msg("COMPLETE");
				msg.PushParamRef(iter->second->name);
				msg.PushParamRef(iter->second->syntax);
				ClientProtocol::Event ev(evprov, msg);
				user->Send(ev);
				maxsent++;
			}

			if (maxsent > maxsuggestions)
				break;
		}
		return CMD_SUCCESS;
	}
};

class ModuleComplete : public Module
{
 private:
	CommandComplete cmd;

 public:
	ModuleComplete()
		: cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("complete");
		cmd.maxsuggestions = tag->getUInt("maxsuggestions", 10, 1);
		cmd.minlength = tag->getUInt("minlength", 3, 1);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows clients to automatically complete commands.");
	}
};

MODULE_INIT(ModuleComplete)
