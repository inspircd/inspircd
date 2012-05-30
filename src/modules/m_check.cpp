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


/* $ModDesc: Provides the /CHECK command to retrieve information on a user, channel, hostname or IP address */

#include "inspircd.h"

/** Handle /CHECK
 */
class CommandCheck : public Command
{
 public:
	CommandCheck(Module* parent) : Command(parent,"CHECK", 1)
	{
		flags_needed = 'o'; syntax = "<nickname>|<ip>|<hostmask>|<channel> <server>";
	}

	std::string timestring(time_t time)
	{
		char timebuf[60];
		struct tm *mytime = gmtime(&time);
		strftime(timebuf, 59, "%Y-%m-%d %H:%M:%S UTC (%s)", mytime);
		return std::string(timebuf);
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

		checkstr = std::string(":") + ServerInstance->Config->ServerName + " 304 " + std::string(user->nick) + " :CHECK";

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
			user->SendText(checkstr + " snomasks +" + targuser->FormatNoticeMasks());
			user->SendText(checkstr + " server " + targuser->server);
			user->SendText(checkstr + " uid " + targuser->uuid);
			user->SendText(checkstr + " signon " + timestring(targuser->signon));
			user->SendText(checkstr + " nickts " + timestring(targuser->age));
			if (loctarg)
				user->SendText(checkstr + " lastmsg " + timestring(targuser->idle_lastmsg));

			if (IS_AWAY(targuser))
			{
				/* user is away */
				user->SendText(checkstr + " awaytime " + timestring(targuser->awaytime));
				user->SendText(checkstr + " awaymsg " + targuser->awaymsg);
			}

			user->SendText(checkstr + " globalclones " + ConvToStr(ServerInstance->Users->GlobalCloneCount(targuser)));
			user->SendText(checkstr + " localclones " + ConvToStr(ServerInstance->Users->LocalCloneCount(targuser)));

			if (IS_OPER(targuser))
			{
				OperInfo* oper = targuser->oper;
				/* user is an oper of type ____ */
				user->SendText(checkstr + " opertype " + oper->NameStr());
				if (loctarg)
				{
					std::string opcmds;
					for(std::set<std::string>::iterator i = oper->AllowedOperCommands.begin(); i != oper->AllowedOperCommands.end(); i++)
					{
						opcmds.push_back(' ');
						opcmds.append(*i);
					}
					std::stringstream opcmddump(opcmds);
					user->SendText(checkstr + " commandperms", opcmddump);
					std::string privs;
					for(std::set<std::string>::iterator i = oper->AllowedPrivs.begin(); i != oper->AllowedPrivs.end(); i++)
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
				user->SendText(checkstr + " connectclass " + loctarg->MyClass->name);
				user->SendText(checkstr + " stats_bytes in=" + ConvToStr(loctarg->bytes_in) + " out=" + ConvToStr(loctarg->bytes_out));
				user->SendText(checkstr + " stats_lines in=" + ConvToStr(loctarg->cmds_in) + " out=" + ConvToStr(loctarg->cmds_out));

				InvitedList* invitelist = loctarg->GetInviteList();
				std::string invites;
				for(InvitedList::iterator i = invitelist->begin(); i != invitelist->end(); i++)
				{
					invites.push_back(' ');
					invites.append(i->first);
					if (i->second)
					{
						invites.push_back(',');
						invites.append(ConvToStr(i->second));
					}
				}
				std::stringstream invitedump(invites);
				user->SendText(checkstr + " invites", invitedump);
			}
			else
				user->SendText(checkstr + " onip " + targuser->GetIPString());

			for (UCListIter i = targuser->chans.begin(); i != targuser->chans.end(); i++)
			{
				chliststr.append(i->chan->GetPrefixChar(targuser)).append(i->chan->name).append(" ");
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

			irc::modestacker cmodes;
			targchan->ChanModes(cmodes, MODELIST_FULL);
			while (!cmodes.empty())
				user->SendText(checkstr + " modes " + cmodes.popModeLine(FORMAT_USER));

			user->SendText(checkstr + " membercount " + ConvToStr(targchan->GetUserCounter()));

			const UserMembList *ulist = targchan->GetUsers();
			std::string checkstr2 = checkstr + " ";
			/* note that unlike /names, we do NOT check +i vs in the channel */
			for (UserMembCIter i = ulist->begin(); i != ulist->end(); i++)
			{
				char tmpbuf[MAXBUF];
				snprintf(tmpbuf, MAXBUF, "%s member %3lu %s%s!%s@%s",
					checkstr.c_str(), ServerInstance->Users->GlobalCloneCount(i->first),
					targchan->GetAllPrefixChars(i->first), i->first->nick.c_str(),
					i->first->ident.c_str(), i->first->dhost.c_str());
				user->SendText(std::string(tmpbuf));
				dumpExt(user, checkstr2, i->second);
			}

			dumpExt(user, checkstr, targchan);
		}
		else
		{
			/*  /check on an IP address, or something that doesn't exist */
			long x = 0;

			/* hostname or other */
			for (user_hash::const_iterator a = ServerInstance->Users->clientlist->begin(); a != ServerInstance->Users->clientlist->end(); a++)
			{
				if (InspIRCd::Match(a->second->host, parameters[0], ascii_case_insensitive_map) || InspIRCd::Match(a->second->dhost, parameters[0], ascii_case_insensitive_map))
				{
					/* host or vhost matches mask */
					user->SendText(checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost());
				}
				/* IP address */
				else if (InspIRCd::MatchCIDR(a->second->GetIPString(), parameters[0]))
				{
					/* same IP. */
					user->SendText(checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost());
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
 private:
	CommandCheck mycommand;
 public:
	ModuleCheck() : mycommand(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(mycommand);
	}

	~ModuleCheck()
	{
	}

	Version GetVersion()
	{
		return Version("CHECK command, view user, channel, IP address or hostname information", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleCheck)
