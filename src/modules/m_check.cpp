/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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

/** Handle /CHECK
 */
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

	static void dumpListMode(User* user, const std::string& checkstr, const ListModeBase::ModeList* list)
	{
		if (!list)
			return;

		std::string buf = checkstr + " modelist";
		const std::string::size_type headlen = buf.length();
		const size_t maxline = ServerInstance->Config->Limits.MaxLine;
		for (ListModeBase::ModeList::const_iterator i = list->begin(); i != list->end(); ++i)
		{
			if (buf.size() + i->mask.size() + 1 > maxline)
			{
				user->SendText(buf);
				buf.erase(headlen);
			}
			buf.append(" ").append(i->mask);
		}
		if (buf.length() > headlen)
			user->SendText(buf);
	}

 public:
	CommandCheck(Module* parent)
		: Command(parent,"CHECK", 1)
		, snomaskmode(parent, "snomask")
	{
		flags_needed = 'o'; syntax = "<nickname>|<ip>|<hostmask>|<channel> <server>";
	}

	std::string timestring(time_t time)
	{
		char timebuf[60];
		struct tm *mytime = gmtime(&time);
		strftime(timebuf, 59, "%Y-%m-%d %H:%M:%S UTC (", mytime);
		std::string ret(timebuf);
		ret.append(ConvToStr(time)).push_back(')');
		return ret;
	}

	void dumpExt(User* user, const std::string& checkstr, Extensible* ext)
	{
		std::stringstream dumpkeys;
		for(Extensible::ExtensibleStore::const_iterator i = ext->GetExtList().begin(); i != ext->GetExtList().end(); i++)
		{
			ExtensionItem* item = i->first;
			std::string value = item->serialize(FORMAT_USER, ext, i->second);
			if (!value.empty())
				user->SendText(checkstr + " meta:" + item->name + " " + value);
			else if (!item->name.empty())
				dumpkeys << " " << item->name;
		}
		if (!dumpkeys.str().empty())
			user->SendText(checkstr + " metadata", dumpkeys);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		if (parameters.size() > 1 && parameters[1] != ServerInstance->Config->ServerName.c_str())
			return CMD_SUCCESS;

		User *targuser;
		Channel *targchan;
		std::string checkstr;
		std::string chliststr;

		checkstr = ":" + ServerInstance->Config->ServerName + " 304 " + user->nick + " :CHECK";

		targuser = ServerInstance->FindNick(parameters[0]);
		targchan = ServerInstance->FindChan(parameters[0]);

		/*
		 * Syntax of a /check reply:
		 *  :server.name 304 target :CHECK START <target>
		 *  :server.name 304 target :CHECK <field> <value>
		 *  :server.name 304 target :CHECK END
		 */

		user->SendText(checkstr + " START " + parameters[0]);

		if (targuser)
		{
			LocalUser* loctarg = IS_LOCAL(targuser);
			/* /check on a user */
			user->SendText(checkstr + " nuh " + targuser->GetFullHost());
			user->SendText(checkstr + " realnuh " + targuser->GetFullRealHost());
			user->SendText(checkstr + " realname " + targuser->fullname);
			user->SendText(checkstr + " modes +" + targuser->FormatModes());
			user->SendText(checkstr + " snomasks " + GetSnomasks(targuser));
			user->SendText(checkstr + " server " + targuser->server->GetName());
			user->SendText(checkstr + " uid " + targuser->uuid);
			user->SendText(checkstr + " signon " + timestring(targuser->signon));
			user->SendText(checkstr + " nickts " + timestring(targuser->age));
			if (loctarg)
				user->SendText(checkstr + " lastmsg " + timestring(loctarg->idle_lastmsg));

			if (targuser->IsAway())
			{
				/* user is away */
				user->SendText(checkstr + " awaytime " + timestring(targuser->awaytime));
				user->SendText(checkstr + " awaymsg " + targuser->awaymsg);
			}

			if (targuser->IsOper())
			{
				OperInfo* oper = targuser->oper;
				/* user is an oper of type ____ */
				user->SendText(checkstr + " opertype " + oper->name);
				if (loctarg)
				{
					std::string umodes;
					std::string cmodes;
					for(char c='A'; c <= 'z'; c++)
					{
						ModeHandler* mh = ServerInstance->Modes->FindMode(c, MODETYPE_USER);
						if (mh && mh->NeedsOper() && loctarg->HasModePermission(c, MODETYPE_USER))
							umodes.push_back(c);
						mh = ServerInstance->Modes->FindMode(c, MODETYPE_CHANNEL);
						if (mh && mh->NeedsOper() && loctarg->HasModePermission(c, MODETYPE_CHANNEL))
							cmodes.push_back(c);
					}
					user->SendText(checkstr + " modeperms user=" + umodes + " channel=" + cmodes);
					std::string opcmds;
					for (OperInfo::PrivSet::const_iterator i = oper->AllowedOperCommands.begin(); i != oper->AllowedOperCommands.end(); ++i)
					{
						opcmds.push_back(' ');
						opcmds.append(*i);
					}
					std::stringstream opcmddump(opcmds);
					user->SendText(checkstr + " commandperms", opcmddump);
					std::string privs;
					for (OperInfo::PrivSet::const_iterator i = oper->AllowedPrivs.begin(); i != oper->AllowedPrivs.end(); ++i)
					{
						privs.push_back(' ');
						privs.append(*i);
					}
					std::stringstream privdump(privs);
					user->SendText(checkstr + " permissions", privdump);
				}
			}

			if (loctarg)
			{
				user->SendText(checkstr + " clientaddr " + loctarg->client_sa.str());
				user->SendText(checkstr + " serveraddr " + loctarg->server_sa.str());

				std::string classname = loctarg->GetClass()->name;
				if (!classname.empty())
					user->SendText(checkstr + " connectclass " + classname);
			}
			else
				user->SendText(checkstr + " onip " + targuser->GetIPString());

			for (User::ChanList::iterator i = targuser->chans.begin(); i != targuser->chans.end(); i++)
			{
				Membership* memb = *i;
				Channel* c = memb->chan;
				char prefix = memb->GetPrefixChar();
				if (prefix)
					chliststr.push_back(prefix);
				chliststr.append(c->name).push_back(' ');
			}

			std::stringstream dump(chliststr);

			user->SendText(checkstr + " onchans", dump);

			dumpExt(user, checkstr, targuser);
		}
		else if (targchan)
		{
			/* /check on a channel */
			user->SendText(checkstr + " timestamp " + timestring(targchan->age));

			if (targchan->topic[0] != 0)
			{
				/* there is a topic, assume topic related information exists */
				user->SendText(checkstr + " topic " + targchan->topic);
				user->SendText(checkstr + " topic_setby " + targchan->setby);
				user->SendText(checkstr + " topic_setat " + timestring(targchan->topicset));
			}

			user->SendText(checkstr + " modes " + targchan->ChanModes(true));
			user->SendText(checkstr + " membercount " + ConvToStr(targchan->GetUserCounter()));

			/* now the ugly bit, spool current members of a channel. :| */

			const Channel::MemberMap& ulist = targchan->GetUsers();

			/* note that unlike /names, we do NOT check +i vs in the channel */
			for (Channel::MemberMap::const_iterator i = ulist.begin(); i != ulist.end(); ++i)
			{
				/*
			 	 * Unlike Asuka, I define a clone as coming from the same host. --w00t
			 	 */
				const UserManager::CloneCounts& clonecount = ServerInstance->Users->GetCloneCounts(i->first);
				user->SendText("%s member %-3u %s%s (%s@%s) %s ",
					checkstr.c_str(), clonecount.global,
					i->second->GetAllPrefixChars(), i->first->nick.c_str(),
					i->first->ident.c_str(), i->first->dhost.c_str(), i->first->fullname.c_str());
			}

			const ModeParser::ListModeList& listmodes = ServerInstance->Modes->GetListModes();
			for (ModeParser::ListModeList::const_iterator i = listmodes.begin(); i != listmodes.end(); ++i)
				dumpListMode(user, checkstr, (*i)->GetList(targchan));

			dumpExt(user, checkstr, targchan);
		}
		else
		{
			/*  /check on an IP address, or something that doesn't exist */
			long x = 0;

			/* hostname or other */
			const user_hash& users = ServerInstance->Users->GetUsers();
			for (user_hash::const_iterator a = users.begin(); a != users.end(); ++a)
			{
				if (InspIRCd::Match(a->second->host, parameters[0], ascii_case_insensitive_map) || InspIRCd::Match(a->second->dhost, parameters[0], ascii_case_insensitive_map))
				{
					/* host or vhost matches mask */
					user->SendText(checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost() + " " + a->second->GetIPString() + " " + a->second->fullname);
				}
				/* IP address */
				else if (InspIRCd::MatchCIDR(a->second->GetIPString(), parameters[0]))
				{
					/* same IP. */
					user->SendText(checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost() + " " + a->second->GetIPString() + " " + a->second->fullname);
				}
			}

			user->SendText(checkstr + " matches " + ConvToStr(x));
		}

		user->SendText(checkstr + " END " + parameters[0]);

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (parameters.size() > 1)
			return ROUTE_OPT_UCAST(parameters[1]);
		return ROUTE_LOCALONLY;
	}
};

class ModuleCheck : public Module
{
	CommandCheck mycommand;
 public:
	ModuleCheck() : mycommand(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("CHECK command, view user, channel, IP address or hostname information", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleCheck)
