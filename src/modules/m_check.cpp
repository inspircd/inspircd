/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
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

enum
{
	RPL_CHECK = 802
};

class CheckContext
{
 private:
	User* const user;
	const std::string& target;

	std::string FormatTime(time_t ts)
	{
		std::string timestr(InspIRCd::TimeString(ts, "%Y-%m-%d %H:%M:%S UTC (", true));
		timestr.append(ConvToStr(ts));
		timestr.push_back(')');
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

	class List : public Numeric::GenericBuilder<' ', false, Numeric::WriteRemoteNumericSink>
	{
	 public:
		List(CheckContext& context, const char* checktype)
			: Numeric::GenericBuilder<' ', false, Numeric::WriteRemoteNumericSink>(Numeric::WriteRemoteNumericSink(context.GetUser()), RPL_CHECK, false, (IS_LOCAL(context.GetUser()) ? context.GetUser()->nick.length() : ServerInstance->Config->Limits.MaxNick) + strlen(checktype) + 1)
		{
			GetNumeric().push(checktype).push(std::string());
		}
	};
};

class CommandCheck : public Command
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

	static std::string GetAllowedOperOnlyModes(LocalUser* user, ModeType modetype)
	{
		std::string ret;
		for (const auto& [_, mh] : ServerInstance->Modes.GetModes(modetype))
		{
			if ((mh->NeedsOper()) && (user->HasModePermission(mh)))
				ret.push_back(mh->GetModeChar());
		}
		return ret;
	}

	static std::string GetAllowedOperOnlySnomasks(LocalUser* user)
	{
		std::string ret;
		for (unsigned char sno = 'A'; sno <= 'z'; ++sno)
			if (ServerInstance->SNO.IsSnomaskUsable(sno) && user->HasSnomaskPermission(sno))
				ret.push_back(sno);
		return ret;
	}

 public:
	CommandCheck(Module* parent)
		: Command(parent,"CHECK", 1)
		, snomaskmode(parent, "snomask")
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<nick>|<ipmask>|<hostmask>|<channel> [<servername>]" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (parameters.size() > 1 && !irc::equals(parameters[1], ServerInstance->Config->ServerName))
			return CmdResult::SUCCESS;

		User *targuser;
		Channel *targchan;

		targuser = ServerInstance->Users.Find(parameters[0]);
		targchan = ServerInstance->Channels.Find(parameters[0]);

		/*
		 * Syntax of a /check reply:
		 *  :server.name 802 target START <target>
		 *  :server.name 802 target <field> :<value>
		 *  :server.name 802 target END <target>
		 */

		// Constructor sends START, destructor sends END
		CheckContext context(user, parameters[0]);

		if (targuser)
		{
			LocalUser* loctarg = IS_LOCAL(targuser);
			/* /check on a user */
			context.Write("nuh", targuser->GetFullHost());
			context.Write("realnuh", targuser->GetFullRealHost());
			context.Write("realname", targuser->GetRealName());
			context.Write("modes", targuser->GetModeLetters());
			context.Write("snomasks", GetSnomasks(targuser));
			context.Write("server", targuser->server->GetName());
			context.Write("uid", targuser->uuid);
			context.Write("signon", targuser->signon);
			context.Write("nickts", targuser->age);
			if (loctarg)
				context.Write("lastmsg", loctarg->idle_lastmsg);

			if (targuser->IsAway())
			{
				/* user is away */
				context.Write("awaytime", targuser->awaytime);
				context.Write("awaymsg", targuser->awaymsg);
			}

			if (targuser->IsOper())
			{
				/* user is an oper of type ____ */
				context.Write("opertype", targuser->oper->name);
				if (loctarg)
				{
					context.Write("chanmodeperms", GetAllowedOperOnlyModes(loctarg, MODETYPE_CHANNEL));
					context.Write("usermodeperms", GetAllowedOperOnlyModes(loctarg, MODETYPE_USER));
					context.Write("snomaskperms", GetAllowedOperOnlySnomasks(loctarg));
					context.Write("commandperms", targuser->oper->AllowedOperCommands.ToString());
					context.Write("permissions", targuser->oper->AllowedPrivs.ToString());
				}
			}

			if (loctarg)
			{
				context.Write("clientaddr", loctarg->client_sa.str());
				context.Write("serveraddr", loctarg->server_sa.str());

				std::string classname = loctarg->GetClass()->name;
				if (!classname.empty())
					context.Write("connectclass", classname);

				context.Write("exempt", loctarg->exempt ? "yes" : "no");
			}
			else
				context.Write("onip", targuser->GetIPString());

			CheckContext::List chanlist(context, "onchans");
			for (const auto* memb : targuser->chans)
				chanlist.Add(memb->GetAllPrefixChars() + memb->chan->name);
			chanlist.Flush();

			context.DumpExt(targuser);
		}
		else if (targchan)
		{
			/* /check on a channel */
			context.Write("createdat", targchan->age);

			if (!targchan->topic.empty())
			{
				/* there is a topic, assume topic related information exists */
				context.Write("topic", targchan->topic);
				context.Write("topic_setby", targchan->setby);
				context.Write("topic_setat", targchan->topicset);
			}

			context.Write("modes", targchan->ChanModes(true));
			context.Write("membercount", ConvToStr(targchan->GetUserCounter()));

			for (const auto& [u, memb] : targchan->GetUsers())
			{
				/*
				 * Unlike Asuka, I define a clone as coming from the same host. --w00t
				 */
				const UserManager::CloneCounts& clonecount = ServerInstance->Users.GetCloneCounts(u);
				context.Write("member", InspIRCd::Format("%u %s%s (%s)", clonecount.global,
					memb->GetAllPrefixChars().c_str(), u->GetFullHost().c_str(),
					u->GetRealName().c_str()));
			}

			for (const auto& lm : ServerInstance->Modes.GetListModes())
				context.DumpListMode(lm, targchan);

			context.DumpExt(targchan);
		}
		else
		{
			/*  /check on an IP address, or something that doesn't exist */
			size_t x = 0;

			/* hostname or other */
			for (const auto& [_, u] : ServerInstance->Users.GetUsers())
			{
				if (InspIRCd::Match(u->GetRealHost(), parameters[0], ascii_case_insensitive_map) || InspIRCd::Match(u->GetDisplayedHost(), parameters[0], ascii_case_insensitive_map))
				{
					/* host or vhost matches mask */
					context.Write("match", ConvToStr(++x) + " " + u->GetFullRealHost() + " " + u->GetIPString() + " " + u->GetRealName());
				}
				/* IP address */
				else if (InspIRCd::MatchCIDR(u->GetIPString(), parameters[0]))
				{
					/* same IP. */
					context.Write("match", ConvToStr(++x) + " " + u->GetFullRealHost() + " " + u->GetIPString() + " " + u->GetRealName());
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

class ModuleCheck : public Module
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
