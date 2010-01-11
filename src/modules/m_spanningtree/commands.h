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

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

/** Handle /RCONNECT
 */
class CommandRConnect : public Command
{
        SpanningTreeUtilities* Utils;	/* Utility class */
 public:
        CommandRConnect (Module* Callback, SpanningTreeUtilities* Util);
        CmdResult Handle (const std::vector<std::string>& parameters, User *user);
		RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class CommandRSQuit : public Command
{
        SpanningTreeUtilities* Utils;	/* Utility class */
 public:
        CommandRSQuit(Module* Callback, SpanningTreeUtilities* Util);
        CmdResult Handle (const std::vector<std::string>& parameters, User *user);
		RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
        void NoticeUser(User* user, const std::string &msg);
};

class CommandSVSJoin : public Command
{
 public:
	CommandSVSJoin(Module* Creator) : Command(Creator, "SVSJOIN", 2) { flags_needed = 'o'; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};
class CommandSVSPart : public Command
{
 public:
	CommandSVSPart(Module* Creator) : Command(Creator, "SVSPART", 2) { flags_needed = 'o'; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};
class CommandSVSNick : public Command
{
 public:
	CommandSVSNick(Module* Creator) : Command(Creator, "SVSNICK", 2) { flags_needed = 'o'; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

#endif
