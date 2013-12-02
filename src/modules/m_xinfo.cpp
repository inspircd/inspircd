/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Peter Powell <petpow@saberuk.com>
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


#include "modules/xinfo.h"

// Previously implemented as 'STATS m'
class XInfoCommandUse : public XInfo
{
public:
	XInfoCommandUse(Module* Creator)
		: XInfo(Creator, "COMMANDUSE") { }

	void Handle(User* user, XInfoBuilder& builder) CXX11_OVERRIDE
	{
		const CommandParser::CommandMap& commands = ServerInstance->Parser.GetCommands();
		for (CommandParser::CommandMap::const_iterator i = commands.begin(); i != commands.end(); i++)
		{
			builder.String("NAME", i->second->name)
				.UInt64("COUNT", i->second->use_count)
				.Save();
		}
	}
};

// Previously implemented as 'STATS E'
class XInfoEvents : public XInfo
{
public:
	XInfoEvents(Module* Creator)
		: XInfo(Creator, "EVENTS") { }

	void Handle(User* user, XInfoBuilder& builder) CXX11_OVERRIDE
	{
		const SocketEngine::Statistics& stats = SocketEngine::GetStats();
		builder.UInt64("ERROR", stats.ErrorEvents)
			.UInt64("READ", stats.ReadEvents)
			.UInt64("WRITE", stats.WriteEvents)
			.UInt64("TOTAL", stats.TotalEvents)
			.Save();
	}
};

// Previously implemented as 'STATS o'
class XInfoOpers : public XInfo
{
public:
	XInfoOpers(Module* Creator)
		: XInfo(Creator, "OPERS") { }

	void Handle(User* user, XInfoBuilder& builder) CXX11_OVERRIDE
	{
			ConfigTagList tags = ServerInstance->Config->ConfTags("oper");
			for (ConfigIter i = tags.first; i != tags.second; ++i)
			{
				ConfigTag* tag = i->second;
				builder.Nick("NAME", tag->getString("name"))
					.String("TYPE", tag->getString("type"))
					.Save();
			}

	}
};

// Previously implemented as 'STATS p'
class XInfoPorts : public XInfo
{
public:
	XInfoPorts(Module* Creator)
		: XInfo(Creator, "PORTS") { }

	void Handle(User* user, XInfoBuilder& builder) CXX11_OVERRIDE
	{
		for (std::vector<ListenSocket*>::const_iterator i = ServerInstance->ports.begin(); i != ServerInstance->ports.end(); ++i)
		{
			ListenSocket* socket = *i;
			builder.IP("ADDRESS", socket->bind_addr.empty() ? "*" : socket->bind_addr)
				.Int32("PORT", socket->bind_port)
				.String("TYPE", socket->bind_tag->getString("type", "clients"))
				.String("TRANSPORT", socket->bind_tag->getString("ssl", "plaintext"))
				.Save();
		}
	}
};

// Previously implemented as 'STATS u'
class XInfoUptime : public XInfo
{
public:
	XInfoUptime(Module* Creator)
		: XInfo(Creator, "UPTIME") { }

	void Handle(User* user, XInfoBuilder& builder) CXX11_OVERRIDE
	{
		builder.TimeStamp("STARTUP", ServerInstance->startup_time)
			.Int64("DURATION", ServerInstance->Time() - ServerInstance->startup_time)
			.Save();
	}
};

class CommandXInfo : public Command
{
public:
	std::vector<std::string> UserInfo;

	CommandXInfo(Module* Creator)
		: Command(Creator, "XINFO", 1, 2)
	{
		syntax = "<topic> [<server>]";
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		std::string command = parameters[0];
		std::transform(command.begin(), command.end(), command.begin(), ::toupper);

		dynamic_reference<XInfo> provider(this->creator, "XINFO/" + command);
		if (!provider)
		{
			user->WriteNumeric(ERR_NOSUCHXINFO, command + " :No such XINFO topic available");
			return CMD_FAILURE;
		}

		bool isPublic = std::find(UserInfo.begin(), UserInfo.end(), command) != UserInfo.end();
		bool isRemoteOper = IS_REMOTE(user) && user->IsOper();
		bool isLocalOperWithPrivs = IS_LOCAL(user) && user->HasPrivPermission("servers/auspex");

		if (!isPublic && !isRemoteOper && !isLocalOperWithPrivs)
		{
			ServerInstance->SNO->WriteToSnoMask('t', "%s '%s' denied for %s (%s)", IS_LOCAL(user) ? "XInfo" : "Remote XInfo",
				command.c_str(), user->nick.c_str(), user->MakeHost().c_str());
			user->WriteNumeric(ERR_NOPRIVILEGES, ":Permission denied - XINFO " + command + " requires the servers/auspex privilege.");
			return CMD_FAILURE;
		}

		XInfoBuilder builder;
		try
		{
			provider->Handle(user, builder);
		}
		catch (XInfoException& ex)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "INTERNAL ERROR: handler for '" + command + "' threw exception: " + ex.GetReason());
			user->WriteNumeric(ERR_NOSUCHXINFO, command + " :No such XINFO topic available");
			return CMD_FAILURE;
		}

		if (builder.Lines.empty())
		{
			user->WriteNumeric(ERR_NOSUCHXINFO, command + " :No such XINFO topic available");
			return CMD_FAILURE;
		}

		std::string types;
		for (std::map<std::string, std::string>::iterator iter = builder.Types.begin(); iter != builder.Types.end(); iter++)
			types += " " + iter->first + " " + iter->second;

		user->WriteNumeric(RPL_XINFOTYPE, command + types);

		for (std::vector<std::string>::iterator iter = builder.Lines.begin(); iter != builder.Lines.end(); iter++)
			user->WriteNumeric(RPL_XINFOENTRY, command + *iter);

		user->WriteNumeric(RPL_XINFOEND, command + " :End of XINFO request");

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return parameters.size() > 1 ? ROUTE_UNICAST(parameters[1]) : ROUTE_LOCALONLY;
	}
};

class ModuleXInfo : public Module
{
	CommandXInfo cmd;
	XInfoCommandUse xcommanduse;
	XInfoEvents xevents;
	XInfoOpers xopers;
	XInfoPorts xports;
	XInfoUptime xuptime;

public:
	ModuleXInfo()
		: cmd(this)
		, xcommanduse(this)
		, xevents(this)
		, xopers(this)
		, xports(this)
		, xuptime(this)
		{ }

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		cmd.UserInfo.clear();
		irc::spacesepstream stream(ServerInstance->Config->ConfValue("security")->getString("userinfo"));

		std::string info;
		while (stream.GetToken(info))
		{
			std::transform(info.begin(), info.end(), info.begin(), ::toupper);
			cmd.UserInfo.push_back(info);
		}
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["XINFO"];
	}

	Version GetVersion()
	{
		return Version("Implements the IRCv3 XINFO extension.", VF_CORE|VF_VENDOR);
	}
};

MODULE_INIT(ModuleXInfo)
