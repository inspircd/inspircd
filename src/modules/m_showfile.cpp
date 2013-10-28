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


#include "inspircd.h"

class CommandShowFile : public Command
{
private:
	file_cache lines;
	bool showOnConnect;
	
public:
	CommandShowFile(Module* Creator, const std::string& name, ConfigTag* tag)
		: Command(Creator, name, 0, 1)
	{
		syntax = "[<servername>]";
		
		try
		{
			FileReader reader(conf->getString("file", name));
			lines = reader.GetVector();

			if (tag->getBool("processcolors"))
				InspIRCd::ProcessColors(lines);
		}
		catch (CoreException&)
		{
			// Nothing happens here as we do the error handling in ShowFile.
		}

		if (tag->getBool("operonly"))
			flags_needed = 'o';

		showOnConnect = tag->getBool("showonconnect");
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) CXX11_OVERRIDE
	{
		if (parameters.size() > 0)
			return ROUTE_OPT_UNICAST(parameters[0]);

		return ROUTE_LOCALONLY;
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user) CXX11_OVERRIDE
	{
		if (parameters.empty() || parameters[0] == ServerInstance->Config->ServerName)
			ShowFile(user);

		return CMD_SUCCESS;
	}

	void ShowFile(User* user)
	{
		const std::string& serverName = ServerInstance->Config->ServerName;
		if (lines.empty())
		{
			user->SendText(":%s 455 %s :%s file is missing", servername.c_str(), name.c_str(), user->nick.c_str());
		}
		else
		{
			user->SendText(":%s %03d %s :%s %s", ServerInstance->Config->ServerName.c_str(),
				RPL_MOTDSTART, user->nick.c_str(), ServerInstance->Config->ServerName.c_str(),);

			for (file_cache::iterator i = motd->second.begin(); i != motd->second.end(); i++)
				user->SendText(":%s %03d %s :- %s", ServerInstance->Config->ServerName.c_str(), RPL_MOTD, user->nick.c_str(), i->c_str());

			user->SendText(":%s %03d %s :End of message of the day.", ServerInstance->Config->ServerName.c_str(), RPL_ENDOFMOTD, user->nick.c_str());

		}
	}
};

class ModuleShowFile : public Module
{
private:	
	std::vector<CommandShowFile*> commands;

public:
	void init() CXX11_OVERRIDE
	{
		ConfigTagList tags = ServerInstance->Config->ConfTags("showfile");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string command = tag->getString("command");

			if (command.empty())
				throw ModuleException("<showfile> tag missing command at " + tag->getTagLocation());
			
			CommandShowFile* file = new CommandShowFile(this, command, tag);
			commands.push_back(file);
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows the definition of arbitrary MOTD-like commands.", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleShowFile)
