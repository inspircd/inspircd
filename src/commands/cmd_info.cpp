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

/** Handle /INFO. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandInfo : public Command
{
 public:
	/** Constructor for info.
	 */
	CommandInfo ( Module* parent) : Command(parent,"INFO") { syntax = "[<servermask>]"; }
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

static const char* const lines[] = {
	"                   -/\\- \2InspIRCd\2 -\\/-",
	"                 November 2002 - Present",
	" ",
	"\2Core Developers\2:",
	"    Craig Edwards           Brain      <brain@inspircd.org>",
	"    Craig McLure            Craig      <craig@inspircd.org>",
	"    Robin Burchell          w00t       <w00t@inspircd.org>",
	"    Oliver Lupton           Om         <om@inspircd.org>",
	"    John Brooks             Special    <special@inspircd.org>",
	"    Dennis Friis            peavey     <peavey@inspircd.org>",
	"    Thomas Stagner          aquanight  <aquanight@inspircd.org>",
	"    Uli Schlachter          psychon    <psychon@inspircd.org>",
	"    Matt Smith              dz         <dz@inspircd.org>",
	"    Daniel De Graaf         danieldg   <danieldg@inspircd.org>",
	" ",
	"\2Regular Contributors\2:",
	"    Majic          MacGyver        Namegduf       Ankit",
	"    Phoenix        Taros           jackmcbarn",
	" ",
	"\2Other Contributors\2:",
	"   dmb             Zaba            skenmy         GreenReaper",
	"   Dan             Jason           satmd          owine",
	"   Adremelech      John2           jilles         HiroP",
	"   eggy            Bricker         AnMaster       djGrrr",
	"   nenolod         Quension        praetorian     pippijn",
	" ",
	"\2Former Contributors\2:",
	"   CC              jamie           typobox43      Burlex (win32)",
	"   Stskeeps        ThaPrince       BuildSmart     Thunderhacker",
	"   Skip            LeaChim",
	" ",
	"\2Thanks To\2:",
	"   searchirc.com   irc-junkie.org  Brik",
	" ",
	" Best experienced with: \2An IRC client\2",
	NULL
};

/** Handle /INFO
 */
CmdResult CommandInfo::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 0 && parameters[0] != ServerInstance->Config->ServerName)
		return CMD_SUCCESS;

	int i=0;
	while (lines[i])
		user->SendText(":%s %03d %s :%s", ServerInstance->Config->ServerName.c_str(), RPL_INFO, user->nick.c_str(), lines[i++]);
	FOREACH_MOD(I_OnInfo,OnInfo(user));
	user->SendText(":%s %03d %s :End of /INFO list", ServerInstance->Config->ServerName.c_str(), RPL_ENDOFINFO, user->nick.c_str());
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandInfo)
