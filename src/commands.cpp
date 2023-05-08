/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2014, 2018-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2008 Craig Edwards <brain@inspircd.org>
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

enum
{
	// From RFC 1459.
	ERR_NOTREGISTERED = 451,
	ERR_NEEDMOREPARAMS = 461,

	// InspIRCd-specific.
	RPL_SYNTAX = 650,
};

bool CommandParser::LoopCall(User* user, Command* handler, const CommandBase::Params& parameters, unsigned int splithere, int extra, bool usemax)
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
	CommandBase::Params splitparams(parameters);
	while (items1.GetToken(item) && (!usemax || max++ < ServerInstance->Config->MaxTargets))
	{
		if ((!check_dupes) || (dupes.insert(item).second))
		{
			splitparams[splithere] = item;

			if (extra >= 0)
			{
				// If we have two lists then get the next item from the second list.
				// In case it runs out of elements then 'item' will be an empty string.
				items2.GetToken(item);
				splitparams[extra] = item;
			}

			CmdResult result = handler->Handle(user, splitparams);
			if (localuser)
			{
				// Run the OnPostCommand hook with the last parameter being true to indicate
				// that the event is being called in a loop.
				item.clear();
				FOREACH_MOD(OnPostCommand, (handler, splitparams, localuser, result, true));
			}
		}
	}

	return true;
}

Command* CommandParser::GetHandler(const std::string& commandname)
{
	CommandMap::iterator n = cmdlist.find(commandname);
	if (n != cmdlist.end())
		return n->second;

	return nullptr;
}

// calls a handler function for a command

CmdResult CommandParser::CallHandler(const std::string& commandname, const CommandBase::Params& parameters, User* user, Command** cmd)
{
	/* find the command, check it exists */
	Command* handler = GetHandler(commandname);
	if (handler)
	{
		if ((!parameters.empty()) && (parameters.back().empty()) && (!handler->allow_empty_last_param))
			return CmdResult::INVALID;

		if (parameters.size() >= handler->min_params)
		{
			bool bOkay = false;
			if (IS_LOCAL(user))
			{
				switch (handler->access_needed)
				{
					case CmdAccess::NORMAL: // Anyone can execute.
						bOkay = true;
						break;

					case CmdAccess::OPERATOR: // Only opers can execute.
						bOkay = user->HasCommandPermission(commandname);
						break;

					case CmdAccess::SERVER: // Only servers can execute.
						bOkay = IS_SERVER(user);
						break;
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
					*cmd = handler;

				ClientProtocol::TagMap tags;
				return handler->Handle(user, CommandBase::Params(parameters, tags));
			}
		}
	}
	return CmdResult::INVALID;
}

void CommandParser::ProcessCommand(LocalUser* user, std::string& command, CommandBase::Params& command_p)
{
	/* find the command, check it exists */
	Command* handler = GetHandler(command);

	// Penalty to give if the command fails before the handler is executed
	unsigned int failpenalty = 0;

	/* Modify the user's penalty regardless of whether or not the command exists */
	if (!user->HasPrivPermission("users/flood/no-throttle"))
	{
		// If it *doesn't* exist, give it a slightly heftier penalty than normal to deter flooding us crap
		unsigned int penalty = handler ? handler->penalty : 2000;
		user->CommandFloodPenalty += penalty;

		// Increase their penalty later if we fail and the command has 0 penalty by default (i.e. in Command::penalty) to
		// throttle sending ERR_* from the command parser. If the command does have a non-zero penalty then this is not
		// needed because we've increased their penalty above.
		if (penalty == 0)
			failpenalty = 1000;
	}

	if (!handler)
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, command_p, user, false));
		if (MOD_RESULT == MOD_RES_DENY)
		{
			FOREACH_MOD(OnCommandBlocked, (command, command_p, user));
			return;
		}

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
			if (user->IsFullyConnected())
				user->WriteNumeric(ERR_UNKNOWNCOMMAND, command, "Unknown command");

			ServerInstance->stats.Unknown++;
			FOREACH_MOD(OnCommandBlocked, (command, command_p, user));
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
		const CommandBase::Params::iterator lastkeep = command_p.begin() + (handler->max_params - 1);
		// Iterator to the first excess parameter
		const CommandBase::Params::iterator firstexcess = lastkeep + 1;

		// Append all excess parameter(s) to the last parameter, separated by spaces
		for (const auto& param : insp::iterator_range(firstexcess, command_p.end()))
		{
			lastkeep->push_back(' ');
			lastkeep->append(param);
		}

		// Erase the excess parameter(s)
		command_p.erase(firstexcess, command_p.end());
	}

	/*
	 * We call OnPreCommand here separately if the command exists, so the magic above can
	 * truncate to max_params if necessary. -- w00t
	 */
	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, command_p, user, false));
	if (MOD_RESULT == MOD_RES_DENY)
	{
		FOREACH_MOD(OnCommandBlocked, (command, command_p, user));
		return;
	}

	/* activity resets the ping pending timer */
	user->nextping = ServerInstance->Time() + user->GetClass()->pingtime;
	switch (handler->access_needed)
	{
		case CmdAccess::NORMAL:
			break; // Nothing special too do.

		case CmdAccess::OPERATOR:
		{
			if (!user->IsOper())
			{
				user->CommandFloodPenalty += failpenalty;
				user->WriteNumeric(ERR_NOPRIVILEGES, "Permission Denied - You do not have the required operator privileges");
				FOREACH_MOD(OnCommandBlocked, (command, command_p, user));
				return;
			}

			if (!user->HasCommandPermission(command))
			{
				user->CommandFloodPenalty += failpenalty;
				user->WriteNumeric(ERR_NOPRIVILEGES, INSP_FORMAT("Permission Denied - Oper type {} does not have access to command {}",
					user->oper->GetType(), command));
				FOREACH_MOD(OnCommandBlocked, (command, command_p, user));
				return;
			}
			break;
		}

		case CmdAccess::SERVER:
		{
			if (user->IsFullyConnected())
				user->WriteNumeric(ERR_UNKNOWNCOMMAND, command, "Unknown command");

			ServerInstance->stats.Unknown++;
			FOREACH_MOD(OnCommandBlocked, (command, command_p, user));

			break;
		}
	}

	if ((!command_p.empty()) && (command_p.back().empty()) && (!handler->allow_empty_last_param))
		command_p.pop_back();

	if (command_p.size() < handler->min_params)
	{
		user->CommandFloodPenalty += failpenalty;
		handler->TellNotEnoughParameters(user, command_p);
		FOREACH_MOD(OnCommandBlocked, (command, command_p, user));
		return;
	}

	if (!user->IsFullyConnected() && !handler->works_before_reg)
	{
		user->CommandFloodPenalty += failpenalty;
		handler->TellNotFullyConnected(user, command_p);
		FOREACH_MOD(OnCommandBlocked, (command, command_p, user));
	}
	else
	{
		/* passed all checks.. first, do the (ugly) stats counters. */
		handler->use_count++;

		/* module calls too */
		FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, command_p, user, true));
		if (MOD_RESULT == MOD_RES_DENY)
		{
			FOREACH_MOD(OnCommandBlocked, (command, command_p, user));
			return;
		}

		/*
		 * WARNING: be careful, the user may be deleted soon
		 */
		CmdResult result = handler->Handle(user, command_p);

		FOREACH_MOD(OnPostCommand, (handler, command_p, user, result, false));
	}
}

void CommandParser::RemoveCommand(Command* x)
{
	CommandMap::iterator n = cmdlist.find(x->name);
	if (n != cmdlist.end() && n->second == x)
		cmdlist.erase(n);
}

void CommandParser::ProcessBuffer(LocalUser* user, const std::string& buffer)
{
	ClientProtocol::ParseOutput parseoutput;
	if (!user->serializer->Parse(user, buffer, parseoutput))
		return;

	std::string& command = parseoutput.cmd;
	std::transform(command.begin(), command.end(), command.begin(), ::toupper);

	CommandBase::Params parameters(parseoutput.params, parseoutput.tags);
	ProcessCommand(user, command, parameters);
}

bool CommandParser::AddCommand(Command* cmd)
{
	return cmdlist.emplace(cmd->name, cmd).second;
}

std::string CommandParser::TranslateUIDs(const std::vector<TranslateType>& to, const CommandBase::Params& source, bool prefix_final, CommandBase* custom_translator)
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
			auto* user = ServerInstance->Users.Find(item);
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
		[[fallthrough]];
		default:
			/* Do nothing */
			dest.append(item);
		break;
	}
}

CommandBase::CommandBase(Module* mod, const std::string& cmd, unsigned int minpara, unsigned int maxpara)
	: ServiceProvider(mod, cmd, SERVICE_COMMAND)
	, min_params(minpara)
	, max_params(maxpara)
{
}

void CommandBase::EncodeParameter(std::string& parameter, unsigned int index)
{
}

RouteDescriptor CommandBase::GetRouting(User* user, const Params& parameters)
{
	return ROUTE_LOCALONLY;
}

Command::Command(Module* mod, const std::string& cmd, unsigned int minpara, unsigned int maxpara)
	: CommandBase(mod, cmd, minpara, maxpara)
{
}

Command::~Command()
{
	ServerInstance->Parser.RemoveCommand(this);
}

void Command::RegisterService()
{
	if (!ServerInstance->Parser.AddCommand(this))
		throw ModuleException(creator, "Command already exists: " + name);
}

void Command::TellNotEnoughParameters(LocalUser* user, const Params& parameters)
{
	user->WriteNumeric(ERR_NEEDMOREPARAMS, name, "Not enough parameters.");
	if (ServerInstance->Config->SyntaxHints && user->IsFullyConnected())
	{
		for (const auto& syntaxline : this->syntax)
			user->WriteNumeric(RPL_SYNTAX, name, syntaxline);
	}
}

void Command::TellNotFullyConnected(LocalUser* user, const Params& parameters)
{
	user->WriteNumeric(ERR_NOTREGISTERED, name, "You must be fully connected to use this command.");
}

SplitCommand::SplitCommand(Module* me, const std::string& cmd, unsigned int minpara, unsigned int maxpara)
	: Command(me, cmd, minpara, maxpara)
{
}

CmdResult SplitCommand::Handle(User* user, const Params& parameters)
{
	switch (user->usertype)
	{
		case User::TYPE_LOCAL:
			return HandleLocal(static_cast<LocalUser*>(user), parameters);

		case User::TYPE_REMOTE:
			return HandleRemote(static_cast<RemoteUser*>(user), parameters);

		case User::TYPE_SERVER:
			return HandleServer(static_cast<FakeUser*>(user), parameters);
	}

	ServerInstance->Logs.Debug("COMMAND", "Unknown user type {} in command (uuid={})!",
		user->usertype, user->uuid);
	return CmdResult::INVALID;
}

CmdResult SplitCommand::HandleLocal(LocalUser* user, const Params& parameters)
{
	return CmdResult::INVALID;
}

CmdResult SplitCommand::HandleRemote(RemoteUser* user, const Params& parameters)
{
	return CmdResult::INVALID;
}

CmdResult SplitCommand::HandleServer(FakeUser* user, const Params& parameters)
{
	return CmdResult::INVALID;
}
