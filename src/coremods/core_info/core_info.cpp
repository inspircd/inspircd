/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2018-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014, 2016, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "clientprotocolmsg.h"
#include "timeutils.h"

#include "core_info.h"

enum
{
	// From RFC 2812.
	RPL_WELCOME = 1,
	RPL_YOURHOST = 2,
	RPL_CREATED = 3,
	RPL_MYINFO = 4
};

RouteDescriptor ServerTargetCommand::GetRouting(User* user, const Params& parameters)
{
	// Parameter must be a server name, not a nickname or uuid
	if ((!parameters.empty()) && (parameters[0].find('.') != std::string::npos))
		return ROUTE_UNICAST(parameters[0]);
	return ROUTE_LOCALONLY;
}

struct ISupportAction final
	: public ActionBase
{
	ISupportManager& isupport;

	ISupportAction(ISupportManager& i)
		: isupport(i)
	{
	}

	void Call() override
	{
		isupport.Build();
		ServerInstance->GlobalCulls.AddItem(this);
	}
};

class CoreModInfo final
	: public Module
	, public ISupport::EventListener
{
private:
	CommandAdmin cmdadmin;
	CommandCommands cmdcommands;
	CommandInfo cmdinfo;
	CommandModules cmdmodules;
	CommandMotd cmdmotd;
	CommandServList cmdservlist;
	CommandTime cmdtime;
	ISupportManager isupport;
	CommandVersion cmdversion;
	Numeric::Numeric numeric003;
	Numeric::Numeric numeric004;

	/** Returns a list of user or channel mode characters.
	 * Used for constructing the parts of the mode list in the 004 numeric.
	 * @param mt Controls whether to list user modes or channel modes
	 * @param needparam Return modes only if they require a parameter to be set
	 * @return The available mode letters that satisfy the given conditions
	*/
	static std::string CreateModeList(ModeType mt, bool needparam = false)
	{
		std::string modestr;
		for (const auto& [_, mh] : ServerInstance->Modes.GetModes(mt))
		{
			if (!needparam || mh->NeedsParam(true))
				modestr.push_back(mh->GetModeChar());
		}
		std::sort(modestr.begin(), modestr.end());
		return modestr;
	}

	void Rebuild004()
	{
		std::vector<std::string>& params = numeric004.GetParams();
		params.erase(params.begin()+2, params.end());

		// Create lists of modes
		// 1. User modes
		// 2. Channel modes
		// 3. Channel modes that require a parameter when set
		numeric004.push(CreateModeList(MODETYPE_USER));
		numeric004.push(CreateModeList(MODETYPE_CHANNEL));
		numeric004.push(CreateModeList(MODETYPE_CHANNEL, true));
	}

public:
	CoreModInfo()
		: Module(VF_CORE | VF_VENDOR, "Provides the ADMIN, COMMANDS, INFO, MODULES, MOTD, TIME, SERVLIST, and VERSION commands")
		, ISupport::EventListener(this)
		, cmdadmin(this)
		, cmdcommands(this)
		, cmdinfo(this)
		, cmdmodules(this)
		, cmdmotd(this)
		, cmdservlist(this)
		, cmdtime(this)
		, isupport(this)
		, cmdversion(this, isupport)
		, numeric003(RPL_CREATED)
		, numeric004(RPL_MYINFO)
	{
		numeric003.push(Time::ToString(ServerInstance->startup_time, "This server was created on %d %b %Y at %H:%M:%S %Z", true));

		numeric004.push(ServerInstance->Config->GetServerName());
		numeric004.push(INSPIRCD_BRANCH);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		// Process the escape codes in the MOTDs.
		CommandMotd::MessageCache newmotds;
		for (const auto& klass : ServerInstance->Config->Classes)
		{
			// Don't process the file if it has already been processed.
			const std::string motd = klass->config->getString("motd", "motd");
			if (newmotds.find(motd) != newmotds.end())
				continue;

			auto file = ServerInstance->Config->ReadFile(motd);
			if (!file)
			{
				// We can't process the file if it doesn't exist.
				ServerInstance->Logs.Warning(MODNAME, "Unable to read motd for connect class \"{}\" at {}: {}",
					klass->GetName(), klass->config->source.str(), file.error);
				continue;
			}

			// Process the MOTD entry.
			auto& newmotd = newmotds[motd];
			irc::sepstream linestream(file.contents, '\n', true);
			for (std::string line; linestream.GetToken(line); )
			{
				// Some clients can not handle receiving RPL_MOTD with an empty
				// trailing parameter so if a line is empty we replace it with
				// a single space.
				InspIRCd::ProcessColors(line);
				newmotd.push_back(line.empty() ? " " : line);
			}
		}

		cmdmotd.motds.swap(newmotds);

		const auto& tag = ServerInstance->Config->ConfValue("admin");
		cmdadmin.adminname = tag->getString("name", tag->getString("nick", ServerInstance->Config->Network + " Admins", 1));
		cmdadmin.admindesc = tag->getString("description");
		cmdadmin.adminemail = tag->getString("email", "noreply@" + ServerInstance->Config->GetServerName(), 1);

		Rebuild004();

		cmdversion.BuildNumerics();
		ServerInstance->AtomicActions.AddAction(new ISupportAction(isupport));
	}

	void OnChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, bool force) override
	{
		// TODO: this should be OnPostChangeConnectClass but we need the old
		// connect class which isn't exposed to the module interface and we
		// can't break the API in a stable release. For now we use this and
		// prioritise it to be after core_user checks whether the user needs
		// to die.
		if (user->IsFullyConnected() && !user->quitting)
			isupport.ChangeClass(user, user->GetClass(), klass);
	}

	void OnUserConnect(LocalUser* user) override
	{
		user->WriteNumeric(RPL_WELCOME, INSP_FORMAT("Welcome to the {} IRC Network {}", ServerInstance->Config->Network, user->GetRealMask()));
		user->WriteNumeric(RPL_YOURHOST, INSP_FORMAT("Your host is {}, running version {}", ServerInstance->Config->GetServerName(), INSPIRCD_BRANCH));
		user->WriteNumeric(numeric003);
		user->WriteNumeric(numeric004);
		isupport.SendTo(user);

		/* Trigger MOTD and LUSERS output, give modules a chance too */
		ModResult modres;
		std::string command("LUSERS");
		CommandBase::Params parameters;
		FIRST_MOD_RESULT(OnPreCommand, modres, (command, parameters, user, true));
		if (!modres)
			ServerInstance->Parser.CallHandler(command, parameters, user);

		modres = MOD_RES_PASSTHRU;
		command = "MOTD";
		FIRST_MOD_RESULT(OnPreCommand, modres, (command, parameters, user, true));
		if (!modres)
			ServerInstance->Parser.CallHandler(command, parameters, user);

		if (ServerInstance->Config->RawLog)
			Log::NotifyRawIO(user, MessageType::PRIVMSG);
	}

	void OnLoadModule(Module* mod) override
	{
		isupport.Build();
	}
	void OnUnloadModule(Module* mod) override
	{
		isupport.Build();
	}

	void OnServiceAdd(ServiceProvider& service) override
	{
		if (service.service == SERVICE_MODE)
			Rebuild004();
	}

	void OnServiceDel(ServiceProvider& service) override
	{
		if (service.service == SERVICE_MODE)
			Rebuild004();
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnUserConnect, PRIORITY_FIRST);
	}

	void OnBuildClassISupport(const std::shared_ptr<ConnectClass>& klass, ISupport::TokenMap& tokens) override
	{
		if (klass->fakelag)
			tokens["FAKELAG"];
	}
};

MODULE_INIT(CoreModInfo)
