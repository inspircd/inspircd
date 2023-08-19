/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Sadie Powell <sadie@witchery.services>
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
#include "modules/account.h"
#include "modules/ctctags.h"
#include "modules/stats.h"
#include "timeutils.h"
#include "xline.h"

enum
{
	// From UnrealIRCd.
	ERR_KILLDENY = 485
};

class RegisteredChannel final
	: public SimpleChannelMode
{
public:
	RegisteredChannel(Module* Creator)
		: SimpleChannelMode(Creator, "c_registered", 'r')
	{
		if (ServerInstance->Config->ConfValue("servicesintegration")->getBool("disablemodes"))
			DisableAutoRegister();
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (IS_LOCAL(source))
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "Only a server may modify the +r channel mode");
			return false;
		}

		return SimpleChannelMode::OnModeChange(source, dest, channel, change);
	}
};

class RegisteredUser final
	: public SimpleUserMode
{

public:
	RegisteredUser(Module* Creator)
		: SimpleUserMode(Creator, "u_registered", 'r')
	{
		if (ServerInstance->Config->ConfValue("servicesintegration")->getBool("disablemodes"))
			DisableAutoRegister();
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (IS_LOCAL(source))
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "Only a server may modify the +r user mode");
			return false;
		}

		return SimpleUserMode::OnModeChange(source, dest, channel, change);
	}
};

class ServiceTag final
	: public CTCTags::TagProvider
{
public:
	ServiceTag(Module* mod)
		: CTCTags::TagProvider(mod)
	{
	}

	void OnPopulateTags(ClientProtocol::Message& msg) override
	{
		const auto* user = msg.GetSourceUser();
		if (user && user->server->IsService())
			msg.AddTag("inspircd.org/service", this, "");
	}
};

class ServProtect final
	: public SimpleUserMode
{
public:
	ServProtect(Module* Creator)
		: SimpleUserMode(Creator, "servprotect", 'k', true)
	{
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		// As this mode is only intended for use by pseudoclients the only way
		// to set it is by introducing a user with it.
		return false;
	}
};

class SVSHold final
	: public XLine
{
private:
	std::string nickname;

public:
	SVSHold(time_t settime, unsigned long period, const std::string& setter, const std::string& message, const std::string& nick)
		: XLine(settime, period, setter, message, "SVSHOLD")
		, nickname(nick)
	{
	}

	const std::string& Displayable() const override
	{
		return nickname;
	}

	void DisplayExpiry() override
	{
		// SVSHOLDs do not generate any messages.
	}

	bool Matches(User* user) const override
	{
		return irc::equals(user->nick, nickname);
	}

	bool Matches(const std::string& text) const override
	{
		return irc::equals(text, nickname);
	}
};

class SVSHoldFactory final
	: public XLineFactory
{
public:
	SVSHoldFactory()
		: XLineFactory("SVSHOLD")
	{
	}

	XLine* Generate(time_t settime, unsigned long duration, const std::string& source, const std::string& reason, const std::string& nick) override
	{
		return new SVSHold(settime, duration, source, reason, nick);
	}

	bool AutoApplyToUserList(XLine* x) override
	{
		return false;
	}
};

class CommandSVSHold final
	: public Command
{
public:
	CommandSVSHold(Module* Creator)
		: Command(Creator, "SVSHOLD")
	{
		// No need to set any privs because they're not checked for remote users.
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		// The command can only be executed by remote services servers.
		if (IS_LOCAL(user) || !user->server->IsService())
			return CmdResult::FAILURE;

		if (parameters.size() == 1)
		{
			// :36DAAAAAA SVSHOLD ChanServ
			std::string reason;
			return ServerInstance->XLines->DelLine(parameters[0], "SVSHOLD", reason, user) ? CmdResult::SUCCESS : CmdResult::FAILURE;
		}

		if (parameters.size() == 3)
		{
			/// :36DAAAAAA SVSHOLD NickServ 86400 :Reserved for services
			/// :36DAAAAAA SVSHOLD NickServ 1d :Reserved for services
			unsigned long duration;
			if (!Duration::TryFrom(parameters[1], duration))
				return CmdResult::FAILURE;

			auto* svshold = new SVSHold(ServerInstance->Time(), duration, user->nick, parameters[2], parameters[0]);
			return ServerInstance->XLines->AddLine(svshold, user) ? CmdResult::SUCCESS : CmdResult::FAILURE;
		}

		return CmdResult::FAILURE;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleServices final
	: public Module
	, public Stats::EventListener
{
private:
	Account::API accountapi;
	RegisteredChannel registeredcmode;
	RegisteredUser registeredumode;
	ServiceTag servicetag;
	ServProtect servprotectmode;
	SVSHoldFactory svsholdfactory;
	CommandSVSHold svsholdcmd;
	bool accountoverrideshold;

public:
	ModuleServices()
		: Module(VF_COMMON | VF_VENDOR, "Provides support for integrating with a services server.")
		, Stats::EventListener(this)
		, accountapi(this)
		, registeredcmode(this)
		, registeredumode(this)
		, servicetag(this)
		, servprotectmode(this)
		, svsholdcmd(this)
	{
	}

	~ModuleServices() override
	{
		ServerInstance->XLines->DelAll("SVSHOLD");
		ServerInstance->XLines->UnregisterFactory(&svsholdfactory);
	}

	void init() override
	{
		ServerInstance->XLines->RegisterFactory(&svsholdfactory);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("servicesintegration");
		accountoverrideshold = tag->getBool("accountoverrideshold");
	}

	ModResult OnKill(User* source, User* dest, const std::string& reason) override
	{
		if (!source)
			return MOD_RES_PASSTHRU;

		if (dest->IsModeSet(servprotectmode))
		{
			source->WriteNumeric(ERR_KILLDENY, INSP_FORMAT("You are not permitted to kill {} services!", ServerInstance->Config->Network));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnRawMode(User* user, Channel* chan, const Modes::Change& change) override
	{
		if (!IS_LOCAL(user) || change.adding || change.param.empty())
			return MOD_RES_PASSTHRU; // We only care about local users removing prefix modes.

		const PrefixMode* const pm = change.mh->IsPrefixMode();
		if (!pm)
			return MOD_RES_PASSTHRU; // Mode is not a prefix mode.

		auto* target = ServerInstance->Users.Find(change.param);
		if (!target)
			return MOD_RES_PASSTHRU; // Target does not exist.

		Membership* memb = chan->GetUser(target);
		if (!memb || !memb->HasMode(pm))
			return MOD_RES_PASSTHRU; // Target does not have the mode.

		if (target->IsModeSet(servprotectmode))
		{
			user->WriteNumeric(ERR_RESTRICTED, chan->name, INSP_FORMAT("You are not permitted to remove privileges from {} services!", ServerInstance->Config->Network));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() != 'S')
			return MOD_RES_PASSTHRU;

		ServerInstance->XLines->InvokeStats("SVSHOLD", stats);
		return MOD_RES_DENY;
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string& reason) override
	{
		if (memb->user->IsModeSet(servprotectmode))
		{
			source->WriteNumeric(ERR_RESTRICTED, memb->chan->name, INSP_FORMAT("You are not permitted to kick {} services!", ServerInstance->Config->Network));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) override
	{
		auto* svshold = ServerInstance->XLines->MatchesLine("SVSHOLD", newnick);
		if (!svshold)
			return MOD_RES_PASSTHRU;

		if (accountoverrideshold && accountapi)
		{
			Account::NickList* nicks = accountapi->GetAccountNicks(user);
			if (nicks && nicks->find(svshold->Displayable()) != nicks->end())
			{
				std::string reason;
				ServerInstance->XLines->DelLine(svshold, reason, user);
				return MOD_RES_PASSTHRU;
			}
		}

		user->WriteNumeric(ERR_ERRONEUSNICKNAME, newnick, INSP_FORMAT("Services reserved nickname: {}", svshold->reason));
		return MOD_RES_DENY;
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		if (user->IsModeSet(registeredumode) && !irc::equals(oldnick, user->nick))
			registeredumode.RemoveMode(user);
	}
};

MODULE_INIT(ModuleServices)
