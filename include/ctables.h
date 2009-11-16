/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CTABLES_H__
#define __CTABLES_H__

/** Used to indicate command success codes
 */
enum CmdResult
{
	CMD_FAILURE = 0,	/* Command exists, but failed */
	CMD_SUCCESS = 1,	/* Command exists, and succeeded */
	CMD_INVALID = 2		/* Command doesnt exist at all! */
};

/** Translation types for translation of parameters to UIDs.
 * This allows the core commands to not have to be aware of how UIDs
 * work (making it still possible to write other linking modules which
 * do not use UID (but why would you want to?)
 */
enum TranslateType
{
	TR_END,			/* End of known parameters, everything after this is TR_TEXT */
	TR_TEXT,		/* Raw text, leave as-is */
	TR_NICK,		/* Nickname, translate to UUID for server->server */
	TR_CUSTOM		/* Custom translation handled by EncodeParameter/DecodeParameter */
};

/** Routing types for a command. Any command which is created defaults
 * to having its command broadcasted on success. This behaviour may be
 * overridden to one of the route types shown below (see the #defines
 * below for more information on each one's behaviour)
 */
enum RouteType
{
	ROUTE_TYPE_LOCALONLY,
	ROUTE_TYPE_BROADCAST,
	ROUTE_TYPE_UNICAST,
	ROUTE_TYPE_MESSAGE,
	ROUTE_TYPE_OPT_BCAST,
	ROUTE_TYPE_OPT_UCAST
};

/** Defines routing information for a command, containing a destination
 * server id (if applicable) and a routing type from the enum above.
 */
struct RouteDescriptor
{
	/** Routing type from the enum above
	 */
	RouteType type;
	/** For unicast, the destination server's name
	 */
	std::string serverdest;

	/** Create a RouteDescriptor
	 */
	RouteDescriptor(RouteType t, const std::string &d)
		: type(t), serverdest(d) { }
};

/** Do not route this command */
#define ROUTE_LOCALONLY (RouteDescriptor(ROUTE_TYPE_LOCALONLY, ""))
/** Route this command to all servers, fail if not understood */
#define ROUTE_BROADCAST (RouteDescriptor(ROUTE_TYPE_BROADCAST, ""))
/** Route this command to a single server (do nothing if own server name specified) */
#define ROUTE_UNICAST(x) (RouteDescriptor(ROUTE_TYPE_UNICAST, x))
/** Route this command as a message with the given target (any of user, #channel, @#channel, $servermask) */
#define ROUTE_MESSAGE(x) (RouteDescriptor(ROUTE_TYPE_MESSAGE, x))
/** Route this command to all servers wrapped via ENCAP, so ignored if not understood */
#define ROUTE_OPT_BCAST (RouteDescriptor(ROUTE_TYPE_OPT_BCAST, ""))
/** Route this command to a single server wrapped via ENCAP, so ignored if not understood */
#define ROUTE_OPT_UCAST(x) (RouteDescriptor(ROUTE_TYPE_OPT_UCAST, x))

/** A structure that defines a command. Every command available
 * in InspIRCd must be defined as derived from Command.
 */
class CoreExport Command : public ServiceProvider
{
 public:
	/** User flags needed to execute the command or 0
	 */
	char flags_needed;

	/** Minimum number of parameters command takes
	*/
	const unsigned int min_params;

	/** Maximum number of parameters command takes.
	 * This is used by the command parser to join extra parameters into one last param.
	 * If not set, no munging is done to this command.
	 */
	const unsigned int max_params;

	/** used by /stats m
	 */
	long double use_count;

	/** used by /stats m
	 */
	long double total_bytes;

	/** True if the command is disabled to non-opers
	 */
	bool disabled;

	/** True if the command can be issued before registering
	 */
	bool works_before_reg;

	/** Syntax string for the command, displayed if non-empty string.
	 * This takes place of the text in the 'not enough parameters' numeric.
	 */
	std::string syntax;

	/** Translation type list for possible parameters, used to tokenize
	 * parameters into UIDs and SIDs etc.
	 */
	std::vector<TranslateType> translation;

	/** How many seconds worth of penalty does this command have?
	 */
	int Penalty;

	/** Create a new command.
	 * @param Instance Pointer to creator class
	 * @param cmd Command name. This must be UPPER CASE.
	 * @param flags User mode required to execute the command. May ONLY be one mode - it's a string to give warnings if people mix params up.
	 * For oper only commands, set this to 'o', otherwise use 0.
	 * @param minpara Minimum parameters required for the command.
	 * @param maxpara Maximum number of parameters this command may have - extra parameters will be tossed into one last space-seperated param.
	 * @param before_reg If this is set to true, the command will
	 * be allowed before the user is 'registered' (has sent USER,
	 * NICK, optionally PASS, and been resolved).
	 */
	Command(Module* me, const std::string &cmd, int minpara = 0, int maxpara = 0) :
		ServiceProvider(me, cmd, SERVICE_COMMAND), flags_needed(0), min_params(minpara), max_params(maxpara),
		use_count(0), total_bytes(0), disabled(false), works_before_reg(false), Penalty(1)
	{
	}

	/** Handle the command from a user.
	 * @param parameters The parameters for the command.
	 * @param user The user who issued the command.
	 * @return Return CMD_SUCCESS on success, or CMD_FAILURE on failure.
	 */
	virtual CmdResult Handle(const std::vector<std::string>& parameters, User* user) = 0;

	virtual RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_LOCALONLY;
	}

	/** Encode a parameter for server->server transmission.
	 * Used for parameters for which the translation type is TR_CUSTOM.
	 * @param parameter The parameter to encode. Can be modified in place.
	 * @param index The parameter index (0 == first parameter).
	 */
	virtual void EncodeParameter(std::string& parameter, int index)
	{
	}

	/** Decode a parameter from server->server transmission.
	 * Not currently used in this version of InspIRCd.
	 * Used for parameters for which the translation type is TR_CUSTOM.
	 * @param parameter The parameter to decode. Can be modified in place.
	 * @param index The parameter index (0 == first parameter).
	 */
	virtual void DecodeParameter(std::string& parameter, int index)
	{
	}

	/** Disable or enable this command.
	 * @param setting True to disable the command.
	 */
	void Disable(bool setting)
	{
		disabled = setting;
	}

	/** Obtain this command's disable state.
	 * @return true if the command is currently disabled
	 * (disabled commands can be used only by operators)
	 */
	bool IsDisabled()
	{
		return disabled;
	}

	/** @return true if the command works before registration.
	 */
	bool WorksBeforeReg()
	{
		return works_before_reg;
	}

	virtual ~Command();
};

class CoreExport SplitCommand : public Command
{
 public:
	SplitCommand(Module* me, const std::string &cmd, int minpara = 0, int maxpara = 0)
		: Command(me, cmd, minpara, maxpara) {}
	virtual CmdResult Handle(const std::vector<std::string>& parameters, User* user);
	virtual CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user);
	virtual CmdResult HandleRemote(const std::vector<std::string>& parameters, RemoteUser* user);
	virtual CmdResult HandleServer(const std::vector<std::string>& parameters, FakeUser* user);
};

/** Shortcut macros for defining translation lists
 */
#define TRANSLATE1(x1)	translation.push_back(x1);
#define TRANSLATE2(x1,x2)  translation.push_back(x1);translation.push_back(x2);
#define TRANSLATE3(x1,x2,x3)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);
#define TRANSLATE4(x1,x2,x3,x4)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);translation.push_back(x4);
#define TRANSLATE5(x1,x2,x3,x4,x5)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);translation.push_back(x4);\
	translation.push_back(x5);
#define TRANSLATE6(x1,x2,x3,x4,x5,x6)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);translation.push_back(x4);\
	translation.push_back(x5);translation.push_back(x6);
#define TRANSLATE7(x1,x2,x3,x4,x5,x6,x7)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);translation.push_back(x4);\
	translation.push_back(x5);translation.push_back(x6);translation.push_back(x7);
#define TRANSLATE8(x1,x2,x3,x4,x5,x6,x7,x8)  translation.push_back(x1);translation.push_back(x2);translation.push_back(x3);translation.push_back(x4);\
	translation.push_back(x5);translation.push_back(x6);translation.push_back(x7);translation.push_back(x8);

#endif
