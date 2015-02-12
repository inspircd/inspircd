/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include "modules/cap.h"

/*
CAP LS
:alfred.staticbox.net CAP * LS :multi-prefix sasl
CAP REQ :multi-prefix
:alfred.staticbox.net CAP * ACK :multi-prefix
CAP CLEAR
:alfred.staticbox.net CAP * ACK :-multi-prefix
CAP REQ :multi-prefix
:alfred.staticbox.net CAP * ACK :multi-prefix
CAP LIST
:alfred.staticbox.net CAP * LIST :multi-prefix
CAP END
*/

/** Handle /CAP
 */
class CommandCAP : public Command
{
	Events::ModuleEventProvider capevprov;

 public:
	LocalIntExt reghold;
	CommandCAP (Module* mod) : Command(mod, "CAP", 1),
		capevprov(mod, "event/cap"),
		reghold("CAP_REGHOLD", ExtensionItem::EXT_USER, mod)
	{
		works_before_reg = true;
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string subcommand(parameters[0].length(), ' ');
		std::transform(parameters[0].begin(), parameters[0].end(), subcommand.begin(), ::toupper);

		if (subcommand == "REQ")
		{
			if (parameters.size() < 2)
				return CMD_FAILURE;

			CapEvent Data(creator, user, CapEvent::CAPEVENT_REQ);

			// tokenize the input into a nice list of requested caps
			std::string cap_;
			irc::spacesepstream cap_stream(parameters[1]);

			while (cap_stream.GetToken(cap_))
			{
				std::transform(cap_.begin(), cap_.end(), cap_.begin(), ::tolower);
				Data.wanted.push_back(cap_);
			}

			reghold.set(user, 1);
			FOREACH_MOD_CUSTOM(capevprov, GenericCap, OnCapEvent, (Data));

			if (Data.ack.size() > 0)
			{
				std::string AckResult = irc::stringjoiner(Data.ack);
				user->WriteCommand("CAP", "ACK :" + AckResult);
			}

			if (Data.wanted.size() > 0)
			{
				std::string NakResult = irc::stringjoiner(Data.wanted);
				user->WriteCommand("CAP", "NAK :" + NakResult);
			}
		}
		else if (subcommand == "END")
		{
			reghold.set(user, 0);
		}
		else if ((subcommand == "LS") || (subcommand == "LIST"))
		{
			CapEvent Data(creator, user, subcommand == "LS" ? CapEvent::CAPEVENT_LS : CapEvent::CAPEVENT_LIST);

			reghold.set(user, 1);
			FOREACH_MOD_CUSTOM(capevprov, GenericCap, OnCapEvent, (Data));

			std::string Result = irc::stringjoiner(Data.wanted);
			user->WriteCommand("CAP", subcommand + " :" + Result);
		}
		else if (subcommand == "CLEAR")
		{
			CapEvent Data(creator, user, CapEvent::CAPEVENT_CLEAR);

			reghold.set(user, 1);
			FOREACH_MOD_CUSTOM(capevprov, GenericCap, OnCapEvent, (Data));

			std::string Result = irc::stringjoiner(Data.ack);
			user->WriteCommand("CAP", "ACK :" + Result);
		}
		else
		{
			user->WriteNumeric(ERR_INVALIDCAPSUBCOMMAND, "%s :Invalid CAP subcommand", subcommand.c_str());
			return CMD_FAILURE;
		}

		return CMD_SUCCESS;
	}
};

class ModuleCAP : public Module
{
	CommandCAP cmd;
 public:
	ModuleCAP()
		: cmd(this)
	{
	}

	ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE
	{
		/* Users in CAP state get held until CAP END */
		if (cmd.reghold.get(user))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Client CAP extension support", VF_VENDOR);
	}
};

MODULE_INIT(ModuleCAP)
