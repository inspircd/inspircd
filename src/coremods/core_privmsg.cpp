/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

class MessageDetailsImpl : public MessageDetails
{
public:
	MessageDetailsImpl(MessageType mt, const std::string& msg, const ClientProtocol::TagMap& tags)
		: MessageDetails(mt, msg, tags)
	{
	}

	bool IsCTCP(std::string& name, std::string& body) const CXX11_OVERRIDE
	{
		if (!this->IsCTCP())
			return false;

		size_t end_of_name = text.find(' ', 2);
		size_t end_of_ctcp = *text.rbegin() == '\x1' ? 1 : 0;
		if (end_of_name == std::string::npos)
		{
			// The CTCP only contains a name.
			name.assign(text, 1, text.length() - 1 - end_of_ctcp);
			body.clear();
			return true;
		}

		// The CTCP contains a name and a body.
		name.assign(text, 1, end_of_name - 1);

		size_t start_of_body = text.find_first_not_of(' ', end_of_name + 1);
		if (start_of_body == std::string::npos)
		{
			// The CTCP body is provided but empty.
			body.clear();
			return true;
		}

		// The CTCP body provided was non-empty.
		body.assign(text, start_of_body, text.length() - start_of_body - end_of_ctcp);
		return true;
	}

	bool IsCTCP(std::string& name) const CXX11_OVERRIDE
	{
		if (!this->IsCTCP())
			return false;

		size_t end_of_name = text.find(' ', 2);
		if (end_of_name == std::string::npos)
		{
			// The CTCP only contains a name.
			size_t end_of_ctcp = *text.rbegin() == '\x1' ? 1 : 0;
			name.assign(text, 1, text.length() - 1 - end_of_ctcp);
			return true;
		}

		// The CTCP contains a name and a body.
		name.assign(text, 1, end_of_name - 1);
		return true;
	}

	bool IsCTCP() const CXX11_OVERRIDE
	{
		// According to draft-oakley-irc-ctcp-02 a valid CTCP must begin with SOH and
		// contain at least one octet which is not NUL, SOH, CR, LF, or SPACE. As most
		// of these are restricted at the protocol level we only need to check for SOH
		// and SPACE.
		return (text.length() >= 2) && (text[0] == '\x1') &&  (text[1] != '\x1') && (text[1] != ' ');
	}
};

class MessageCommandBase : public Command
{
	ChanModeReference moderatedmode;
	ChanModeReference noextmsgmode;

	/** Send a PRIVMSG or NOTICE message to all local users from the given user
	 * @param user User sending the message
	 * @param msg The message to send
	 * @param mt Type of the message (MSG_PRIVMSG or MSG_NOTICE)
	 * @param tags Message tags to include in the outgoing protocol message
	 */
	static void SendAll(User* user, const std::string& msg, MessageType mt, const ClientProtocol::TagMap& tags);

 public:
	MessageCommandBase(Module* parent, MessageType mt)
		: Command(parent, ClientProtocol::Messages::Privmsg::CommandStrFromMsgType(mt), 2, 2)
		, moderatedmode(parent, "moderated")
		, noextmsgmode(parent, "noextmsg")
	{
		syntax = "<target>{,<target>} <message>";
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleMessage(User* user, const Params& parameters, MessageType mt);

	RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (IS_LOCAL(user))
			// This is handled by the OnUserPostMessage hook to split the LoopCall pieces
			return ROUTE_LOCALONLY;
		else
			return ROUTE_MESSAGE(parameters[0]);
	}
};

void MessageCommandBase::SendAll(User* user, const std::string& msg, MessageType mt, const ClientProtocol::TagMap& tags)
{
	ClientProtocol::Messages::Privmsg message(ClientProtocol::Messages::Privmsg::nocopy, user, "$*", msg, mt);
	message.AddTags(tags);
	message.SetSideEffect(true);
	ClientProtocol::Event messageevent(ServerInstance->GetRFCEvents().privmsg, message);

	const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
	for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		if ((*i)->registered == REG_ALL)
			(*i)->Send(messageevent);
	}
}

CmdResult MessageCommandBase::HandleMessage(User* user, const Params& parameters, MessageType mt)
{
	User *dest;
	Channel *chan;

	LocalUser* localuser = IS_LOCAL(user);
	if (localuser)
		localuser->idle_lastmsg = ServerInstance->Time();

	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	if (parameters[0][0] == '$')
	{
		if (!user->HasPrivPermission("users/mass-message"))
			return CMD_SUCCESS;

		std::string servername(parameters[0], 1);
		MessageTarget msgtarget(&servername);
		MessageDetailsImpl msgdetails(mt, parameters[1], parameters.GetTags());

		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnUserPreMessage, MOD_RESULT, (user, msgtarget, msgdetails));
		if (MOD_RESULT == MOD_RES_DENY)
		{
			FOREACH_MOD(OnUserMessageBlocked, (user, msgtarget, msgdetails));
			return CMD_FAILURE;
		}

		FOREACH_MOD(OnUserMessage, (user, msgtarget, msgdetails));
		if (InspIRCd::Match(ServerInstance->Config->ServerName, servername, NULL))
		{
			SendAll(user, msgdetails.text, mt, msgdetails.tags_out);
		}
		FOREACH_MOD(OnUserPostMessage, (user, msgtarget, msgdetails));
		return CMD_SUCCESS;
	}

	char status = 0;
	const char* target = parameters[0].c_str();

	if (ServerInstance->Modes->FindPrefix(*target))
	{
		status = *target;
		target++;
	}
	if (*target == '#')
	{
		chan = ServerInstance->FindChan(target);

		if (chan)
		{
			if (localuser && chan->GetPrefixValue(user) < VOICE_VALUE)
			{
				if (chan->IsModeSet(noextmsgmode) && !chan->HasUser(user))
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (no external messages)");
					return CMD_FAILURE;
				}

				if (chan->IsModeSet(moderatedmode))
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (+m)");
					return CMD_FAILURE;
				}

				if (ServerInstance->Config->RestrictBannedUsers != ServerConfig::BUT_NORMAL)
				{
					if (chan->IsBanned(user))
					{
						if (ServerInstance->Config->RestrictBannedUsers == ServerConfig::BUT_RESTRICT_NOTIFY)
							user->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (you're banned)");
						return CMD_FAILURE;
					}
				}
			}

			MessageTarget msgtarget(chan, status);
			MessageDetailsImpl msgdetails(mt, parameters[1], parameters.GetTags());
			msgdetails.exemptions.insert(user);

			ModResult MOD_RESULT;
			FIRST_MOD_RESULT(OnUserPreMessage, MOD_RESULT, (user, msgtarget, msgdetails));
			if (MOD_RESULT == MOD_RES_DENY)
			{
				FOREACH_MOD(OnUserMessageBlocked, (user, msgtarget, msgdetails));
				return CMD_FAILURE;
			}

			/* Check again, a module may have zapped the input string */
			if (msgdetails.text.empty())
			{
				user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
				return CMD_FAILURE;
			}

			FOREACH_MOD(OnUserMessage, (user, msgtarget, msgdetails));

			ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, user, chan, msgdetails.text, msgdetails.type, msgtarget.status);
			privmsg.AddTags(msgdetails.tags_out);
			privmsg.SetSideEffect(true);
			chan->Write(ServerInstance->GetRFCEvents().privmsg, privmsg, msgtarget.status, msgdetails.exemptions);

			FOREACH_MOD(OnUserPostMessage, (user, msgtarget, msgdetails));
		}
		else
		{
			/* channel does not exist */
			user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CMD_FAILURE;
		}
		return CMD_SUCCESS;
	}

	const char* destnick = parameters[0].c_str();

	if (localuser)
	{
		const char* targetserver = strchr(destnick, '@');

		if (targetserver)
		{
			std::string nickonly;

			nickonly.assign(destnick, 0, targetserver - destnick);
			dest = ServerInstance->FindNickOnly(nickonly);
			if (dest && strcasecmp(dest->server->GetName().c_str(), targetserver + 1))
			{
				/* Incorrect server for user */
				user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
				return CMD_FAILURE;
			}
		}
		else
			dest = ServerInstance->FindNickOnly(destnick);
	}
	else
		dest = ServerInstance->FindNick(destnick);

	if ((dest) && (dest->registered == REG_ALL))
	{
		if (parameters[1].empty())
		{
			user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
			return CMD_FAILURE;
		}

		if ((dest->IsAway()) && (mt == MSG_PRIVMSG))
		{
			/* auto respond with aweh msg */
			user->WriteNumeric(RPL_AWAY, dest->nick, dest->awaymsg);
		}

		MessageTarget msgtarget(dest);
		MessageDetailsImpl msgdetails(mt, parameters[1], parameters.GetTags());


		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnUserPreMessage, MOD_RESULT, (user, msgtarget, msgdetails));
		if (MOD_RESULT == MOD_RES_DENY)
		{
			FOREACH_MOD(OnUserMessageBlocked, (user, msgtarget, msgdetails));
			return CMD_FAILURE;
		}

		FOREACH_MOD(OnUserMessage, (user, msgtarget, msgdetails));

		LocalUser* const localtarget = IS_LOCAL(dest);
		if (localtarget)
		{
			// direct write, same server
			ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, user, localtarget->nick, msgdetails.text, mt);
			privmsg.AddTags(msgdetails.tags_out);
			privmsg.SetSideEffect(true);
			localtarget->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
		}

		FOREACH_MOD(OnUserPostMessage, (user, msgtarget, msgdetails));
	}
	else
	{
		/* no such nick/channel */
		user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

template<MessageType MT>
class CommandMessage : public MessageCommandBase
{
 public:
	CommandMessage(Module* parent)
		: MessageCommandBase(parent, MT)
	{
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		return HandleMessage(user, parameters, MT);
	}
};

class ModuleCoreMessage : public Module
{
	CommandMessage<MSG_PRIVMSG> CommandPrivmsg;
	CommandMessage<MSG_NOTICE> CommandNotice;

 public:
	ModuleCoreMessage()
		: CommandPrivmsg(this), CommandNotice(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("PRIVMSG, NOTICE", VF_CORE|VF_VENDOR);
	}
};

MODULE_INIT(ModuleCoreMessage)
