/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2005-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
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

bool InspIRCd::PassCompare(Extensible* ex, const std::string& data, const std::string& input, const std::string& hashtype)
{
	ModResult res;
	FIRST_MOD_RESULT(OnPassCompare, res, (ex, data, input, hashtype));

	/* Module matched */
	if (res == MOD_RES_ALLOW)
		return true;

	/* Module explicitly didnt match */
	if (res == MOD_RES_DENY)
		return false;

	/* We dont handle any hash types except for plaintext - Thanks tra26 */
	if (!hashtype.empty() && hashtype != "plaintext")
		return false;

	return TimingSafeCompare(data, input);
}

bool CommandParser::LoopCall(User* user, Command* handler, const std::vector<std::string>& parameters, unsigned int splithere, int extra, bool usemax)
{
	if (splithere >= parameters.size())
		return false;

	/* First check if we have more than one item in the list, if we don't we return false here and the handler
	 * which called us just carries on as it was.
	 */
	if (parameters[splithere].find(',') == std::string::npos)
		return false;

	/** Some lame ircds will weed out dupes using some shitty O(n^2) algorithm.
	 * By using std::set (thanks for the idea w00t) we can cut this down a ton.
	 * ...VOOODOOOO!
	 *
	 * Only check for duplicates if there is one list (allow them in JOIN).
	 */
	insp::flat_set<std::string, irc::insensitive_swo> dupes;
	bool check_dupes = (extra < 0);

	/* Create two sepstreams, if we have only one list, then initialize the second sepstream with
	 * an empty string. The second parameter of the constructor of the sepstream tells whether
	 * or not to allow empty tokens.
	 * We allow empty keys, so "JOIN #a,#b ,bkey" will be interpreted as "JOIN #a", "JOIN #b bkey"
	 */
	irc::commasepstream items1(parameters[splithere]);
	irc::commasepstream items2(extra >= 0 ? parameters[extra] : "", true);
	std::string item;
	unsigned int max = 0;
	LocalUser* localuser = IS_LOCAL(user);

	/* Attempt to iterate these lists and call the command handler
	 * for every parameter or parameter pair until there are no more
	 * left to parse.
	 */
	while (items1.GetToken(item) && (!usemax || max++ < ServerInstance->Config->MaxTargets))
	{
		if ((!check_dupes) || (dupes.insert(item).second))
		{
			std::vector<std::string> new_parameters(parameters);
			new_parameters[splithere] = item;

			if (extra >= 0)
			{
				// If we have two lists then get the next item from the second list.
				// In case it runs out of elements then 'item' will be an empty string.
				items2.GetToken(item);
				new_parameters[extra] = item;
			}

			CmdResult result = handler->Handle(new_parameters, user);
			if (localuser)
			{
				// Run the OnPostCommand hook with the last parameter (original line) being empty
				// to indicate that the command had more targets in its original form.
				item.clear();
				FOREACH_MOD(OnPostCommand, (handler, new_parameters, localuser, result, item));
			}
		}
	}

	return true;
}

Command* CommandParser::GetHandler(const std::string &commandname)
{
	CommandMap::iterator n = cmdlist.find(commandname);
	if (n != cmdlist.end())
		return n->second;

	return NULL;
}

// calls a handler function for a command

CmdResult CommandParser::CallHandler(const std::string& commandname, const std::vector<std::string>& parameters, User* user, Command** cmd)
{
	CommandMap::iterator n = cmdlist.find(commandname);

	if (n != cmdlist.end())
	{
		if ((!parameters.empty()) && (parameters.back().empty()) && (!n->second->allow_empty_last_param))
			return CMD_INVALID;

		if (parameters.size() >= n->second->min_params)
		{
			bool bOkay = false;

			if (IS_LOCAL(user) && n->second->flags_needed)
			{
				/* if user is local, and flags are needed .. */

				if (user->IsModeSet(n->second->flags_needed))
				{
					/* if user has the flags, and now has the permissions, go ahead */
					if (user->HasPermission(commandname))
						bOkay = true;
				}
			}
			else
			{
				/* remote or no flags required anyway */
				bOkay = true;
			}

			if (bOkay)
			{
				if (cmd)
					*cmd = n->second;
				return n->second->Handle(parameters,user);
			}
		}
	}
	return CMD_INVALID;
}

void CommandParser::ProcessCommand(LocalUser *user, std::string &cmd)
{
	std::vector<std::string> command_p;
	irc::tokenstream tokens(cmd);
	std::string command, token;
	tokens.GetToken(command);

	/* A client sent a nick prefix on their command (ick)
	 * rhapsody and some braindead bouncers do this --
	 * the rfc says they shouldnt but also says the ircd should
	 * discard it if they do.
	 */
	if (command[0] == ':')
		tokens.GetToken(command);

	while (tokens.GetToken(token))
		command_p.push_back(token);

	std::transform(command.begin(), command.end(), command.begin(), ::toupper);

	/* find the command, check it exists */
	Command* handler = GetHandler(command);

	// Penalty to give if the command fails before the handler is executed
	unsigned int failpenalty = 0;

	/* Modify the user's penalty regardless of whether or not the command exists */
	if (!user->HasPrivPermission("users/flood/no-throttle"))
	{
		// If it *doesn't* exist, give it a slightly heftier penalty than normal to deter flooding us crap
		unsigned int penalty = (handler ? handler->Penalty * 1000 : 2000);
		user->CommandFloodPenalty += penalty;

		// Increase their penalty later if we fail and the command has 0 penalty by default (i.e. in Command::Penalty) to
		// throttle sending ERR_* from the command parser. If the command does have a non-zero penalty then this is not
		// needed because we've increased their penalty above.
		if (penalty == 0)
			failpenalty = 1000;
	}

	if (!handler)
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, command_p, user, false, cmd));
		if (MOD_RESULT == MOD_RES_DENY)
			return;

		/*
		 * This double lookup is in case a module (abbreviation) wishes to change a command.
		 * Sure, the double lookup is a bit painful, but bear in mind this only happens for unknowns anyway.
		 *
		 * Thanks dz for making me actually understand why this is necessary!
		 * -- w00t
		 */
		handler = GetHandler(command);
		if (!handler)
		{
			if (user->registered == REG_ALL)
				user->WriteNumeric(ERR_UNKNOWNCOMMAND, command, "Unknown command");
			ServerInstance->stats.Unknown++;
			return;
		}
	}

	// If we were given more parameters than max_params then append the excess parameter(s)
	// to command_p[maxparams-1], i.e. to the last param that is still allowed
	if (handler->max_params && command_p.size() > handler->max_params)
	{
		/*
		 * command_p input (assuming max_params 1):
		 *	this
		 *	is
		 *	a
		 *	test
		 */

		// Iterator to the last parameter that will be kept
		const std::vector<std::string>::iterator lastkeep = command_p.begin() + (handler->max_params - 1);
		// Iterator to the first excess parameter
		const std::vector<std::string>::iterator firstexcess = lastkeep + 1;

		// Append all excess parameter(s) to the last parameter, seperated by spaces
		for (std::vector<std::string>::const_iterator i = firstexcess; i != command_p.end(); ++i)
		{
			lastkeep->push_back(' ');
			lastkeep->append(*i);
		}

		// Erase the excess parameter(s)
		command_p.erase(firstexcess, command_p.end());
	}

	/*
	 * We call OnPreCommand here seperately if the command exists, so the magic above can
	 * truncate to max_params if necessary. -- w00t
	 */
	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, command_p, user, false, cmd));
	if (MOD_RESULT == MOD_RES_DENY)
		return;

	/* activity resets the ping pending timer */
	user->nping = ServerInstance->Time() + user->MyClass->GetPingTime();

	if (handler->flags_needed)
	{
		if (!user->IsModeSet(handler->flags_needed))
		{
			user->CommandFloodPenalty += failpenalty;
			user->WriteNumeric(ERR_NOPRIVILEGES, "Permission Denied - You do not have the required operator privileges");
			return;
		}

		if (!user->HasPermission(command))
		{
			user->CommandFloodPenalty += failpenalty;
			user->WriteNumeric(ERR_NOPRIVILEGES, InspIRCd::Format("Permission Denied - Oper type %s does not have access to command %s",
				user->oper->name.c_str(), command.c_str()));
			return;
		}
	}

	if ((user->registered == REG_ALL) && (!user->IsOper()) && (handler->IsDisabled()))
	{
		/* command is disabled! */
		user->CommandFloodPenalty += failpenalty;
		if (ServerInstance->Config->DisabledDontExist)
		{
			user->WriteNumeric(ERR_UNKNOWNCOMMAND, command, "Unknown command");
		}
		else
		{
			user->WriteNumeric(ERR_UNKNOWNCOMMAND, command, "This command has been disabled.");
		}

		ServerInstance->SNO->WriteToSnoMask('a', "%s denied for %s (%s@%s)",
				command.c_str(), user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		return;
	}

	if ((!command_p.empty()) && (command_p.back().empty()) && (!handler->allow_empty_last_param))
		command_p.pop_back();

	if (command_p.size() < handler->min_params)
	{
		user->CommandFloodPenalty += failpenalty;
		user->WriteNumeric(ERR_NEEDMOREPARAMS, command, "Not enough parameters.");
		if ((ServerInstance->Config->SyntaxHints) && (user->registered == REG_ALL) && (handler->syntax.length()))
			user->WriteNumeric(RPL_SYNTAX, InspIRCd::Format("SYNTAX %s %s", handler->name.c_str(), handler->syntax.c_str()));
		return;
	}

	if ((user->registered != REG_ALL) && (!handler->WorksBeforeReg()))
	{
		user->CommandFloodPenalty += failpenalty;
		user->WriteNumeric(ERR_NOTREGISTERED, command, "You have not registered");
	}
	else
	{
		/* passed all checks.. first, do the (ugly) stats counters. */
		handler->use_count++;

		/* module calls too */
		FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, command_p, user, true, cmd));
		if (MOD_RESULT == MOD_RES_DENY)
			return;

		/*
		 * WARNING: be careful, the user may be deleted soon
		 */
		CmdResult result = handler->Handle(command_p, user);

		FOREACH_MOD(OnPostCommand, (handler, command_p, user, result, cmd));
	}
}

void CommandParser::RemoveCommand(Command* x)
{
	CommandMap::iterator n = cmdlist.find(x->name);
	if (n != cmdlist.end() && n->second == x)
		cmdlist.erase(n);
}

CommandBase::CommandBase(Module* mod, const std::string& cmd, unsigned int minpara, unsigned int maxpara)
	: ServiceProvider(mod, cmd, SERVICE_COMMAND)
	, flags_needed(0)
	, min_params(minpara)
	, max_params(maxpara)
	, use_count(0)
	, disabled(false)
	, works_before_reg(false)
	, allow_empty_last_param(true)
	, Penalty(1)
{
}

CommandBase::~CommandBase()
{
}

void CommandBase::EncodeParameter(std::string& parameter, int index)
{
}

RouteDescriptor CommandBase::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	return ROUTE_LOCALONLY;
}

Command::Command(Module* mod, const std::string& cmd, unsigned int minpara, unsigned int maxpara)
	: CommandBase(mod, cmd, minpara, maxpara)
	, force_manual_route(false)
{
}

Command::~Command()
{
	ServerInstance->Parser.RemoveCommand(this);
}

void Command::RegisterService()
{
	if (!ServerInstance->Parser.AddCommand(this))
		throw ModuleException("Command already exists: " + name);
}

void CommandParser::ProcessBuffer(std::string &buffer,LocalUser *user)
{
	if (buffer.empty())
		return;

	ServerInstance->Logs->Log("USERINPUT", LOG_RAWIO, "C[%s] I %s", user->uuid.c_str(), buffer.c_str());
	ProcessCommand(user,buffer);
}

bool CommandParser::AddCommand(Command *f)
{
	/* create the command and push it onto the table */
	if (cmdlist.find(f->name) == cmdlist.end())
	{
		cmdlist[f->name] = f;
		return true;
	}
	return false;
}

CommandParser::CommandParser()
{
}

std::string CommandParser::TranslateUIDs(const std::vector<TranslateType>& to, const std::vector<std::string>& source, bool prefix_final, CommandBase* custom_translator)
{
	std::vector<TranslateType>::const_iterator types = to.begin();
	std::string dest;

	for (unsigned int i = 0; i < source.size(); i++)
	{
		TranslateType t = TR_TEXT;
		// They might supply less translation types than parameters,
		// in that case pretend that all remaining types are TR_TEXT
		if (types != to.end())
		{
			t = *types;
			types++;
		}

		bool last = (i == (source.size() - 1));
		if (prefix_final && last)
			dest.push_back(':');

		TranslateSingleParam(t, source[i], dest, custom_translator, i);

		if (!last)
			dest.push_back(' ');
	}

	return dest;
}

void CommandParser::TranslateSingleParam(TranslateType to, const std::string& item, std::string& dest, CommandBase* custom_translator, unsigned int paramnumber)
{
	switch (to)
	{
		case TR_NICK:
		{
			/* Translate single nickname */
			User* user = ServerInstance->FindNick(item);
			if (user)
				dest.append(user->uuid);
			else
				dest.append(item);
			break;
		}
		case TR_CUSTOM:
		{
			if (custom_translator)
			{
				std::string translated = item;
				custom_translator->EncodeParameter(translated, paramnumber);
				dest.append(translated);
				break;
			}
			// If no custom translator was given, fall through
		}
		case TR_TEXT:
		default:
			/* Do nothing */
			dest.append(item);
		break;
	}
}
