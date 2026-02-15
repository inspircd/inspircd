/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012-2015, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003, 2006-2009 Craig Edwards <brain@inspircd.org>
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


#pragma once

/** Used to indicate the type of a command. */
enum class CmdAccess
	: uint8_t
{
	/* The command has no special attributes. */
	NORMAL = 0,

	/** Only server operators can use the command. */
	OPERATOR = 1,

	/** Only servers can use the command. */
	SERVER = 2,
};

/** Used to indicate the result of trying to execute a command. */
enum class CmdResult
	: uint8_t
{
	/** The command exists and its execution succeeded. */
	SUCCESS = 0,

	/** The command exists but its execution failed. */
	FAILURE = 1,

	/* The command does not exist. */
	INVALID = 2,
};

/** Translation types for translation of parameters to UIDs.
 * This allows the core commands to not have to be aware of how UIDs
 * work (making it still possible to write other linking modules which
 * do not use UID (but why would you want to?)
 */
enum TranslateType
{
	TR_TEXT,		/* Raw text, leave as-is */
	TR_NICK,		/* Nickname, translate to UUID for server->server */
	TR_CUSTOM		/* Custom translation handled by EncodeParameter/DecodeParameter */
};

/** The type of routes that a message can take. */
enum class RouteType
	: uint8_t
{
	/** The message is only routed to the local server. */
	LOCAL,

	/** The message is routed to all servers. */
	BROADCAST,

	/** The message is routed to all servers using ENCAP. */
	OPTIONAL_BROADCAST,

	/** The message is routed to a specific remote server. */
	UNICAST,

	/** The message is routed to a specific remote server using ENCAP. */
	OPTIONAL_UNICAST,

	/** The message is routed to a specific user, channel, or server mask. */
	MESSAGE,
};

/** Describes the routing of an IRC message. */
class RouteDescriptor final
{
public:
	/** The target of the message in question. */
	const std::string target;

	/** The type of route that the message should take. */
	const RouteType type;

	/** For unicast messages a specific server that the message should be routed to. */
	const Server* server;

	/** Creates a new route descriptor with the specified route type and target string. */
	RouteDescriptor(RouteType rt, const std::string& t)
		: target(t)
		, type(rt)
		, server(nullptr)
	{
	}

	RouteDescriptor(RouteType rt, const Server* s = nullptr)
		: type(rt)
		, server(s)
	{
	}
};

/** The message is only routed to the local server. */
#define ROUTE_LOCALONLY (RouteDescriptor(RouteType::LOCAL))

/** The message is routed to all servers. */
#define ROUTE_BROADCAST (RouteDescriptor(RouteType::BROADCAST))

/** The message is routed to all servers using ENCAP. */
#define ROUTE_OPT_BCAST (RouteDescriptor(RouteType::OPTIONAL_BROADCAST))

/** The message is routed to a specific remote server. */
#define ROUTE_UNICAST(dest) (RouteDescriptor(RouteType::UNICAST, dest))

/** The message is routed to a specific remote server using ENCAP. */
#define ROUTE_OPT_UCAST(dest) (RouteDescriptor(RouteType::OPTIONAL_UNICAST, dest))

/** The message is routed to a specific user, channel, or server mask. */
#define ROUTE_MESSAGE(dest) (RouteDescriptor(RouteType::MESSAGE, dest))

/** A structure that defines a command. Every command available
 * in InspIRCd must be defined as derived from Command.
 */
class CoreExport CommandBase
	: public ServiceProvider
{
public:
	/** Encapsulates parameters to a command. */
	class Params : public std::vector<std::string>
	{
	private:
		/* IRCv3 message tags. */
		ClientProtocol::TagMap tags;

	public:
		/** Initializes a new instance from parameter and tag references.
		 * @param paramsref Message parameters.
		 * @param tagsref IRCv3 message tags.
		 */
		Params(const std::vector<std::string>& paramsref, const ClientProtocol::TagMap& tagsref)
			: std::vector<std::string>(paramsref)
			, tags(tagsref)
		{
		}

		/** Initializes a new instance from parameter iterators.
		 * @param first The first element in the parameter array.
		 * @param last The last element in the parameter array.
		 */
		template<typename Iterator>
		Params(Iterator first, Iterator last)
			: std::vector<std::string>(first, last)
		{
		}

		/** Initializes a new empty instance. */
		Params() = default;

		/** Retrieves the IRCv3 message tags. */
		const ClientProtocol::TagMap& GetTags() const { return tags; }
		ClientProtocol::TagMap& GetTags() { return tags; }
	};

	/** Minimum number of parameters command takes
	*/
	const unsigned int min_params;

	/** Maximum number of parameters command takes.
	 * This is used by the command parser to join extra parameters into one last param.
	 * If not set, no munging is done to this command.
	 */
	const unsigned int max_params;

	/** True if the command allows an empty last parameter.
	 * When false and the last parameter is empty, it's popped BEFORE
	 * checking there are enough params, etc. (i.e. the handler won't
	 * be called if there aren't enough params after popping the empty
	 * param).
	 */
	bool allow_empty_last_param = false;

	/** Translation type list for possible parameters, used to tokenize
	 * parameters into UIDs and SIDs etc.
	 */
	std::vector<TranslateType> translation;

	/** Create a new command.
	 * @param me The module which created this command.
	 * @param cmd Command name. This must be UPPER CASE.
	 * @param minpara Minimum parameters required for the command.
	 * @param maxpara Maximum number of parameters this command may have - extra parameters
	 * will be tossed into one last space-separated param.
	 */
	CommandBase(Module* me, const std::string& cmd, unsigned int minpara = 0, unsigned int maxpara = 0);

	virtual RouteDescriptor GetRouting(User* user, const CommandBase::Params& parameters);

	/** Encode a parameter for server->server transmission.
	 * Used for parameters for which the translation type is TR_CUSTOM.
	 * @param parameter The parameter to encode. Can be modified in place.
	 * @param index The parameter index (0 == first parameter).
	 */
	virtual void EncodeParameter(std::string& parameter, unsigned int index);
};

class CoreExport Command
	: public CommandBase
{
protected:
	/** Initializes a new instance of the Command class.
	 * @param me The module which created this instance.
	 * @param cmd The name of the command.
	 * @param minpara The minimum number of parameters that the command accepts.
	 * @param maxpara The maximum number of parameters that the command accepts.
	 */
	Command(Module* me, const std::string& cmd, unsigned int minpara = 0, unsigned int maxpara = 0);

public:
	/** Unregisters this command from the command parser. */
	~Command() override;

	/** Who can access this command? */
	CmdAccess access_needed = CmdAccess::NORMAL;

	/** Whether the command will not be forwarded by the linking module even if it comes via ENCAP. */
	bool force_manual_route = false;

	/** The number of milliseconds worth of penalty that executing this command gives. */
	unsigned int penalty = 1000;

	/** The number of times this command has been executed. */
	unsigned long use_count = 0;

	/** If non-empty then the syntax of the parameter for this command. */
	std::vector<std::string> syntax;

	/** Whether the command can be issued before registering. */
	bool works_before_reg = false;

	/** Handle the command from a user.
	 * @param user The user who issued the command.
	 * @param parameters The parameters for the command.
	 * @return Returns CmdResult::FAILURE on failure, CmdResult::SUCCESS on success, or CmdResult::INVALID
	 *         if the command was malformed.
	 */
	virtual CmdResult Handle(User* user, const Params& parameters) = 0;

	/** Registers this command with the command parser. */
	void RegisterService() override;

	/** Tells the user they did not specify enough parameters.
	 * @param user The user who issued the command.
	 * @param parameters The parameters for the command.
	 */
	virtual void TellNotEnoughParameters(LocalUser* user, const Params& parameters);

	/** Tells the user they need to be fully connected to execute this command.
	 * @param user The user who issued the command.
	 * @param parameters The parameters for the command.
	 */
	virtual void TellNotFullyConnected(LocalUser* user, const Params& parameters);
};

class CoreExport SplitCommand
	: public Command
{
protected:
	/** Initializes a new instance of the SplitCommand class.
	 * @param me The module which created this instance.
	 * @param cmd The name of the command.
	 * @param minpara The minimum number of parameters that the command accepts.
	 * @param maxpara The maximum number of parameters that the command accepts.
	 */
	SplitCommand(Module* me, const std::string& cmd, unsigned int minpara = 0, unsigned int maxpara = 0);

public:
	/** @copydoc Command::Handle */
	CmdResult Handle(User* user, const Params& parameters) override;

	/** Handle the command from a local user.
	 * @param user The user who issued the command.
	 * @param parameters The parameters for the command.
	 * @return Returns CmdResult::FAILURE on failure, CmdResult::SUCCESS on success, or CmdResult::INVALID
	 *         if the command was malformed.
	 */
	virtual CmdResult HandleLocal(LocalUser* user, const Params& parameters);

	/** Handle the command from a remote user.
	 * @param user The user who issued the command.
	 * @param parameters The parameters for the command.
	 * @return Returns CmdResult::FAILURE on failure, CmdResult::SUCCESS on success, or CmdResult::INVALID
	 *         if the command was malformed.
	 */
	virtual CmdResult HandleRemote(RemoteUser* user, const Params& parameters);

	/** Handle the command from a server user.
	 * @param user The user who issued the command.
	 * @param parameters The parameters for the command.
	 * @return Returns CmdResult::FAILURE on failure, CmdResult::SUCCESS on success, or CmdResult::INVALID
	 *         if the command was malformed.
	 */
	virtual CmdResult HandleServer(FakeUser* user, const Params& parameters);
};

/** This class handles command management and parsing.
 * It allows you to add and remove commands from the map,
 * call command handlers by name, and chop up comma separated
 * parameters into multiple calls.
 */
class CoreExport CommandParser final
{
public:
	typedef std::unordered_map<std::string, Command*, irc::insensitive, irc::StrHashComp> CommandMap;

private:
	/** Command list, a hash_map of command names to Command*
	 */
	CommandMap cmdlist;

public:
	/** Get a command name -> Command* map containing all client to server commands
	 * @return A map of command handlers keyed by command names
	 */
	const CommandMap& GetCommands() const { return cmdlist; }

	/** Calls the handler for a given command.
	 * @param commandname The command to find. This should be in uppercase.
	 * @param parameters Parameter list
	 * @param user The user to call the handler on behalf of
	 * @param cmd If non-NULL and the command was executed it is set to the command handler,
	 * otherwise it isn't written to.
	 * @return This method will return CmdResult::SUCCESS if the command handler was found and called,
	 * and the command completeld successfully. It will return CmdResult::FAILURE if the command handler was found
	 * and called, but the command did not complete successfully, and it will return CmdResult::INVALID if the
	 * command simply did not exist at all or the wrong number of parameters were given, or the user
	 * was not privileged enough to execute the command.
	 */
	CmdResult CallHandler(const std::string& commandname, const CommandBase::Params& parameters, User* user, Command** cmd = nullptr);

	/** Get the handler function for a command.
	 * @param commandname The command required. Always use uppercase for this parameter.
	 * @return a pointer to the command handler, or NULL
	 */
	Command* GetHandler(const std::string& commandname);

	/** LoopCall is used to call a command handler repeatedly based on the contents of a comma separated list.
	 * There are two ways to call this method, either with one potential list or with two potential lists.
	 * We need to handle two potential lists for JOIN, because a JOIN may contain two lists of items at once:
	 * the channel names and their keys as follows:
	 *
	 * JOIN \#chan1,\#chan2,\#chan3 key1,,key3
	 *
	 * Therefore, we need to deal with both lists concurrently. If there are two lists then the method reads
	 * them both together until the first runs out of tokens.
	 * With one list it is much simpler, and is used in NAMES, WHOIS, PRIVMSG etc.
	 *
	 * If there is only one list and there are duplicates in it, then the command handler is only called for
	 * unique items. Entries are compared using "irc comparison".
	 * If the usemax parameter is true (the default) the function only parses until it reaches
	 * ServerInstance->Config->MaxTargets number of targets, to stop abuse via spam.
	 *
	 * The OnPostCommand hook is executed for each item after it has been processed by the handler, with the
	 * original line parameter being empty (to indicate that the command in that form was created by this function).
	 * This only applies if the user executing the command is local.
	 *
	 * If there are two lists and the second list runs out of tokens before the first list then parameters[extra]
	 * will be an EMPTY string when Handle() is called for the remaining tokens in the first list, even if it is
	 * in the middle of parameters[]! Moreover, empty tokens in the second list are allowed, and those will also
	 * result in the appropriate entry being empty in parameters[].
	 * This is different than what command handlers usually expect; the command parser only allows an empty param
	 * as the last item in the vector.
	 *
	 * @param user The user who sent the command
	 * @param handler The command handler to call for each parameter in the list
	 * @param parameters Parameter list as a vector of strings
	 * @param splithere The first parameter index to split as a comma separated list
	 * @param extra The second parameter index to split as a comma separated list, or -1 (the default) if there is only one list
	 * @param usemax True to limit the command to MaxTargets targets (default), or false to process all tokens
	 * @return This function returns true when it identified a list in the given parameter and finished calling the
	 * command handler for each entry on the list. When this occurs, the caller should return without doing anything,
	 * otherwise it should continue into its main section of code.
	 */
	static bool LoopCall(User* user, Command* handler, const CommandBase::Params& parameters, unsigned int splithere, int extra = -1, bool usemax = true);

	/** Take a raw input buffer from a recvq, and process it on behalf of a user.
	 * @param buffer The buffer line to process
	 * @param user The user to whom this line belongs
	 */
	void ProcessBuffer(LocalUser* user, const std::string& buffer);

	/** Process a command from a user.
	 * @param user The user to parse the command for.
	 * @param command The name of the command.
	 * @param parameters The parameters to the command.
	 */
	void ProcessCommand(LocalUser* user, std::string& command, CommandBase::Params& parameters);

	/** Add a new command to the commands hash
	 * @param f The new Command to add to the list
	 * @return True if the command was added
	 */
	bool AddCommand(Command* f);

	/** Removes a command.
	 */
	void RemoveCommand(Command* x);

	/** Translate a single item based on the TranslationType given.
	 * @param to The translation type to use for the process
	 * @param item The input string
	 * @param dest The output string. The translation result will be appended to this string
	 * @param custom_translator Used to translate the parameter if the translation type is TR_CUSTOM, if NULL, TR_CUSTOM will act like TR_TEXT
	 * @param paramnumber The index of the parameter we are translating.
	 */
	static void TranslateSingleParam(TranslateType to, const std::string& item, std::string& dest, CommandBase* custom_translator = nullptr, unsigned int paramnumber = 0);

	/** Translate nicknames in a list of strings into UIDs, based on the TranslateTypes given.
	 * @param to The translation types to use for the process. If this list is too short, TR_TEXT is assumed for the rest.
	 * @param source The strings to translate
	 * @param prefix_final True if the final source argument should have a colon prepended (if it could contain a space)
	 * @param custom_translator Used to translate the parameter if the translation type is TR_CUSTOM, if NULL, TR_CUSTOM will act like TR_TEXT
	 * @return dest The output string
	 */
	static std::string TranslateUIDs(const std::vector<TranslateType>& to, const CommandBase::Params& source, bool prefix_final = false, CommandBase* custom_translator = nullptr);
};
