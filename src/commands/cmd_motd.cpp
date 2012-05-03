/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

/** Handle /MOTD. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandMotd : public Command
{
 public:
	/** Constructor for motd.
	 */
	CommandMotd ( Module* parent) : Command(parent,"MOTD",0,1) { ServerInstance->ProcessedMotdEscapes = false; syntax = "[<servername>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (parameters.size() > 0)
			return ROUTE_UNICAST(parameters[0]);
		return ROUTE_LOCALONLY;
	}
};

inline std::string replace_all(const std::string &str, const std::string &orig, const std::string &repl)
{
	std::string new_str = str;
	std::string::size_type pos = new_str.find(orig), orig_length = orig.length(), repl_length = repl.length();
	while (pos != std::string::npos)
	{
		new_str = new_str.substr(0, pos) + repl + new_str.substr(pos + orig_length);
		pos = new_str.find(orig, pos + repl_length);
	}
	return new_str;
}

/*
 * Replace all color codes from the special[] array to actual
 * color code chars using C++ style escape sequences. You
 * can append other chars to replace if you like (such as %U
 * being underline). -- Justasic
 */
void ProcessColors(ConfigFileCache::iterator &file)
{
	static struct special_chars
	{
		std::string character;
		std::string replace;
		special_chars(const std::string &c, const std::string &r) : character(c), replace(r) { }
	}

	special[] = {
		special_chars("\\002", "\002"),  // Bold
                special_chars("\\037", "\037"),  // underline
                special_chars("\\003", "\003"),  // Color
		special_chars("\\0017", "\017"), // Stop colors
		special_chars("\\u", "\037"),    // Alias for underline
		special_chars("\\b", "\002"),    // Alias for Bold
		special_chars("\\x", "\017"),    // Alias for stop
		special_chars("\\c", "\003"),    // Alias for color
                special_chars("", "")
	};

	for(file_cache::iterator it = file->second.begin(); it != file->second.end(); it++)
	{
		std::string ret = *it;
		for(int i = 0; special[i].character.empty() == false; ++i)
		{
			std::string::size_type pos = ret.find(special[i].character);
			if(pos != std::string::npos && ret[pos-1] == '\\' && ret[pos] == '\\')
				continue; // Skip double slashes.

			ret = replace_all(ret, special[i].character, special[i].replace);
		}
		// Replace double slashes with a single slash before we return
		*it = replace_all(ret, "\\\\", "\\");
	}
}

/** Handle /MOTD
 */
CmdResult CommandMotd::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 0 && parameters[0] != ServerInstance->Config->ServerName)
		return CMD_SUCCESS;

	ConfigTag* tag = NULL;
	if (IS_LOCAL(user))
		tag = user->GetClass()->config;
	std::string motd_name = tag->getString("motd", "motd");
	ConfigFileCache::iterator motd = ServerInstance->Config->Files.find(motd_name);
	if (motd == ServerInstance->Config->Files.end())
	{
		user->SendText(":%s %03d %s :Message of the day file is missing.",
			ServerInstance->Config->ServerName.c_str(), ERR_NOMOTD, user->nick.c_str());
		return CMD_SUCCESS;
	}

	if(!ServerInstance->ProcessedMotdEscapes)
	{
		ProcessColors(motd);
		ServerInstance->ProcessedMotdEscapes = true;
	}

	user->SendText(":%s %03d %s :%s message of the day", ServerInstance->Config->ServerName.c_str(),
		RPL_MOTDSTART, user->nick.c_str(), ServerInstance->Config->ServerName.c_str());

	for (file_cache::iterator i = motd->second.begin(); i != motd->second.end(); i++)
		user->SendText(":%s %03d %s :- %s", ServerInstance->Config->ServerName.c_str(), RPL_MOTD, user->nick.c_str(), i->c_str());

	user->SendText(":%s %03d %s :End of message of the day.", ServerInstance->Config->ServerName.c_str(), RPL_ENDOFMOTD, user->nick.c_str());

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandMotd)
