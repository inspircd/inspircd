/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
#include "xline.h"
#include "socketengine.h"
#include "socket.h"
#include "command_parse.h"
#include "exitcodes.h"

/* Directory Searching for Unix-Only */
#ifndef WIN32
#include <dirent.h>
#include <dlfcn.h>
#endif

int InspIRCd::PassCompare(Extensible* ex, const std::string &data, const std::string &input, const std::string &hashtype)
{
	int MOD_RESULT = 0;
	FOREACH_RESULT_I(this,I_OnPassCompare,OnPassCompare(ex, data, input, hashtype))

	/* Module matched */
	if (MOD_RESULT == 1)
		return 0;

	/* Module explicitly didnt match */
	if (MOD_RESULT == -1)
		return 1;

	/* We dont handle any hash types except for plaintext - Thanks tra26 */
	if (hashtype != "" && hashtype != "plaintext")
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
int CommandParser::LoopCall(User* user, Command* CommandObj, const std::vector<std::string>& parameters, unsigned int splithere, unsigned int extra)
{
	if (splithere >= parameters.size())
		return 0;

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
	irc::commasepstream items2(parameters[extra]);
	std::string extrastuff;
	std::string item;
	unsigned int max = 0;

	/* Attempt to iterate these lists and call the command objech
	 * which called us, for every parameter pair until there are
	 * no more left to parse.
	 */
	while (items1.GetToken(item) && (max++ < ServerInstance->Config->MaxTargets))
	{
		if (dupes.find(item.c_str()) == dupes.end())
		{
			std::vector<std::string> new_parameters;

			for (unsigned int t = 0; (t < parameters.size()) && (t < MAXPARAMETERS); t++)
				new_parameters.push_back(parameters[t]);

			if (!items2.GetToken(extrastuff))
				extrastuff = "";

			new_parameters[splithere] = item.c_str();
			new_parameters[extra] = extrastuff.c_str();

			CommandObj->Handle(new_parameters, user);

			dupes.insert(item.c_str());
		}
	}
	return 1;
}

int CommandParser::LoopCall(User* user, Command* CommandObj, const std::vector<std::string>& parameters, unsigned int splithere)
{
	if (splithere >= parameters.size())
		return 0;

	/* First check if we have more than one item in the list, if we don't we return zero here and the handler
	 * which called us just carries on as it was.
	 */
	if (parameters[splithere].find(',') == std::string::npos)
		return 0;

	std::set<irc::string> dupes;

	/* Only one commasepstream here */
	irc::commasepstream items1(parameters[splithere]);
	std::string item;
	unsigned int max = 0;

	/* Parse the commasepstream until there are no tokens remaining.
	 * Each token we parse out, call the command handler that called us
	 * with it
	 */
	while (items1.GetToken(item) && (max++ < ServerInstance->Config->MaxTargets))
	{
		if (dupes.find(item.c_str()) == dupes.end())
		{
			std::vector<std::string> new_parameters;

			for (unsigned int t = 0; (t < parameters.size()) && (t < MAXPARAMETERS); t++)
				new_parameters.push_back(parameters[t]);

			new_parameters[splithere] = item.c_str();

			/* Execute the command handler. */
			CommandObj->Handle(new_parameters, user);

			dupes.insert(item.c_str());
		}
	}
	/* By returning 1 we tell our caller that nothing is to be done,
	 * as all the previous calls handled the data. This makes the parent
	 * return without doing any processing.
	 */
	return 1;
}

bool CommandParser::IsValidCommand(const std::string &commandname, unsigned int pcnt, User * user)
{
	Commandtable::iterator n = cmdlist.find(commandname);

	if (n != cmdlist.end())
	{
		if ((pcnt >= n->second->min_params) && (n->second->source != "<core>"))
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

void CommandParser::DoLines(User* current, bool one_only)
{
	while (current->BufferIsReady())
	{
		// use GetBuffer to copy single lines into the sanitized string
		std::string single_line = current->GetBuffer();
		current->bytes_in += single_line.length();
		current->cmds_in++;
		if (single_line.length() > MAXBUF - 2)  // MAXBUF is 514 to allow for neccessary line terminators
			single_line.resize(MAXBUF - 2); // So to trim to 512 here, we use MAXBUF - 2

		// ProcessBuffer returns false if the user has gone over penalty
		if (!ServerInstance->Parser->ProcessBuffer(single_line, current) || one_only)
			break;
	}
}

bool CommandParser::ProcessCommand(User *user, std::string &cmd)
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

	while (tokens.GetToken(token) && (command_p.size() <= MAXPARAMETERS))
		command_p.push_back(token);

	std::transform(command.begin(), command.end(), command.begin(), ::toupper);

	/* find the command, check it exists */
	Commandtable::iterator cm = cmdlist.find(command);

	if (cm == cmdlist.end())
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnPreCommand,OnPreCommand(command, command_p, user, false, cmd));
		if (MOD_RESULT == 1)
			return true;

		/*
		 * This double lookup is in case a module (abbreviation) wishes to change a command.
		 * Sure, the double lookup is a bit painful, but bear in mind this only happens for unknowns anyway.
		 *
		 * Thanks dz for making me actually understand why this is necessary!
		 * -- w00t
		 */
		cm = cmdlist.find(command);
		if (cm == cmdlist.end())
		{
			if (user->registered == REG_ALL)
				user->WriteNumeric(ERR_UNKNOWNCOMMAND, "%s %s :Unknown command",user->nick.c_str(),command.c_str());
			ServerInstance->stats->statsUnknown++;
			return true;
		}
	}

	if (cm->second->max_params && command_p.size() > cm->second->max_params)
	{
		/*
		 * command_p input (assuming max_params 1):
		 *	this
		 *	is
		 *	a
		 *	test
		 */
		std::string lparam = "";

		/*
		 * The '-1' here is a clever trick, we'll go backwards throwing everything into a temporary param
		 * and then just toss that into the array.
		 * -- w00t
		 */
		while (command_p.size() > (cm->second->max_params - 1))
		{
			// BE CAREFUL: .end() returns past the end of the vector, hence decrement.
			std::vector<std::string>::iterator it = --command_p.end();

			lparam.insert(0, " " + *(it));
			command_p.erase(it); // remove last element
		}

		/* we now have (each iteration):
		 *	' test'
		 *	' a test'
		 *	' is a test' <-- final string
		 * ...now remove the ' ' at the start...
		 */
		lparam.erase(lparam.begin());

		/* param is now 'is a test', which is exactly what we wanted! */
		command_p.push_back(lparam);
	}

	/*
	 * We call OnPreCommand here seperately if the command exists, so the magic above can
	 * truncate to max_params if necessary. -- w00t
	 */
	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnPreCommand,OnPreCommand(command, command_p, user, false, cmd));
	if (MOD_RESULT == 1)
		return true;

	/* Modify the user's penalty */
	bool do_more = true;
	if (!user->HasPrivPermission("users/flood/no-throttle"))
	{
		user->IncreasePenalty(cm->second->Penalty);
		do_more = (user->Penalty < 10);
	}

	/* activity resets the ping pending timer */
	if (user->MyClass)
		user->nping = ServerInstance->Time() + user->MyClass->GetPingTime();

	if (cm->second->flags_needed)
	{
		if (!user->IsModeSet(cm->second->flags_needed))
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - You do not have the required operator privileges",user->nick.c_str());
			return do_more;
		}
		if (!user->HasPermission(command))
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Oper type %s does not have access to command %s",user->nick.c_str(),user->oper.c_str(),command.c_str());
			return do_more;
		}
	}
	if ((user->registered == REG_ALL) && (!IS_OPER(user)) && (cm->second->IsDisabled()))
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
		return do_more;
	}
	if (command_p.size() < cm->second->min_params)
	{
		user->WriteNumeric(ERR_NEEDMOREPARAMS, "%s %s :Not enough parameters.", user->nick.c_str(), command.c_str());
		if ((ServerInstance->Config->SyntaxHints) && (user->registered == REG_ALL) && (cm->second->syntax.length()))
			user->WriteNumeric(RPL_SYNTAX, "%s :SYNTAX %s %s", user->nick.c_str(), cm->second->command.c_str(), cm->second->syntax.c_str());
		return do_more;
	}
	if ((user->registered != REG_ALL) && (!cm->second->WorksBeforeReg()))
	{
		user->WriteNumeric(ERR_NOTREGISTERED, "%s :You have not registered",command.c_str());
		return do_more;
	}
	else
	{
		/* passed all checks.. first, do the (ugly) stats counters. */
		cm->second->use_count++;
		cm->second->total_bytes += cmd.length();

		/* module calls too */
		MOD_RESULT = 0;
		FOREACH_RESULT(I_OnPreCommand,OnPreCommand(command, command_p, user, true, cmd));
		if (MOD_RESULT == 1)
			return do_more;

		/*
		 * WARNING: be careful, the user may be deleted soon
		 */
		CmdResult result = cm->second->Handle(command_p, user);

		FOREACH_MOD(I_OnPostCommand,OnPostCommand(command, command_p, user, result,cmd));
		return do_more;
	}
}

void CommandParser::RemoveCommands(const char* source)
{
	Commandtable::iterator i,safei;
	for (i = cmdlist.begin(); i != cmdlist.end();)
	{
		safei = i;
		i++;
		RemoveCommand(safei, source);
	}
}

void CommandParser::RemoveCommand(Commandtable::iterator safei, const char* source)
{
	Command* x = safei->second;
	if (x->source == std::string(source))
	{
		cmdlist.erase(safei);
		delete x;
	}
}

bool CommandParser::ProcessBuffer(std::string &buffer,User *user)
{
	std::string::size_type a;

	if (!user)
		return true;

	while ((a = buffer.rfind("\n")) != std::string::npos)
		buffer.erase(a);
	while ((a = buffer.rfind("\r")) != std::string::npos)
		buffer.erase(a);

	if (buffer.length())
	{
		ServerInstance->Logs->Log("USERINPUT", DEBUG,"C[%d] I :%s %s",user->GetFd(), user->nick.c_str(), buffer.c_str());
		return this->ProcessCommand(user,buffer);
	}

	return true;
}

bool CommandParser::CreateCommand(Command *f, void* so_handle)
{
	if (so_handle)
	{
		if (RFCCommands.find(f->command) == RFCCommands.end())
			RFCCommands[f->command] = so_handle;
		else
		{
			ServerInstance->Logs->Log("COMMAND",DEFAULT,"ERK! Somehow, we loaded a cmd_*.so file twice! Only the first instance is being recorded.");
			return false;
		}
	}

	/* create the command and push it onto the table */
	if (cmdlist.find(f->command) == cmdlist.end())
	{
		cmdlist[f->command] = f;
		return true;
	}
	else return false;
}

CommandParser::CommandParser(InspIRCd* Instance) : ServerInstance(Instance)
{
	para.resize(128);
}

bool CommandParser::FindSym(void** v, void* h, const std::string &name)
{
	*v = dlsym(h, "init_command");
	const char* err = dlerror();
	if (err && !(*v))
	{
		ServerInstance->Logs->Log("COMMAND",SPARSE, "Error loading core command %s: %s\n", name.c_str(), err);
		return false;
	}
	return true;
}

bool CommandParser::ReloadCommand(std::string cmd, User* user)
{
	char filename[MAXBUF];
	std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

	SharedObjectList::iterator command = RFCCommands.find(cmd);

	if (command != RFCCommands.end())
	{
		Command* cmdptr = cmdlist.find(cmd)->second;
		cmdlist.erase(cmdlist.find(cmd));

		RFCCommands.erase(cmd);
		delete cmdptr;
		dlclose(command->second);
	}

	std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
	snprintf(filename, MAXBUF, "cmd_%s.so", cmd.c_str());
	const char* err = this->LoadCommand(filename);
	if (err)
	{
		if (user)
			user->WriteServ("NOTICE %s :*** Error loading '%s': %s", user->nick.c_str(), filename, err);
		return false;
	}
	return true;
}

CmdResult CommandReload::Handle(const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() < 1)
		return CMD_FAILURE;

	user->WriteServ("NOTICE %s :*** Reloading command '%s'",user->nick.c_str(), parameters[0].c_str());
	if (ServerInstance->Parser->ReloadCommand(parameters[0], user))
	{
		user->WriteServ("NOTICE %s :*** Successfully reloaded command '%s'", user->nick.c_str(), parameters[0].c_str());
		ServerInstance->SNO->WriteToSnoMask('a', "RELOAD: %s reloaded the '%s' command.", user->nick.c_str(), parameters[0].c_str());
		return CMD_SUCCESS;
	}
	else
	{
		user->WriteServ("NOTICE %s :*** Could not reload command '%s'. The command will not work until reloaded successfully.", user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}
}

const char* CommandParser::LoadCommand(const char* name)
{
	char filename[MAXBUF];
	void* h;
	Command* (*cmd_factory_func)(InspIRCd*);

	/* Command already exists? Succeed silently - this is needed for REHASH */
	if (RFCCommands.find(name) != RFCCommands.end())
	{
		ServerInstance->Logs->Log("COMMAND",DEBUG,"Not reloading command %s/%s, it already exists", LIBRARYDIR, name);
		return NULL;
	}

	snprintf(filename, MAXBUF, "%s/%s", LIBRARYDIR, name);
	h = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);

	if (!h)
	{
		const char* n = dlerror();
		ServerInstance->Logs->Log("COMMAND",SPARSE, "Error loading core command %s: %s", name, n);
		return n;
	}

	if (this->FindSym((void **)&cmd_factory_func, h, name))
	{
		Command* newcommand = cmd_factory_func(ServerInstance);
		this->CreateCommand(newcommand, h);
	}
	return NULL;
}

/** This is only invoked on startup
 */
void CommandParser::SetupCommandTable()
{
	printf("\nLoading core commands");
	fflush(stdout);

	DIR* library = opendir(LIBRARYDIR);
	if (library)
	{
		dirent* entry = NULL;
		while (0 != (entry = readdir(library)))
		{
			if (InspIRCd::Match(entry->d_name, "cmd_*.so", ascii_case_insensitive_map))
			{
				printf(".");
				fflush(stdout);

				const char* err = this->LoadCommand(entry->d_name);
				if (err)
				{
					printf("Error loading %s: %s", entry->d_name, err);
					exit(EXIT_STATUS_BADHANDLER);
				}
			}
		}
		closedir(library);
		printf("\n");
	}

	if (cmdlist.find("RELOAD") == cmdlist.end())
		this->CreateCommand(new CommandReload(ServerInstance));
}

int CommandParser::TranslateUIDs(const std::deque<TranslateType> to, const std::deque<std::string> &source, std::string &dest)
{
	std::deque<std::string>::const_iterator items = source.begin();
	std::deque<TranslateType>::const_iterator types = to.begin();
	User* user = NULL;
	int translations = 0;
	dest.clear();

	while (items != source.end() && types != to.end())
	{
		TranslateType t = *types;
		std::string item = *items;
		types++;
		items++;

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
			break;
			case TR_END:
			case TR_TEXT:
			default:
				/* Do nothing */
				dest.append(item);
			break;
		}
		dest.append(" ");
	}

	if (!dest.empty())
		dest.erase(dest.end() - 1);
	return translations;
}

int CommandParser::TranslateUIDsOnce(TranslateType to, const std::string &source, std::string &dest)
{
	User* user = NULL;
	std::string item;
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
