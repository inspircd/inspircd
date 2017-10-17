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
#include "m_cap.h"

/* $ModDesc: Provides the CAP negotiation mechanism seen in ratbox-derived ircds */

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
 public:
	LocalIntExt reghold;
	CommandCAP (Module* mod) : Command(mod, "CAP", 1),
		reghold("CAP_REGHOLD", mod)
	{
		works_before_reg = true;
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		irc::string subcommand = parameters[0].c_str();

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
				// Whilst the handling of extraneous spaces is not currently defined in the CAP specification
				// every single other implementation ignores extraneous spaces. Lets copy them for
				// compatibility purposes.
				trim(cap_);
				if (!cap_.empty())
					Data.wanted.push_back(cap_);
			}

			reghold.set(user, 1);
			Data.Send();

			if (Data.wanted.empty())
			{
				user->WriteServ("CAP %s ACK :%s", user->nick.c_str(), parameters[1].c_str());
				return CMD_SUCCESS;
			}

			// HACK: reset all of the caps which were enabled on this user because a cap request is atomic.
			for (std::vector<std::pair<GenericCap*, int> >::iterator iter = Data.changed.begin(); iter != Data.changed.end(); ++iter)
				iter->first->ext.set(user, iter->second);

			user->WriteServ("CAP %s NAK :%s", user->nick.c_str(), parameters[1].c_str());
		}
		else if (subcommand == "END")
		{
			reghold.set(user, 0);
		}
		else if ((subcommand == "LS") || (subcommand == "LIST"))
		{
			CapEvent Data(creator, user, subcommand == "LS" ? CapEvent::CAPEVENT_LS : CapEvent::CAPEVENT_LIST);

			reghold.set(user, 1);
			Data.Send();

			std::string Result;
			if (Data.wanted.size() > 0)
				Result = irc::stringjoiner(" ", Data.wanted, 0, Data.wanted.size() - 1).GetJoined();

			user->WriteServ("CAP %s %s :%s", user->nick.c_str(), subcommand.c_str(), Result.c_str());
		}
		else if (subcommand == "CLEAR")
		{
			CapEvent Data(creator, user, CapEvent::CAPEVENT_CLEAR);

			reghold.set(user, 1);
			Data.Send();

			std::string Result;
			if (!Data.ack.empty())
				Result = irc::stringjoiner(" ", Data.ack, 0, Data.ack.size() - 1).GetJoined();
			user->WriteServ("CAP %s ACK :%s", user->nick.c_str(), Result.c_str());
		}
		else
		{
			user->WriteNumeric(ERR_INVALIDCAPSUBCOMMAND, "%s %s :Invalid CAP subcommand", user->nick.c_str(), subcommand.empty() ? "*" : subcommand.c_str());
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

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		ServerInstance->Modules->AddService(cmd.reghold);

		Implementation eventlist[] = { I_OnCheckReady };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		/* Users in CAP state get held until CAP END */
		if (cmd.reghold.get(user))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	~ModuleCAP()
	{
	}

	Version GetVersion()
	{
		return Version("Client CAP extension support", VF_VENDOR);
	}
};

MODULE_INIT(ModuleCAP)

