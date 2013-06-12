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

int InspIRCd::PassCompare(Extensible* ex, const std::string &data, const std::string &input, const std::string &hashtype)
{
	ModResult res;
	FIRST_MOD_RESULT(OnPassCompare, res, (ex, data, input, hashtype));

	/* Module matched */
	if (res == MOD_RES_ALLOW)
		return 0;

	/* Module explicitly didnt match */
	if (res == MOD_RES_DENY)
		return 1;

	/* We dont handle any hash types except for plaintext - Thanks tra26 */
	if (!hashtype.empty() && hashtype != "plaintext")
		/* See below. 1 because they dont match */
		return 1;

	return (data != input); // this seems back to front, but returns 0 if they *match*, 1 else
}

/* LoopCall is used to call a command classes handler repeatedly based on the contents of a comma seperated list.
 * There are two overriden versions of this method, one of which takes two potential lists and the other takes one.
 * We need a version which takes two potential lists for JOIN, because a JOIN may contain two lists of items at once,
 * the channel names and their keys as follows:
 * JOIN #chan1,#chan2,#chan3 key1,,key3
 * Therefore, we need to deal with both lists concurrently. The first instance of this method does that by creating
 * two instances of irc::commasepstream and reading them both together until the first runs out of tokens.
 * The second version is much simpler and just has the one stream to read, and is used in NAMES, WHOIS, PRIVMSG etc.
 * Both will only parse until they reach ServerInstance->Config->MaxTargets number of targets, to stop abuse via spam.
 */
int CommandParser::LoopCall(User* user, Command* CommandObj, const std::vector<std::string>& parameters, unsigned int splithere, int extra, bool usemax)
{
	if (splithere >= parameters.size())
		return 0;

	if (extra >= (signed)parameters.size())
		extra = -1;

	/* First check if we have more than one item in the list, if we don't we return zero here and the handler
	 * which called us just carries on as it was.
	 */
	if (parameters[splithere].find(',') == std::string::npos)
		return 0;

	/** Some lame ircds will weed out dupes using some shitty O(n^2) algorithm.
	 * By using std::set (thanks for the idea w00t) we can cut this down a ton.
	 * ...VOOODOOOO!
	 */
	std::set<irc::string> dupes;

	/* Create two lists, one for channel names, one for keys
	 */
	irc::commasepstream items1(parameters[splithere]);
	irc::commasepstream items2(extra >= 0 ? parameters[extra] : "");
	std::string extrastuff;
	std::string item;
	unsigned int max = 0;

	/* Attempt to iterate these lists and call the command objech
	 * which called us, for every parameter pair until there are
	 * no more left to parse.
	 */
	while (items1.GetToken(item) && (!usemax || max++ < ServerInstance->Config->MaxTargets))
	{
		if (dupes.find(item.c_str()) == dupes.end())
		{
			std::vector<std::string> new_parameters(parameters);

			if (!items2.GetToken(extrastuff))
				extrastuff.clear();

			new_parameters[splithere] = item;
			if (extra >= 0)
				new_parameters[extra] = extrastuff;

			CommandObj->Handle(new_parameters, user);

			dupes.insert(item.c_str());
		}
	}
	return 1;
}

bool CommandParser::IsValidCommand(const std::string &commandname, unsigned int pcnt, User * user)
{
	Commandtable::iterator n = cmdlist.find(commandname);

	if (n != cmdlist.end())
	{
		if ((pcnt >= n->second->min_params))
		{
			if (IS_LOCAL(user) && n->second->flags_needed)
			{
				if (user->IsModeSet(n->second->flags_needed))
				{
					return (user->HasPermission(commandname));
				}
			}
			else
			{
				return true;
			}
		}
	}
	return false;
}

Command* CommandParser::GetHandler(const std::string &commandname)
{
	Commandtable::iterator n = cmdlist.find(commandname);
	if (n != cmdlist.end())
		return n->second;

	return NULL;
}

// calls a handler function for a command

CmdResult CommandParser::CallHandler(const std::string &commandname, const std::vector<std::string>& parameters, User *user)
{
	Commandtable::iterator n = cmdlist.find(commandname);

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

	/* Modify the user's penalty regardless of whether or not the command exists */
	if (!user->HasPrivPermission("users/flood/no-throttle"))
	{
		// If it *doesn't* exist, give it a slightly heftier penalty than normal to deter flooding us crap
		user->CommandFloodPenalty += handler ? handler->Penalty * 1000 : 2000;
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
				user->WriteNumeric(ERR_UNKNOWNCOMMAND, "%s %s :Unknown command",user->nick.c_str(),command.c_str());
			ServerInstance->stats->statsUnknown++;
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
			user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - You do not have the required operator privileges",user->nick.c_str());
			return;
		}

		if (!user->HasPermission(command))
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Oper type %s does not have access to command %s",
				user->nick.c_str(), user->oper->name.c_str(), command.c_str());
			return;
		}
	}

	if ((user->registered == REG_ALL) && (!user->IsOper()) && (handler->IsDisabled()))
	{
		/* command is disabled! */
		if (ServerInstance->Config->DisabledDontExist)
		{
			user->WriteNumeric(ERR_UNKNOWNCOMMAND, "%s %s :Unknown command",user->nick.c_str(),command.c_str());
		}
		else
		{
			user->WriteNumeric(ERR_UNKNOWNCOMMAND, "%s %s :This command has been disabled.",
										user->nick.c_str(), command.c_str());
		}

		ServerInstance->SNO->WriteToSnoMask('t', "%s denied for %s (%s@%s)",
				command.c_str(), user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		return;
	}

	if ((!command_p.empty()) && (command_p.back().empty()) && (!handler->allow_empty_last_param))
		command_p.pop_back();

	if (command_p.size() < handler->min_params)
	{
		user->WriteNumeric(ERR_NEEDMOREPARAMS, "%s %s :Not enough parameters.", user->nick.c_str(), command.c_str());
		if ((ServerInstance->Config->SyntaxHints) && (user->registered == REG_ALL) && (handler->syntax.length()))
			user->WriteNumeric(RPL_SYNTAX, "%s :SYNTAX %s %s", user->nick.c_str(), handler->name.c_str(), handler->syntax.c_str());
		return;
	}

	if ((user->registered != REG_ALL) && (!handler->WorksBeforeReg()))
	{
		user->WriteNumeric(ERR_NOTREGISTERED, "%s :You have not registered",command.c_str());
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

		FOREACH_MOD(I_OnPostCommand, OnPostCommand(handler, command_p, user, result, cmd));
	}
}

void CommandParser::RemoveCommand(Command* x)
{
	Commandtable::iterator n = cmdlist.find(x->name);
	if (n != cmdlist.end() && n->second == x)
		cmdlist.erase(n);
}

Command::~Command()
{
	ServerInstance->Parser->RemoveCommand(this);
}

void CommandParser::ProcessBuffer(std::string &buffer,LocalUser *user)
{
	if (!user || buffer.empty())
		return;

	ServerInstance->Logs->Log("USERINPUT", LOG_RAWIO, "C[%s] I :%s %s",
		user->uuid.c_str(), user->nick.c_str(), buffer.c_str());
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

int CommandParser::TranslateUIDs(const std::vector<TranslateType> to, const std::vector<std::string> &source, std::string &dest, bool prefix_final, Command* custom_translator)
{
	std::vector<TranslateType>::const_iterator types = to.begin();
	User* user = NULL;
	unsigned int i;
	int translations = 0;
	dest.clear();

	for(i=0; i < source.size(); i++)
	{
		TranslateType t;
		std::string item = source[i];

		if (types == to.end())
			t = TR_TEXT;
		else
		{
			t = *types;
			types++;
		}

		if (prefix_final && i == source.size() - 1)
			dest.append(":");

		switch (t)
		{
			case TR_NICK:
				/* Translate single nickname */
				user = ServerInstance->FindNick(item);
				if (user)
				{
					dest.append(user->uuid);
					translations++;
				}
				else
					dest.append(item);
			break;
			case TR_CUSTOM:
				if (custom_translator)
					custom_translator->EncodeParameter(item, i);
				dest.append(item);
			break;
			case TR_END:
			case TR_TEXT:
			default:
				/* Do nothing */
				dest.append(item);
			break;
		}
		if (i != source.size() - 1)
			dest.append(" ");
	}

	return translations;
}

int CommandParser::TranslateUIDs(TranslateType to, const std::string &source, std::string &dest)
{
	User* user = NULL;
	int translations = 0;
	dest.clear();

	switch (to)
	{
		case TR_NICK:
			/* Translate single nickname */
			user = ServerInstance->FindNick(source);
			if (user)
			{
				dest = user->uuid;
				translations++;
			}
			else
				dest = source;
		break;
		case TR_END:
		case TR_TEXT:
		default:
			/* Do nothing */
			dest = source;
		break;
	}

	return translations;
}
