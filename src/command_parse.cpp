/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008, 2010 Craig Edwards <brain@inspircd.org>
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
	if (!hashtype.empty() && !stdalgo::string::equalsci(hashtype, "plaintext"))
		return false;

	return TimingSafeCompare(data, input);
}

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

Command* CommandParser::GetHandler(const std::string &commandname)
{
	CommandMap::iterator n = cmdlist.find(commandname);
	if (n != cmdlist.end())
		return n->second;

	return NULL;
}

// calls a handler function for a command

CmdResult CommandParser::CallHandler(const std::string& commandname, const CommandBase::Params& parameters, User* user, Command** cmd)
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
					if (user->HasCommandPermission(commandname))
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

				ClientProtocol::TagMap tags;
				return n->second->Handle(user, CommandBase::Params(parameters, tags));
			}
		}
	}
	return CMD_INVALID;
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
			if (user->registered == REG_ALL)
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
		for (CommandBase::Params::const_iterator i = firstexcess; i != command_p.end(); ++i)
		{
			lastkeep->push_back(' ');
			lastkeep->append(*i);
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
	user->nextping = ServerInstance->Time() + user->MyClass->GetPingTime();

	if (handler->flags_needed)
	{
		if (!user->IsModeSet(handler->flags_needed))
		{
			user->CommandFloodPenalty += failpenalty;
			user->WriteNumeric(ERR_NOPRIVILEGES, "Permission Denied - You do not have the required operator privileges");
			FOREACH_MOD(OnCommandBlocked, (command, command_p, user));
			return;
		}

		if (!user->HasCommandPermission(command))
		{
			user->CommandFloodPenalty += failpenalty;
			user->WriteNumeric(ERR_NOPRIVILEGES, InspIRCd::Format("Permission Denied - Oper type %s does not have access to command %s",
				user->oper->name.c_str(), command.c_str()));
			FOREACH_MOD(OnCommandBlocked, (command, command_p, user));
			return;
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

	if ((user->registered != REG_ALL) && (!handler->works_before_reg))
	{
		user->CommandFloodPenalty += failpenalty;
		handler->TellNotRegistered(user, command_p);
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
		/*@fallthrough@*/
		default:
			/* Do nothing */
			dest.append(item);
		break;
	}
}
