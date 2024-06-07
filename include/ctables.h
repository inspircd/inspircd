/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012-2015, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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
