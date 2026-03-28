/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022-2024 Sadie Powell <sadie@witchery.services>
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
#include "listmode.h"
#include "modules/account.h"
#include "modules/ctctags.h"
#include "modules/server.h"
#include "modules/stats.h"
#include "numerichelper.h"
#include "timeutils.h"
#include "xline.h"

enum
{
	// From UnrealIRCd.
	ERR_KILLDENY = 485,

	// From Charybdis.
	ERR_MLOCKRESTRICTED = 742,

	// InspIRCd-specific.
	ERR_TOPICLOCK = 744,
};

class ServicesAccountProvider final
	: public Account::ProviderAPIBase
	, public ServerProtocol::LinkEventListener
{
private:
	std::string target;
	const Server* targetserver = nullptr;

	void OnServerLink(const Server& server) override
	{
		UpdateStatus(server, true);
	}

	void OnServerSplit(const Server& server, bool error) override
	{
		UpdateStatus(server, false);
	}

	void SetAvailable(bool online)
	{
		auto *api = ServerInstance->Modules.FindService("accountproviderapi");
		if (online && api != this)
			ServerInstance->Modules.AddService(*this);
		else if (!online && api == this)
			ServerInstance->Modules.DelService(*this);
	}

	void UpdateStatus(const Server& server, bool online)
	{
		if (insp::casemapped_equals(target, server.GetName()))
		{
			ServerInstance->Logs.Debug(MODNAME, "Services server {} ({}) {}.", server.GetName(),
				server.GetId(), online ? "came online" : "went offline");
			targetserver = online ? &server : nullptr;
			SetAvailable(online);
		}
	}

public:
	ServicesAccountProvider(const WeakModulePtr& mod)
		: Account::ProviderAPIBase(mod)
		, ServerProtocol::LinkEventListener(mod)
	{
	}

	const Server* GetServer() const override
	{
		return targetserver;
	}

	void SetTarget(const std::string& newtarget)
	{
		if (target == newtarget)
			return; // Nothing has changed.

		target = newtarget;
		ProtocolInterface::ServerList servers;
		ServerInstance->PI->GetServerList(servers);
		for (const auto& server : servers)
		{
			if (insp::casemapped_equals(target, server->GetName()))
			{
				ServerInstance->Logs.Debug(MODNAME, "Changed the services server to {}.", server->GetName());
				SetAvailable(true);
				return;
			}
		}

		ServerInstance->Logs.Debug(MODNAME, "The services server ({}) is currently unavailable.", target);
		SetAvailable(false);
	}
};

class RegisteredChannel final
	: public SimpleChannelMode
{
public:
	RegisteredChannel(const WeakModulePtr& Creator)
		: SimpleChannelMode(Creator, "registered", 'r', false, "c_registered")
	{
		if (ServerInstance->Config->ConfValue("servicesintegration")->getBool("disablemodes", true))
			DisableAutoRegister();
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (source->IsLocal())
		{
			source->WriteNumeric(Numerics::NoPrivileges("only services may {} channel mode {} ({})",
				source->IsModeSet(this) ? "set" : "unset", GetModeChar(), this->service_name));
			return false;
		}

		return SimpleChannelMode::OnModeChange(source, dest, channel, change);
	}
};

class RegisteredUser final
	: public SimpleUserMode
{

public:
	RegisteredUser(const WeakModulePtr& Creator)
		: SimpleUserMode(Creator, "registered", 'r', false, "u_registered")
	{
		if (ServerInstance->Config->ConfValue("servicesintegration")->getBool("disablemodes", true))
			DisableAutoRegister();
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (source->IsLocal())
		{
			source->WriteNumeric(Numerics::NoPrivileges("only services may {} user mode {} ({})",
				source->IsModeSet(this) ? "set" : "unset", GetModeChar(), this->service_name));
			return false;
		}

		return SimpleUserMode::OnModeChange(source, dest, channel, change);
	}
};

class ServiceTag final
	: public CTCTags::TagProvider
{
public:
	ServiceTag(const WeakModulePtr& mod)
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
	ServProtect(const WeakModulePtr& Creator)
		: SimpleUserMode(Creator, "protect", 'k', true, "servprotect")
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

	void Apply(User* u) override
	{
		u->WriteNumeric(RPL_SAVENICK, u->nick, FMT::format("Services reserved nickname: {}", reason));
		u->ChangeNick(u->uuid);
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
		return insp::casemapped_equals(user->nick, nickname);
	}

	bool Matches(const std::string& text) const override
	{
		return insp::casemapped_equals(text, nickname);
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
};

class CommandSVSCMode final
	: public Command
{
public:
	CommandSVSCMode(const WeakModulePtr& mod)
		: Command(mod, "SVSCMODE", 3)
	{
		access_needed = CmdAccess::SERVER;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		// The command can only be executed by remote services servers.
		if (user->IsLocal() || !user->server->IsService())
			return CmdResult::FAILURE;

		auto* u = ServerInstance->Users.FindUUID(parameters[0]);
		if (!u)
			return CmdResult::FAILURE;

		auto* c = ServerInstance->Channels.Find(parameters[1]);
		if (!c)
			return CmdResult::FAILURE;

		auto full = ServerInstance->Config->BanRealMask;
		if (parameters.size() > 3)
			full = !!ConvToNum<uint8_t>(parameters[3], 1);

		for (auto mode : parameters[2])
		{
			auto* mh = ServerInstance->Modes.FindMode(mode, MODETYPE_CHANNEL);
			if (!mh || !mh->IsListModeBase())
				continue; // Not a list mode.

			auto* list = mh->IsListModeBase()->GetList(c);
			if (!list)
				continue; // No list modes set.

			Modes::ChangeList changelist;
			for (const auto& entry : *list)
			{
				if (c->CheckListEntry(mh, u, entry.mask, full))
					changelist.push(mh, false, entry.mask);
			}
			ServerInstance->Modes.Process(user, c, nullptr, changelist);
		}
		return CmdResult::SUCCESS;
	}
};

class CommandSVSHold final
	: public Command
{
public:
	CommandSVSHold(const WeakModulePtr& Creator)
		: Command(Creator, "SVSHOLD")
	{
		access_needed = CmdAccess::SERVER;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		// The command can only be executed by remote services servers.
		if (user->IsLocal() || !user->server->IsService())
			return CmdResult::FAILURE;

		if (parameters.size() == 1)
		{
			if (insp::casemapped_equals(parameters[0], "*"))
			{
				ServerInstance->XLines->DelAll("SVSHOLD", true);
			}
			else
			{
				// :36DAAAAAA SVSHOLD ChanServ
				std::string reason;
				return ServerInstance->XLines->DelLine(parameters[0], "SVSHOLD", reason, user) ? CmdResult::SUCCESS : CmdResult::FAILURE;
			}
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

class CommandSVSJoin final
	: public Command
{
public:
	CommandSVSJoin(const WeakModulePtr& mod)
		: Command(mod, "SVSJOIN", 2)
	{
		access_needed = CmdAccess::SERVER;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		// The command can only be executed by remote services servers.
		if (user->IsLocal() || !user->server->IsService())
			return CmdResult::FAILURE;

		// Check for a valid channel name.
		if (!ServerInstance->Channels.IsChannel(parameters[1]))
			return CmdResult::FAILURE;

		// Check the target exists/
		auto* u = ServerInstance->Users.FindUUID(parameters[0]);
		if (!u)
			return CmdResult::FAILURE;

		/* only join if it's local, otherwise just pass it on! */
		auto* lu = u->AsLocal();
		if (lu)
		{
			bool override = false;
			std::string key;
			if (parameters.size() >= 3)
			{
				key = parameters[2];
				if (key.empty())
					override = true;
			}

			Channel::JoinUser(lu, parameters[1], override, key);
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_UNICAST(parameters[0]);
	}
};

class CommandSVSNick final
	: public Command
{
public:
	CommandSVSNick(const WeakModulePtr& mod)
		: Command(mod, "SVSNICK", 3)
	{
		access_needed = CmdAccess::SERVER;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		// The command can only be executed by remote services servers.
		if (user->IsLocal() || !user->server->IsService())
			return CmdResult::FAILURE;

		auto* u = ServerInstance->Users.Find(parameters[0]);
		if (u && u->IsLocal())
		{
			// The 4th parameter is optional and it is the expected nick TS of the target user. If
			// this parameter is present and it doesn't match the user's nick TS, the SVSNICK is not
			// acted upon.
			//
			// This makes it possible to detect the case when services wants to change the nick of
			// a user, but the user changes their nick before the SVSNICK arrives, making the
			// SVSNICK nick change (usually to a guest nick) unnecessary. Consider the following for
			// example:
			//
			// 1. test changes nick to Attila which is protected by services
			// 2. Services SVSNICKs the user to Guest12345
			// 3. Attila changes nick to Attila_ which isn't protected by services
			// 4. SVSNICK arrives
			// 5. Attila_ gets his nick changed to Guest12345 unnecessarily
			//
			// In this case when the SVSNICK is processed the target has already changed their nick
			// to something which isn't protected, so changing the nick again to a Guest nick is not
			// desired. However, if the expected nick TS parameter is present in the SVSNICK then
			// the nick change in step 5 won't happen because the timestamps won't match.
			if (parameters.size() > 3)
			{
				time_t expectedts = ConvToNum<time_t>(parameters[3]);
				if (!expectedts)
					return CmdResult::INVALID; // Malformed message

				if (u->nickchanged != expectedts)
					return CmdResult::FAILURE; // Ignore SVSNICK
			}

			std::string nick = parameters[1];
			if (isdigit(nick[0]))
				nick = u->uuid;

			time_t nickts = ConvToNum<time_t>(parameters[2]);
			if (!nickts)
				return CmdResult::INVALID; // Malformed message

			if (!u->ChangeNick(nick, nickts))
			{
				// Changing to 'nick' failed (it may already be in use), change to the uuid
				u->WriteNumeric(RPL_SAVENICK, u->uuid, "Your nickname is in use by an older user on a new server.");
				u->ChangeNick(u->uuid);
			}
		}
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_UNICAST(parameters[0]);
	}
};

class CommandSVSOper final
	: public Command
{
public:
	CommandSVSOper(const WeakModulePtr& Creator)
		: Command(Creator, "SVSOPER", 2, 2)
	{
		access_needed = CmdAccess::SERVER;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (!user->server->IsService())
			return CmdResult::FAILURE;

		auto* target = ServerInstance->Users.FindUUID(parameters[0]);
		if (!target)
			return CmdResult::FAILURE;

		if (target->IsLocal())
		{
			auto iter = ServerInstance->Config->OperTypes.find(parameters[1]);
			if (iter == ServerInstance->Config->OperTypes.end())
				return CmdResult::FAILURE;

			auto automatic = parameters.GetTags().find("~automatic") != parameters.GetTags().end();
			auto account = std::make_shared<OperAccount>(MODNAME, iter->second, ServerInstance->Config->EmptyTag);
			target->OperLogin(account, automatic, true);
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		auto* target = ServerInstance->Users.FindUUID(parameters[0]);
		if (!target)
			return ROUTE_LOCALONLY;
		return ROUTE_OPT_UCAST(target->server);
	}
};

class CommandSVSPart final
	: public Command
{
public:
	CommandSVSPart(const WeakModulePtr& mod)
		: Command(mod, "SVSPART", 2)
	{
		access_needed = CmdAccess::SERVER;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		// The command can only be executed by remote services servers.
		if (user->IsLocal() || !user->server->IsService())
			return CmdResult::FAILURE;

		auto* u = ServerInstance->Users.FindUUID(parameters[0]);
		if (!u)
			return CmdResult::FAILURE;

		auto* c = ServerInstance->Channels.Find(parameters[1]);
		if (!c)
			return CmdResult::FAILURE;

		if (u->IsLocal())
			c->PartUser(u, parameters.size() == 3 ? parameters[2] : "Services forced part");

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_UNICAST(parameters[0]);
	}
};

class CommandSVSTopic final
	: public Command
{
public:
	CommandSVSTopic(const WeakModulePtr& mod)
		: Command(mod, "SVSTOPIC", 1, 4)
	{
		access_needed = CmdAccess::SERVER;
		allow_empty_last_param = true;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		// The command can only be executed by remote services servers.
		if (user->IsLocal() || !user->server->IsService())
			return CmdResult::FAILURE;

		auto* chan = ServerInstance->Channels.Find(parameters[0]);
		if (!chan)
			return CmdResult::FAILURE;

		if (parameters.size() == 4)
		{
			// 4 parameter version, set all topic data on the channel to the ones given in the parameters.
			time_t topicts = ConvToNum<time_t>(parameters[1]);
			if (!topicts)
			{
				ServerInstance->Logs.Debug(MODNAME, "Received SVSTOPIC with a 0 topicts; dropped.");
				return CmdResult::INVALID;
			}

			chan->SetTopic(user, parameters[3], topicts, &parameters[2]);
		}
		else
		{
			// 1 parameter version, nuke the topic
			chan->SetTopic(user, std::string(), 0);
			chan->setby.clear();
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleServices final
	: public Module
	, public ServerProtocol::RouteEventListener
	, public Stats::EventListener
{
private:
	Account::API accountapi;
	ServicesAccountProvider accountproviderapi;
	RegisteredChannel registeredcmode;
	RegisteredUser registeredumode;
	ServiceTag servicetag;
	ServProtect servprotectmode;
	SVSHoldFactory svsholdfactory;
	BoolExtItem auspexext;
	StringExtItem mlockext;
	BoolExtItem topiclockext;
	CommandSVSCMode svscmodecmd;
	CommandSVSHold svsholdcmd;
	CommandSVSJoin svsjoincmd;
	CommandSVSNick svsnickcmd;
	CommandSVSOper svsopercmd;
	CommandSVSPart svspartcmd;
	CommandSVSTopic svstopiccmd;
	bool accountoverrideshold;

	bool HandleModeLock(User* user, Channel* chan, const Modes::Change& change)
	{
		const auto* mlock = mlockext.Get(chan);
		if (!mlock)
			return true; // No mode lock.

		if (mlock->find(change.mh->GetModeChar()) == std::string::npos)
			return true; // Mode is not locked.

		user->WriteNumeric(ERR_MLOCKRESTRICTED, chan->name, change.mh->GetModeChar(), *mlock, FMT::format("Mode cannot be changed as it has been locked {} by services!",
			chan->IsModeSet(change.mh) ? "on" : "off"));
		return false;
	}

	bool HandleProtectedService(User* user, Channel* chan, const Modes::Change& change)
	{
		if (change.adding || change.param.empty())
			return true; // We only care about local users removing prefix modes.

		const auto* pm = change.mh->IsPrefixMode();
		if (!pm)
			return true; // Mode is not a prefix mode.

		auto* target = ServerInstance->Users.Find(change.param);
		if (!target)
			return true; // Target does not exist.

		Membership* memb = chan->GetUser(target);
		if (!memb || !memb->HasMode(pm))
			return true; // Target does not have the mode.

		if (!target->IsModeSet(servprotectmode))
			return true; // Target is not a protected service.

		user->WriteNumeric(ERR_RESTRICTED, chan->name, FMT::format("You are not permitted to remove privileges from {} services!", ServerInstance->Config->Network));
		return false;
	}

public:
	ModuleServices()
		: Module(VF_COMMON | VF_VENDOR, "Provides support for integrating with a services server.")
		, ServerProtocol::RouteEventListener(weak_from_this())
		, Stats::EventListener(weak_from_this())
		, accountapi(weak_from_this())
		, accountproviderapi(weak_from_this())
		, registeredcmode(weak_from_this())
		, registeredumode(weak_from_this())
		, servicetag(weak_from_this())
		, servprotectmode(weak_from_this())
		, auspexext(weak_from_this(), "auspex", ExtensionType::CHANNEL, true)
		, mlockext(weak_from_this(), "mlock", ExtensionType::CHANNEL, true)
		, topiclockext(weak_from_this(), "topiclock", ExtensionType::CHANNEL, true)
		, svscmodecmd(weak_from_this())
		, svsholdcmd(weak_from_this())
		, svsjoincmd(weak_from_this())
		, svsnickcmd(weak_from_this())
		, svsopercmd(weak_from_this())
		, svspartcmd(weak_from_this())
		, svstopiccmd(weak_from_this())
	{
	}

	~ModuleServices() override
	{
		ServerInstance->XLines->DelAll("SVSHOLD", true);
		ServerInstance->XLines->UnregisterFactory(&svsholdfactory);
	}

	void init() override
	{
		ServerInstance->XLines->RegisterFactory(&svsholdfactory);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("servicesintegration");

		const auto target = tag->getString("server", ServerInstance->Config->ConfValue("sasl")->getString("target"));
		if (target.empty())
			throw ModuleException(weak_from_this(), "<servicesintegration:server> must be set to the name of your services server!");

		accountproviderapi.SetTarget(target);
		accountoverrideshold = tag->getBool("accountoverrideshold");
	}

	ModResult OnRouteMessage(const Channel* channel, const Server* server) override
	{
		if (!server->IsService() || !auspexext.Get(channel))
			return MOD_RES_PASSTHRU;

		// Allow services to see messages in this channel even if not guarded.
		return MOD_RES_ALLOW;
	}

	ModResult OnKill(User* source, User* dest, const std::string& reason) override
	{
		if (!source)
			return MOD_RES_PASSTHRU;

		if (dest->IsModeSet(servprotectmode))
		{
			source->WriteNumeric(ERR_KILLDENY, FMT::format("You are not permitted to kill {} services!", ServerInstance->Config->Network));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreTopicChange(User* user, Channel* chan, const std::string& topic) override
	{
		if (!user->IsLocal() || !topiclockext.Get(chan))
			return MOD_RES_PASSTHRU; // Remote user or no topiclock.

		user->WriteNumeric(ERR_TOPICLOCK, chan->name, "Topic cannot be changed as it has been locked by services!");
		return MOD_RES_DENY;
	}

	ModResult OnRawMode(User* user, Channel* chan, const Modes::Change& change) override
	{
		if (!user->IsLocal() || !chan)
			return MOD_RES_PASSTHRU; // Not our job to handle.

		if (!HandleProtectedService(user, chan, change))
			return MOD_RES_DENY;

		if (!HandleModeLock(user, chan, change))
			return MOD_RES_DENY;

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
			source->WriteNumeric(ERR_RESTRICTED, memb->chan->name, FMT::format("You are not permitted to kick {} services!", ServerInstance->Config->Network));
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

		user->WriteNumeric(ERR_ERRONEUSNICKNAME, newnick, FMT::format("Services reserved nickname: {}", svshold->reason));
		return MOD_RES_DENY;
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		if (user->IsModeSet(registeredumode) && !insp::casemapped_equals(oldnick, user->nick))
			registeredumode.RemoveMode(user);
	}
};

MODULE_INIT(ModuleServices)
