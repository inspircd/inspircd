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

#include "inspircd.h"

/* $ModDesc: Provides the /check command to retrieve information on a user, channel, or IP address */

/** Handle /CHECK
 */
class CommandCheck : public Command
{
 public:
	CommandCheck (InspIRCd* Instance, Module* parent) : Command(Instance,parent,"CHECK", "o", 1)
	{
		syntax = "<nickname>|<ip>|<hostmask>|<channel> <server>";
	}

	std::string timestring(time_t time)
	{
		char timebuf[60];
		struct tm *mytime = gmtime(&time);
		strftime(timebuf, 59, "%Y-%m-%d %H:%M:%S UTC (%s)", mytime);
		return std::string(timebuf);
	}

	void dumpExt(User* user, std::string checkstr, Extensible* ext)
	{
		std::stringstream dumpkeys;
		for(ExtensibleStore::const_iterator i = ext->GetExtList().begin(); i != ext->GetExtList().end(); i++)
		{
			ExtensionItem* item = Extensible::GetItem(i->first);
			std::string value;
			if (item)
				value = item->serialize(FORMAT_USER, ext, i->second);
			if (value.empty())
				dumpkeys << " " << i->first;
			else
				ServerInstance->DumpText(user, checkstr + " meta:" + i->first + " " + value);
		}
		if (!dumpkeys.str().empty())
			ServerInstance->DumpText(user,checkstr + " metadata", dumpkeys);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		if (parameters.size() > 1 && parameters[1] != ServerInstance->Config->ServerName)
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

		ServerInstance->DumpText(user, checkstr + " START " + parameters[0]);

		if (targuser)
		{
			/* /check on a user */
			ServerInstance->DumpText(user, checkstr + " nuh " + targuser->GetFullHost());
			ServerInstance->DumpText(user, checkstr + " realnuh " + targuser->GetFullRealHost());
			ServerInstance->DumpText(user, checkstr + " realname " + targuser->fullname);
			ServerInstance->DumpText(user, checkstr + " modes +" + targuser->FormatModes());
			ServerInstance->DumpText(user, checkstr + " snomasks +" + targuser->FormatNoticeMasks());
			ServerInstance->DumpText(user, checkstr + " server " + targuser->server);
			ServerInstance->DumpText(user, checkstr + " uid " + targuser->uuid);
			ServerInstance->DumpText(user, checkstr + " signon " + timestring(targuser->signon));
			ServerInstance->DumpText(user, checkstr + " nickts " + timestring(targuser->age));
			if (IS_LOCAL(targuser))
				ServerInstance->DumpText(user, checkstr + " lastmsg " + timestring(targuser->idle_lastmsg));

			if (IS_AWAY(targuser))
			{
				/* user is away */
				ServerInstance->DumpText(user, checkstr + " awaytime " + timestring(targuser->awaytime));
				ServerInstance->DumpText(user, checkstr + " awaymsg " + targuser->awaymsg);
			}

			if (IS_OPER(targuser))
			{
				/* user is an oper of type ____ */
				ServerInstance->DumpText(user, checkstr + " opertype " + irc::Spacify(targuser->oper.c_str()));
			}

			if (IS_LOCAL(targuser))
			{
				ServerInstance->DumpText(user, checkstr + " clientaddr " + irc::sockets::satouser(&targuser->client_sa));
				ServerInstance->DumpText(user, checkstr + " serveraddr " + irc::sockets::satouser(&targuser->server_sa));

				std::string classname = targuser->GetClass()->name;
				if (!classname.empty())
					ServerInstance->DumpText(user, checkstr + " connectclass " + classname);
			}
			else
				ServerInstance->DumpText(user, checkstr + " onip " + targuser->GetIPString());

			chliststr = targuser->ChannelList(targuser);
			std::stringstream dump(chliststr);

			ServerInstance->DumpText(user,checkstr + " onchans", dump);

			dumpExt(user, checkstr, targuser);
		}
		else if (targchan)
		{
			/* /check on a channel */
			ServerInstance->DumpText(user, checkstr + " timestamp " + timestring(targchan->age));

			if (targchan->topic[0] != 0)
			{
				/* there is a topic, assume topic related information exists */
				ServerInstance->DumpText(user, checkstr + " topic " + targchan->topic);
				ServerInstance->DumpText(user, checkstr + " topic_setby " + targchan->setby);
				ServerInstance->DumpText(user, checkstr + " topic_setat " + timestring(targchan->topicset));
			}

			ServerInstance->DumpText(user, checkstr + " modes " + targchan->ChanModes(true));
			ServerInstance->DumpText(user, checkstr + " membercount " + ConvToStr(targchan->GetUserCounter()));

			/* now the ugly bit, spool current members of a channel. :| */

			const UserMembList *ulist= targchan->GetUsers();

			/* note that unlike /names, we do NOT check +i vs in the channel */
			for (UserMembCIter i = ulist->begin(); i != ulist->end(); i++)
			{
				char tmpbuf[MAXBUF];
				/*
				 * Unlike Asuka, I define a clone as coming from the same host. --w00t
				 */
				snprintf(tmpbuf, MAXBUF, "%-3lu %s%s (%s@%s) %s ", ServerInstance->Users->GlobalCloneCount(i->first), targchan->GetAllPrefixChars(i->first), i->first->nick.c_str(), i->first->ident.c_str(), i->first->dhost.c_str(), i->first->fullname.c_str());
				ServerInstance->DumpText(user, checkstr + " member " + tmpbuf);
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
					ServerInstance->DumpText(user, checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost());
				}
				/* IP address */
				else if (InspIRCd::MatchCIDR(a->second->GetIPString(), parameters[0]))
				{
					/* same IP. */
					ServerInstance->DumpText(user, checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost());
				}
			}

			ServerInstance->DumpText(user, checkstr + " matches " + ConvToStr(x));
		}

		ServerInstance->DumpText(user, checkstr + " END " + parameters[0]);

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
	ModuleCheck(InspIRCd* Me) : Module(Me), mycommand(Me, this)
	{
		ServerInstance->AddCommand(&mycommand);
	}

	~ModuleCheck()
	{
	}

	Version GetVersion()
	{
		return Version("CHECK command, view user/channel details", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleCheck)
