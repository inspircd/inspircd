/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020-2022 Sadie Powell <sadie@witchery.services>
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
#include "fileutils.h"
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
{
	CommandAdmin cmdadmin;
	CommandCommands cmdcommands;
	CommandInfo cmdinfo;
	CommandModules cmdmodules;
	CommandMotd cmdmotd;
	CommandServList cmdservlist;
	CommandTime cmdtime;
	ISupportManager isupport;
	CommandVersion cmdversion;
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
		return modestr;
	}

	void OnServiceChange(const ServiceProvider& prov)
	{
		if (prov.service != SERVICE_MODE)
			return;

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
		, cmdadmin(this)
		, cmdcommands(this)
		, cmdinfo(this)
		, cmdmodules(this)
		, cmdmotd(this)
		, cmdservlist(this)
		, cmdtime(this)
		, isupport(this)
		, cmdversion(this, isupport)
		, numeric004(RPL_MYINFO)
	{
		numeric004.push(ServerInstance->Config->GetServerName());
		numeric004.push(INSPIRCD_BRANCH);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		// Process the escape codes in the MOTDs.
		ConfigFileCache newmotds;
		for (const auto& klass : ServerInstance->Config->Classes)
		{
			// Don't process the file if it has already been processed.
			const std::string motd = klass->config->getString("motd", "motd");
			if (newmotds.find(motd) != newmotds.end())
				continue;

			FileReader reader;
			try
			{
				reader.Load(motd);
			}
			catch (const CoreException& ce)
			{
				// We can't process the file if it doesn't exist.
				ServerInstance->Logs.Warning(MODNAME, "Unable to read motd for connect class \"{}\" at {}: {}",
					klass->GetName(), klass->config->source.str(), ce.GetReason());
				continue;
			}

			// Process the MOTD entry.
			file_cache& newmotd = newmotds[motd];
			newmotd.reserve(reader.GetVector().size());
			for (const auto& line : reader.GetVector())
			{
				// Some clients can not handle receiving RPL_MOTD with an empty
				// trailing parameter so if a line is empty we replace it with
				// a single space.
				newmotd.push_back(line.empty() ? " " : line);
			}
			InspIRCd::ProcessColors(newmotd);
		}

		cmdmotd.motds.swap(newmotds);

		const auto& tag = ServerInstance->Config->ConfValue("admin");
		cmdadmin.adminname = tag->getString("name", tag->getString("nick", ServerInstance->Config->Network + " Admins", 1));
		cmdadmin.admindesc = tag->getString("description");
		cmdadmin.adminemail = tag->getString("email", "noreply@" + ServerInstance->Config->GetServerName(), 1);

		cmdversion.BuildNumerics();
		ServerInstance->AtomicActions.AddAction(new ISupportAction(isupport));
	}

	void OnUserConnect(LocalUser* user) override
	{
		user->WriteNumeric(RPL_WELCOME, INSP_FORMAT("Welcome to the {} IRC Network {}", ServerInstance->Config->Network, user->GetRealMask()));
		user->WriteNumeric(RPL_YOURHOST, INSP_FORMAT("Your host is {}, running version {}", ServerInstance->Config->GetServerName(), INSPIRCD_BRANCH));
		user->WriteNumeric(RPL_CREATED, Time::ToString(ServerInstance->startup_time, "This server was created %H:%M:%S %b %d %Y"));
		user->WriteNumeric(numeric004);
		isupport.SendTo(user);

		/* Trigger MOTD and LUSERS output, give modules a chance too */
		ModResult MOD_RESULT;
		std::string command("LUSERS");
		CommandBase::Params parameters;
		FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, parameters, user, true));
		if (!MOD_RESULT)
			ServerInstance->Parser.CallHandler(command, parameters, user);

		MOD_RESULT = MOD_RES_PASSTHRU;
		command = "MOTD";
		FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, parameters, user, true));
		if (!MOD_RESULT)
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
		OnServiceChange(service);
	}

	void OnServiceDel(ServiceProvider& service) override
	{
		OnServiceChange(service);
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnUserConnect, PRIORITY_FIRST);
	}
};

MODULE_INIT(CoreModInfo)
