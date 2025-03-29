/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Robin Burchell <robin+git@viroteck.net>
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
#include "listmode.h"
#include "numericbuilder.h"
#include "timeutils.h"
#include "utility/string.h"

enum
{
	RPL_CHECK = 802
};

class CheckContext final
{
private:
	User* const user;
	const std::string& target;

	static std::string FormatTime(time_t ts)
	{
		auto timestr = Time::ToString(ts, Time::DEFAULT_LONG, true);
		timestr += FMT::format(" ({})", ServerInstance->Time());
		return timestr;
	}

public:
	CheckContext(User* u, const std::string& targetstr)
		: user(u)
		, target(targetstr)
	{
		Write("START", target);
	}

	~CheckContext()
	{
		Write("END", target);
	}

	void Write(const std::string& type, const std::string& text)
	{
		user->WriteRemoteNumeric(RPL_CHECK, type, text);
	}

	void Write(const std::string& type, time_t ts)
	{
		user->WriteRemoteNumeric(RPL_CHECK, type, FormatTime(ts));
	}

	User* GetUser() const { return user; }

	void DumpListMode(ListModeBase* mode, Channel* chan)
	{
		const ListModeBase::ModeList* list = mode->GetList(chan);
		if (!list)
			return;

		for (const auto& entry : *list)
		{
			CheckContext::List listmode(*this, "listmode");
			listmode.Add(ConvToStr(mode->GetModeChar()));
			listmode.Add(entry.mask);
			listmode.Add(entry.setter);
			listmode.Add(FormatTime(entry.time));
			listmode.Flush();
		}
	}

	void DumpExt(Extensible* ext)
	{
		CheckContext::List extlist(*this, "metadata");
		for (const auto& [item, obj] : ext->GetExtList())
		{
			const std::string value = item->ToHuman(ext, obj);
			if (!value.empty())
				Write("meta:" + item->name, value);
			else if (!item->name.empty())
				extlist.Add(item->name);
		}

		extlist.Flush();
	}

	class List final
		: public Numeric::GenericBuilder<' ', false, Numeric::WriteRemoteNumericSink>
	{
	public:
		List(CheckContext& context, const char* checktype)
			: Numeric::GenericBuilder<' ', false, Numeric::WriteRemoteNumericSink>(Numeric::WriteRemoteNumericSink(context.GetUser()), RPL_CHECK, false, (IS_LOCAL(context.GetUser()) ? context.GetUser()->nick.length() : ServerInstance->Config->Limits.MaxNick) + strlen(checktype) + 1)
		{
			GetNumeric().push(checktype).push(std::string());
		}
	};
};

class CommandCheck final
	: public Command
{
	UserModeReference snomaskmode;

	std::string GetSnomasks(User* user)
	{
		std::string ret;
		if (snomaskmode)
			ret = snomaskmode->GetUserParameter(user);

		if (ret.empty())
			ret = "+";
		return ret;
	}

public:
	CommandCheck(Module* parent)
		: Command(parent, "CHECK", 1)
		, snomaskmode(parent, "snomask")
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = {
			"<nick>|<channel> [<servername>]",
			"<nickmask>|<usermask>|<hostmask>|<ipmask>|<realnamemask> [<servername>]",
		};
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (parameters.size() > 1 && !irc::equals(parameters[1], ServerInstance->Config->ServerName))
			return CmdResult::SUCCESS;

		auto* targetuser = ServerInstance->Users.FindNick(parameters[0]);
		auto* targetchan = ServerInstance->Channels.Find(parameters[0]);

		/*
		 * Syntax of a /check reply:
		 *  :server.name 802 target START <target>
		 *  :server.name 802 target <field> :<value>
		 *  :server.name 802 target END <target>
		 */

		// Constructor sends START, destructor sends END
		CheckContext context(user, parameters[0]);

		if (targetuser)
		{
			LocalUser* localtarget = IS_LOCAL(targetuser);
			/* /check on a user */
			context.Write("nuh", targetuser->GetMask());
			context.Write("realnuh", targetuser->GetRealMask());
			context.Write("realname", targetuser->GetRealName());
			context.Write("modes", targetuser->GetModeLetters());
			context.Write("snomasks", GetSnomasks(targetuser));
			context.Write("server", targetuser->server->GetName());
			context.Write("uid", targetuser->uuid);
			context.Write("signon", targetuser->signon);
			context.Write("nickchanged", targetuser->nickchanged);
			if (localtarget)
				context.Write("lastmsg", localtarget->idle_lastmsg);

			if (targetuser->IsAway())
			{
				/* user is away */
				context.Write("awaytime", targetuser->away->time);
				context.Write("awaymsg", targetuser->away->message);
			}

			if (targetuser->IsOper())
			{
				context.Write("oper", targetuser->oper->GetName());
				context.Write("opertype", targetuser->oper->GetType());
				if (localtarget)
				{
					context.Write("chanmodeperms", localtarget->oper->GetModes(MODETYPE_CHANNEL));
					context.Write("usermodeperms", localtarget->oper->GetModes(MODETYPE_USER));
					context.Write("snomaskperms", localtarget->oper->GetSnomasks());
					context.Write("commandperms", localtarget->oper->GetCommands());
					context.Write("permissions", localtarget->oper->GetPrivileges());
				}
			}

			if (localtarget)
			{
				context.Write("clientaddr", localtarget->client_sa.str());
				context.Write("serveraddr", localtarget->server_sa.str());

				std::string classname = localtarget->GetClass()->name;
				if (!classname.empty())
					context.Write("connectclass", classname);

				context.Write("exempt", localtarget->exempt ? "yes" : "no");
				context.Write("serializer", localtarget->serializer->name.substr(11));
			}
			else
				context.Write("onip", targetuser->GetAddress());

			CheckContext::List chanlist(context, "onchans");
			for (const auto* memb : targetuser->chans)
				chanlist.Add(memb->GetAllPrefixChars() + memb->chan->name);
			chanlist.Flush();

			context.DumpExt(targetuser);
		}
		else if (targetchan)
		{
			/* /check on a channel */
			context.Write("createdat", targetchan->age);

			if (!targetchan->topic.empty())
			{
				/* there is a topic, assume topic related information exists */
				context.Write("topic", targetchan->topic);
				context.Write("topic_setby", targetchan->setby);
				context.Write("topic_setat", targetchan->topicset);
			}

			context.Write("modes", targetchan->ChanModes(true));
			context.Write("membercount", ConvToStr(targetchan->GetUsers().size()));

			for (const auto& [u, memb] : targetchan->GetUsers())
			{
				/*
				 * Unlike Asuka, I define a clone as coming from the same host. --w00t
				 */
				const UserManager::CloneCounts& clonecount = ServerInstance->Users.GetCloneCounts(u);
				context.Write("member", FMT::format("{} {}{} ({}\x0F)", clonecount.global, memb->GetAllPrefixChars(),
					u->GetMask(), u->GetRealName()));
			}

			for (auto* lm : ServerInstance->Modes.GetListModes())
				context.DumpListMode(lm, targetchan);

			context.DumpExt(targetchan);
		}
		else
		{
			/*  /check on an IP address, or something that doesn't exist */
			size_t x = 0;

			std::vector<const char*> matches;
			matches.reserve(6);

			/* hostname or other */
			for (const auto& [_, u] : ServerInstance->Users.GetUsers())
			{
				if (InspIRCd::Match(u->nick, parameters[0], ascii_case_insensitive_map))
					matches.push_back("nick");

				if (InspIRCd::Match(u->GetRealUser(), parameters[0], ascii_case_insensitive_map))
					matches.push_back("ruser");

				if (InspIRCd::Match(u->GetDisplayedUser(), parameters[0], ascii_case_insensitive_map))
					matches.push_back("duser");

				if (InspIRCd::Match(u->GetRealHost(), parameters[0], ascii_case_insensitive_map))
					matches.push_back("rhost");

				if (InspIRCd::Match(u->GetDisplayedHost(), parameters[0], ascii_case_insensitive_map))
					matches.push_back("dhost");

				if (InspIRCd::MatchCIDR(u->GetAddress(), parameters[0]))
					matches.push_back("ipaddr");

				if (InspIRCd::MatchCIDR(u->GetRealName(), parameters[0]))
					matches.push_back("realname");

				if (!matches.empty())
				{
					const std::string whatmatch = insp::join(matches, ',');
					context.Write("match", FMT::format("{} {} {} {} {} {} {} {} :{}", ++x, whatmatch, u->nick, u->GetRealUser(),
						u->GetDisplayedUser(), u->GetRealHost(), u->GetDisplayedHost(), u->GetAddress(), u->GetRealName()));
					matches.clear();
				}
			}

			context.Write("matches", ConvToStr(x));
		}

		// END is sent by the CheckContext destructor
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		if ((parameters.size() > 1) && (parameters[1].find('.') != std::string::npos))
			return ROUTE_OPT_UCAST(parameters[1]);
		return ROUTE_LOCALONLY;
	}
};

class ModuleCheck final
	: public Module
{
private:
	CommandCheck cmd;

public:
	ModuleCheck()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /CHECK command which allows server operators to look up details about a channel, user, IP address, or hostname.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleCheck)
