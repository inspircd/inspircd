/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
	
	// Replace all our characters in the array
	while(pos != std::string::npos)
	{
	  ret = ret.substr(0, pos) + special[i].replace + ret.substr(pos + special[i].character.size());
	  pos = ret.find(special[i].character, pos + special[i].replace.size());
	}
    }
    
    // Replace double slashes with a single slash before we return
    std::string::size_type pos = ret.find("\\\\");
    while(pos != std::string::npos)
    {
      ret = ret.substr(0, pos) + "\\" + ret.substr(pos + 2);
      pos = ret.find("\\\\", pos + 1);
    }
    *it = ret;
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
