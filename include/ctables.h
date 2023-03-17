/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012-2015, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2018, 2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

/** Used to indicate the result of trying to execute a command. */
enum CmdResult {
    /** The command exists but its execution failed. */
    CMD_FAILURE = 0,

    /** The command exists and its execution succeeded. */
    CMD_SUCCESS = 1,

    /* The command does not exist. */
    CMD_INVALID = 2
};

/** Flag for commands that are only allowed from servers */
const char FLAG_SERVERONLY = 7; // technically anything nonzero below 'A' works

/** Translation types for translation of parameters to UIDs.
 * This allows the core commands to not have to be aware of how UIDs
 * work (making it still possible to write other linking modules which
 * do not use UID (but why would you want to?)
 */
enum TranslateType {
    TR_TEXT,        /* Raw text, leave as-is */
    TR_NICK,        /* Nickname, translate to UUID for server->server */
    TR_CUSTOM       /* Custom translation handled by EncodeParameter/DecodeParameter */
};

/** Routing types for a command. Any command which is created defaults
 * to having its command broadcasted on success. This behaviour may be
 * overridden to one of the route types shown below (see the \#defines
 * below for more information on each one's behaviour)
 */
enum RouteType {
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
struct RouteDescriptor {
    /** Routing type from the enum above
     */
    RouteType type;
    /** For unicast, the destination server's name
     */
    std::string serverdest;

    /** For unicast, the destination Server
     */
    Server* server;

    /** Create a RouteDescriptor
     */
    RouteDescriptor(RouteType t, const std::string &d)
        : type(t), serverdest(d), server(NULL) { }

    RouteDescriptor(RouteType t, Server* srv)
        : type(t), server(srv) { }
};

/** Do not route this command */
#define ROUTE_LOCALONLY (RouteDescriptor(ROUTE_TYPE_LOCALONLY, ""))
/** Route this command to all servers, fail if not understood */
#define ROUTE_BROADCAST (RouteDescriptor(ROUTE_TYPE_BROADCAST, ""))
/** Route this command to a single server (do nothing if own server name specified) */
#define ROUTE_UNICAST(x) (RouteDescriptor(ROUTE_TYPE_UNICAST, x))
/** Route this command as a message with the given target (any of user, \#channel, @#channel, $servermask) */
#define ROUTE_MESSAGE(x) (RouteDescriptor(ROUTE_TYPE_MESSAGE, x))
/** Route this command to all servers wrapped via ENCAP, so ignored if not understood */
#define ROUTE_OPT_BCAST (RouteDescriptor(ROUTE_TYPE_OPT_BCAST, ""))
/** Route this command to a single server wrapped via ENCAP, so ignored if not understood */
#define ROUTE_OPT_UCAST(x) (RouteDescriptor(ROUTE_TYPE_OPT_UCAST, x))

/** A structure that defines a command. Every command available
 * in InspIRCd must be defined as derived from Command.
 */
class CoreExport CommandBase : public ServiceProvider {
  public:
    /** Encapsulates parameters to a command. */
    class Params : public std::vector<std::string> {
      private:
        /* IRCv3 message tags. */
        ClientProtocol::TagMap tags;

      public:
        /** Initializes a new instance from parameter and tag references.
         * @param paramsref Message parameters.
         * @param tagsref IRCv3 message tags.
         */
        Params(const std::vector<std::string>& paramsref,
               const ClientProtocol::TagMap& tagsref)
            : std::vector<std::string>(paramsref)
            , tags(tagsref) {
        }

        /** Initializes a new instance from parameter iterators.
         * @param first The first element in the parameter array.
         * @param last The last element in the parameter array.
         */
        template<typename Iterator>
        Params(Iterator first, Iterator last)
            : std::vector<std::string>(first, last) {
        }

        /** Initializes a new empty instance. */
        Params() { }

        /** Retrieves the IRCv3 message tags. */
        const ClientProtocol::TagMap& GetTags() const {
            return tags;
        }
        ClientProtocol::TagMap& GetTags() {
            return tags;
        }
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
     * True by default
     */
    bool allow_empty_last_param;

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
    CommandBase(Module* me, const std::string& cmd, unsigned int minpara = 0,
                unsigned int maxpara = 0);

    virtual RouteDescriptor GetRouting(User* user,
                                       const CommandBase::Params& parameters);

    /** Encode a parameter for server->server transmission.
     * Used for parameters for which the translation type is TR_CUSTOM.
     * @param parameter The parameter to encode. Can be modified in place.
     * @param index The parameter index (0 == first parameter).
     */
    virtual void EncodeParameter(std::string& parameter, unsigned int index);

    virtual ~CommandBase();
};

class CoreExport Command : public CommandBase {
  protected:
    /** Initializes a new instance of the Command class.
     * @param me The module which created this instance.
     * @param cmd The name of the command.
     * @param minpara The minimum number of parameters that the command accepts.
     * @param maxpara The maximum number of parameters that the command accepts.
     */
    Command(Module* me, const std::string& cmd, unsigned int minpara = 0,
            unsigned int maxpara = 0);

  public:
    /** Unregisters this command from the command parser. */
    ~Command() CXX11_OVERRIDE;

    /** The user modes required to be able to execute this command. */
    unsigned char flags_needed;

    /** Whether the command will not be forwarded by the linking module even if it comes via ENCAP. */
    bool force_manual_route;

    /** The number of seconds worth of penalty that executing this command gives. */
    unsigned int Penalty;

    /** The number of times this command has been executed. */
    unsigned long use_count;

    /** If non-empty then the syntax of the parameter for this command. */
    std::string syntax;

    /** Whether the command can be issued before registering. */
    bool works_before_reg;

    /** Handle the command from a user.
     * @param user The user who issued the command.
     * @param parameters The parameters for the command.
     * @return Returns CMD_FAILURE on failure, CMD_SUCCESS on success, or CMD_INVALID
     *         if the command was malformed.
     */
    virtual CmdResult Handle(User* user, const Params& parameters) = 0;

    /** Registers this command with the command parser. */
    void RegisterService() CXX11_OVERRIDE;

    /** Tells the user they did not specify enough parameters.
     * @param user The user who issued the command.
     * @param parameters The parameters for the command.
     */
    virtual void TellNotEnoughParameters(LocalUser* user, const Params& parameters);

    /** Tells the user they need to be registered to execute this command.
     * @param user The user who issued the command.
     * @param parameters The parameters for the command.
     */
    virtual void TellNotRegistered(LocalUser* user, const Params& parameters);
};

class CoreExport SplitCommand : public Command {
  protected:
    /** Initializes a new instance of the SplitCommand class.
     * @param me The module which created this instance.
     * @param cmd The name of the command.
     * @param minpara The minimum number of parameters that the command accepts.
     * @param maxpara The maximum number of parameters that the command accepts.
     */
    SplitCommand(Module* me, const std::string& cmd, unsigned int minpara = 0,
                 unsigned int maxpara = 0);

  public:
    /** @copydoc Command::Handle */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;

    /** Handle the command from a local user.
     * @param user The user who issued the command.
     * @param parameters The parameters for the command.
     * @return Returns CMD_FAILURE on failure, CMD_SUCCESS on success, or CMD_INVALID
     *         if the command was malformed.
     */
    virtual CmdResult HandleLocal(LocalUser* user, const Params& parameters);

    /** Handle the command from a remote user.
     * @param user The user who issued the command.
     * @param parameters The parameters for the command.
     * @return Returns CMD_FAILURE on failure, CMD_SUCCESS on success, or CMD_INVALID
     *         if the command was malformed.
     */
    virtual CmdResult HandleRemote(RemoteUser* user, const Params& parameters);

    /** Handle the command from a server user.
     * @param user The user who issued the command.
     * @param parameters The parameters for the command.
     * @return Returns CMD_FAILURE on failure, CMD_SUCCESS on success, or CMD_INVALID
     *         if the command was malformed.
     */
    virtual CmdResult HandleServer(FakeUser* user, const Params& parameters);
};

/** Shortcut macros for defining translation lists
 */
#define TRANSLATE1(x1)  translation.push_back(x1);
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
