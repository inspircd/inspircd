/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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

#include "inspircd.h"
#include "clientprotocolmsg.h"
#include "numerichelper.h"
#include "utility/string.h"

struct SharedData final
{
	// A reference to the +k user mode from the servicdes.
	UserModeReference servprotectmode;

	// Whether to sign messages with the executing operator's nick.
	bool sign = true;

	SharedData(Module *mod)
		: servprotectmode(mod, "protect")
	{
	}
};

class CommandSAJoin final
	: public Command
{
private:
	SharedData& data;

public:
	CommandSAJoin(Module* mod, SharedData& sd)
		: Command(mod, "SAJOIN", 1)
		, data(sd)
	{
		accepts_multiple_targets = true;
		access_needed = CmdAccess::OPERATOR;
		syntax = { "[<nick>] <channel>[,<channel>]+" };
		translation = { TranslateType::NICK, TranslateType::TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		const unsigned int channelindex = (parameters.size() > 1) ? 1 : 0;
		if (CommandParser::LoopCall(user, this, parameters, channelindex))
			return CmdResult::FAILURE;

		User* target = user;
		if (parameters.size() > 1)
		{
			const auto& targetnick = parameters[0];
			target = ServerInstance->Users.Find(targetnick, true);
			if (!target)
			{
				user->WriteRemoteNumeric(Numerics::NoSuchNick(targetnick));
				return CmdResult::FAILURE;
			}
		}

		if (user != target)
		{
			// Force joining another user requires a special permission.
			if (!user->HasPrivPermission("users/sajoin-others"))
			{
				user->WriteRemoteNumeric(Numerics::NoPrivileges("your server operator account does not have the users/sajoin-others privilege"));
				return CmdResult::FAILURE;
			}
			if (target->IsModeSet(data.servprotectmode))
			{
				user->WriteRemoteNumeric(Numerics::NoPrivileges("you can not use the {} command on a protected service", this->service_name));
				return CmdResult::FAILURE;
			}
		}

		const auto& targetchan = parameters[channelindex];
		if (user->IsLocal() && !ServerInstance->Channels.IsChannel(targetchan))
		{
			user->WriteRemoteNumeric(ERR_BADCHANMASK, targetchan, "Invalid channel name");
			return CmdResult::FAILURE;
		}

		auto* chan = ServerInstance->Channels.Find(targetchan);
		if (chan && chan->HasUser(target))
		{
			user->WriteRemoteNumeric(ERR_USERONCHANNEL, target->nick, chan->name, "is already on channel");
			return CmdResult::FAILURE;
		}

		auto* ltarget = target->AsLocal();
		if (!ltarget)
			return CmdResult::SUCCESS; // User will be handled by their own server.

		auto* memb = Channel::JoinUser(ltarget, targetchan, true);
		if (!memb)
		{
			user->WriteRemoteNotice("*** {}: could not join {} to {}.", this->service_name, target->nick, targetchan);
			return CmdResult::FAILURE;
		}

		ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to make {} join {}", user->nick,
			user->oper->GetName(), this->service_name, target->nick, memb->chan->name);

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class CommandSAKick final
	: public Command
{
private:
	SharedData& data;

public:
	CommandSAKick(Module* mod, SharedData& sd)
		: Command(mod, "SAKICK", 2, 3)
		, data(sd)
	{
		access_needed = CmdAccess::OPERATOR;
		allow_empty_last_param = true;
		syntax = { "<channel> <nick> [:<reason>]" };
		translation = { TranslateType::TEXT, TranslateType::NICK, TranslateType::TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		const auto& targetnick = parameters[1];
		auto* target = ServerInstance->Users.Find(targetnick, true);
		if (!target)
		{
			user->WriteRemoteNumeric(Numerics::NoSuchNick(targetnick));
			return CmdResult::FAILURE;
		}
		if (target->IsModeSet(data.servprotectmode))
		{
			user->WriteRemoteNumeric(Numerics::NoPrivileges("you can not use the {} command on a protected service", this->service_name));
			return CmdResult::FAILURE;
		}

		const auto& targetchan = parameters[1];
		auto* chan = ServerInstance->Channels.Find(targetchan);
		if (!chan)
		{
			user->WriteRemoteNumeric(Numerics::NoSuchChannel(targetchan));
			return CmdResult::FAILURE;
		}
		if (!chan->HasUser(target))
		{
			user->WriteRemoteNumeric(ERR_USERNOTINCHANNEL, target->nick, chan->name, "They are not on that channel");
			return CmdResult::FAILURE;
		}

		if (!target->IsLocal())
			return CmdResult::SUCCESS; // User will be handled by their own server.

		const std::string reason(parameters.size() > 2 ? parameters.back() : target->nick, 0, ServerInstance->Config->Limits.MaxKick);
		chan->KickUser(user, target, data.sign ? FMT::format("{} ({})", reason, user->nick) : reason);

		ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to kick {} from {} ({})", user->nick,
			user->oper->GetName(), this->service_name, target->nick, chan->name, reason);

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[1]);
	}
};

class CommandSAMode final
	: public Command
{
private:
	SharedData& data;
	bool logged = false;

public:
	bool active = false;

	CommandSAMode(Module* mod, SharedData& sd)
		: Command(mod, "SAMODE", 2)
		, data(sd)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<target> (+|-)<modes> [<mode-parameters>]" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		const auto& targetstr = parameters[0];
		if (!ServerInstance->Channels.IsPrefix(targetstr[0]))
		{
			auto* target = ServerInstance->Users.FindNick(targetstr, true);
			if (!target)
			{
				user->WriteNumeric(Numerics::NoSuchNick(targetstr));
				return CmdResult::FAILURE;
			}
			if (target->IsModeSet(data.servprotectmode))
			{
				user->WriteRemoteNumeric(Numerics::NoPrivileges("you can not use the {} command on a protected service", this->service_name));
				return CmdResult::FAILURE;
			}

			// Changing the modes of another user requires a special permission.
			if (target != user && !user->HasPrivPermission("users/samode-usermodes"))
			{
				user->WriteRemoteNumeric(Numerics::NoPrivileges("your server operator account does not have the users/samode-usermodes privilege"));
				return CmdResult::FAILURE;
			}
		}

		// XXX: Make ModeParser clear LastParse
		Modes::ChangeList emptychangelist;
		ServerInstance->Modes.ProcessSingle(ServerInstance->FakeClient, nullptr, ServerInstance->FakeClient, emptychangelist);

		this->logged = false;

		this->active = true;
		ServerInstance->Parser.CallHandler("MODE", parameters, user);
		this->active = false;

		if (!this->logged)
		{
			// If we haven't logged anything yet then the client queried the list of a listmode
			// (e.g. /SAMODE #chan b), which was handled internally by the MODE command handler.
			//
			// Viewing the modes of a user or a channel could also result in this, but that is not
			// possible with /SAMODE because we require at least 2 parameters.
			this->LogUsage(user, targetstr, insp::join(insp::iterator_range(parameters.begin() + 1, parameters.end())));
		}

		return CmdResult::SUCCESS;
	}

	void LogUsage(const User* user, const std::string& target, const std::string& modes)
	{
		this->logged = true;
		ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to change the modes of {} to {}", user->nick,
			user->oper->GetName(), this->service_name, target, modes);
	}
};

class CommandSANick final
	: public Command
{
private:
	SharedData& data;

public:
	CommandSANick(Module* mod, SharedData& sd)
		: Command(mod, "SANICK", 2)
		, data(sd)
	{
		access_needed = CmdAccess::OPERATOR;
		allow_empty_last_param = true;
		syntax = { "<nick> <newnick>" };
		translation = { TranslateType::NICK, TranslateType::TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		const auto& targetnick = parameters[0];
		auto* target = ServerInstance->Users.Find(targetnick, true);
		if (!target)
		{
			user->WriteRemoteNumeric(Numerics::NoSuchNick(targetnick));
			return CmdResult::FAILURE;
		}
		if (target->IsModeSet(data.servprotectmode))
		{
			user->WriteRemoteNumeric(Numerics::NoPrivileges("you can not use the {} command on a protected service", this->service_name));
			return CmdResult::FAILURE;
		}

		auto targetnewnick = parameters[1];
		if (targetnewnick.empty())
		{
			user->WriteRemoteNumeric(ERR_NONICKNAMEGIVEN, "No nickname given");
			return CmdResult::FAILURE;
		}

		if (targetnewnick == "0")
			targetnewnick = target->uuid;
		else if (!ServerInstance->Users.IsNick(targetnewnick))
		{
			user->WriteRemoteNumeric(ERR_ERRONEUSNICKNAME, targetnewnick, "Erroneous nickname");
			return CmdResult::FAILURE;
		}

		if (ServerInstance->Users.FindNick(targetnewnick))
		{
			user->WriteRemoteNumeric(ERR_NICKNAMEINUSE, targetnewnick, "Nickname is already in use.");
			return CmdResult::FAILURE;
		}

		if (!target->IsLocal())
			return CmdResult::SUCCESS; // User will be handled by their own server.

		if (!target->ChangeNick(targetnewnick))
		{
			user->WriteRemoteNotice("*** {}: could not change the nick of {} to {}.", this->service_name,
				targetnick, targetnewnick);
			return CmdResult::FAILURE;
		}

		ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to change the nick of {} to {}", user->nick,
			user->oper->GetName(), this->service_name, targetnick, targetnewnick);

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class CommandSAPart final
	: public Command
{
private:
	SharedData& data;

public:
	CommandSAPart(Module* mod, SharedData& sd)
		: Command(mod, "SAPART", 2, 3)
		, data(sd)
	{
		accepts_multiple_targets = true;
		access_needed = CmdAccess::OPERATOR;
		allow_empty_last_param = true;
		syntax = { "<nick> <channel>[,<channel>]+ [:<reason>]" };
		translation = { TranslateType::NICK, TranslateType::TEXT, TranslateType::TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (CommandParser::LoopCall(user, this, parameters, 1))
			return CmdResult::FAILURE;

		const auto& targetnick = parameters[0];
		auto* target = ServerInstance->Users.Find(targetnick, true);
		if (!target)
		{
			user->WriteRemoteNumeric(Numerics::NoSuchNick(targetnick));
			return CmdResult::FAILURE;
		}
		if (target->IsModeSet(data.servprotectmode))
		{
			user->WriteRemoteNumeric(Numerics::NoPrivileges("you can not use the {} command on a protected service", this->service_name));
			return CmdResult::FAILURE;
		}

		const auto& targetchan = parameters[0];
		auto* chan = ServerInstance->Channels.Find(targetchan);
		if (!chan)
		{
			user->WriteRemoteNumeric(Numerics::NoSuchChannel(targetchan));
			return CmdResult::FAILURE;
		}

		if (!target->IsLocal())
			return CmdResult::SUCCESS; // User will be handled by their own server.

		const auto& reason = parameters.size() > 2 ? parameters.back() : "";
		if (!chan->PartUser(target, data.sign ? FMT::format("{}{}({})", reason, reason.empty() ? "" : " ", user->nick) : reason))
		{
			user->WriteRemoteNumeric(ERR_USERNOTINCHANNEL, target->nick, chan->name, "They are not on that channel");
			return CmdResult::FAILURE;
		}

		ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to part {} from {} ({})", user->nick,
			user->oper->GetName(), this->service_name, target->nick, chan->name, reason);

		return CmdResult::FAILURE;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class CommandSAQuit final
	: public Command
{
private:
	SharedData& data;

public:
	CommandSAQuit(Module* mod, SharedData& sd)
		: Command(mod, "SAQUIT", 2, 2)
		, data(sd)
	{
		access_needed = CmdAccess::OPERATOR;
		allow_empty_last_param = true;
		syntax = { "<nick> :<reason>" };
		translation = { TranslateType::NICK, TranslateType::TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		const auto& targetnick = parameters[0];
		auto* target = ServerInstance->Users.Find(targetnick, true);
		if (!target)
		{
			user->WriteRemoteNumeric(Numerics::NoSuchNick(targetnick));
			return CmdResult::FAILURE;
		}
		if (target->IsModeSet(data.servprotectmode))
		{
			user->WriteRemoteNumeric(Numerics::NoPrivileges("you can not use the {} command on a protected service", this->service_name));
			return CmdResult::FAILURE;
		}

		if (!target->IsLocal())
			return CmdResult::SUCCESS; // User will be handled by their own server.

		const auto& reason = parameters.size() > 2 ? parameters.back() : "";
		ServerInstance->Users.QuitUser(target, data.sign ? FMT::format("{}{}({})", reason, reason.empty() ? "" : " ", user->nick) : reason);

		ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to quit {} ({})", user->nick,
			user->oper->GetName(), this->service_name, target->nick, reason);

		return CmdResult::FAILURE;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};


class CommandSATopic final
	: public Command
{
public:
	CommandSATopic(Module* mod)
		: Command(mod, "SATOPIC", 2, 2)
	{
		access_needed = CmdAccess::OPERATOR;
		allow_empty_last_param = true;
		syntax = { "<channel> :<topic>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		auto* target = ServerInstance->Channels.Find(parameters[0]);
		if (!target)
		{
			user->WriteRemoteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CmdResult::FAILURE;
		}

		const std::string topic(parameters[1], 0, ServerInstance->Config->Limits.MaxTopic);
		if (target->topic == topic)
		{
			user->WriteRemoteNotice("*** {}: The topic on {} is already what you are trying to change it to.",
				this->service_name, target->name);
			return CmdResult::SUCCESS;
		}

		target->SetTopic(user, topic, ServerInstance->Time(), nullptr);

		ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to change the topic of {} ({})", user->nick,
			user->oper->GetName(), this->service_name, target->name, topic);

		return CmdResult::SUCCESS;
	}
};

class ModuleSACommands final
	: public Module
{
private:
	SharedData data;
	CommandSAJoin cmdsajoin;
	CommandSAKick cmdsakick;
	CommandSAMode cmdsamode;
	CommandSANick cmdsanick;
	CommandSAPart cmdsapart;
	CommandSAQuit cmdsaquit;
	CommandSATopic cmdsatopic;

public:
	ModuleSACommands()
		: Module(VF_VENDOR, "Adds various server operator-only versions of regular commands.")
		, data(this)
		, cmdsajoin(this, data)
		, cmdsakick(this, data)
		, cmdsamode(this, data)
		, cmdsanick(this, data)
		, cmdsapart(this, data)
		, cmdsaquit(this, data)
		, cmdsatopic(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("sacommands");
		data.sign = tag->getBool("sign", true);
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnRawMode, PRIORITY_BEFORE, "disable");
		ServerInstance->Modules.SetPriority(this, I_OnPreMode, PRIORITY_BEFORE, "override");
	}

	ModResult OnPreMode(User* source, User* dest, Channel* channel, Modes::ChangeList& modes) override
	{
		if (cmdsamode.active)
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

	void OnMode(User* user, User* destuser, Channel* destchan, const Modes::ChangeList& modes, ModeParser::ModeProcessFlag processflags) override
	{
		if (!cmdsamode.active)
			return;


		auto modestr = ClientProtocol::Messages::Mode::ToModeLetters(modes);
		for (const auto& item : modes.getlist())
		{
			if (!item.param.empty())
				modestr.append(1, ' ').append(item.param);
		}

		cmdsamode.LogUsage(user, destuser ? destuser->nick : destchan->name, modestr);
	}
};

MODULE_INIT(ModuleSACommands)
