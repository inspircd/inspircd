/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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

	void dumpExt(User* user, std::string checkstr, Extensible* ext)
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
			if (IS_LOCAL(targuser))
				user->SendText(checkstr + " lastmsg " + timestring(targuser->idle_lastmsg));

			if (IS_AWAY(targuser))
			{
				/* user is away */
				user->SendText(checkstr + " awaytime " + timestring(targuser->awaytime));
				user->SendText(checkstr + " awaymsg " + targuser->awaymsg);
			}

			if (IS_OPER(targuser))
			{
				/* user is an oper of type ____ */
				user->SendText(checkstr + " opertype " + targuser->oper->NameStr());
			}

			LocalUser* loctarg = IS_LOCAL(targuser);
			if (loctarg)
			{
				user->SendText(checkstr + " clientaddr " + irc::sockets::satouser(loctarg->client_sa));
				user->SendText(checkstr + " serveraddr " + irc::sockets::satouser(loctarg->server_sa));

				std::string classname = loctarg->GetClass()->name;
				if (!classname.empty())
					user->SendText(checkstr + " connectclass " + classname);
			}
			else
				user->SendText(checkstr + " onip " + targuser->GetIPString());

			for (UCListIter i = targuser->chans.begin(); i != targuser->chans.end(); i++)
			{
				Channel* c = *i;
				chliststr.append(c->GetPrefixChar(targuser)).append(c->name).append(" ");
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

			const UserMembList *ulist= targchan->GetUsers();

			/* note that unlike /names, we do NOT check +i vs in the channel */
			for (UserMembCIter i = ulist->begin(); i != ulist->end(); i++)
			{
				char tmpbuf[MAXBUF];
				/*
				 * Unlike Asuka, I define a clone as coming from the same host. --w00t
				 */
				snprintf(tmpbuf, MAXBUF, "%-3lu %s%s (%s@%s) %s ", ServerInstance->Users->GlobalCloneCount(i->first), targchan->GetAllPrefixChars(i->first), i->first->nick.c_str(), i->first->ident.c_str(), i->first->dhost.c_str(), i->first->fullname.c_str());
				user->SendText(checkstr + " member " + tmpbuf);
			}

			irc::modestacker modestack(true);
			std::string letter_b("b");
			for(BanList::iterator b = targchan->bans.begin(); b != targchan->bans.end(); ++b)
			{
				modestack.Push('b', b->data);
			}
			std::vector<std::string> stackresult;
			std::vector<TranslateType> dummy;
			while (modestack.GetStackedLine(stackresult))
			{
				creator->ProtoSendMode(user, TYPE_CHANNEL, targchan, stackresult, dummy);
				stackresult.clear();
			}
			FOREACH_MOD(I_OnSyncChannel,OnSyncChannel(targchan,creator,user));
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
		ServerInstance->AddCommand(&mycommand);
	}

	~ModuleCheck()
	{
	}

	void ProtoSendMode(void* uv, TargetTypeFlags, void*, const std::vector<std::string>& result, const std::vector<TranslateType>&)
	{
		User* user = (User*)uv;
		std::string checkstr(":");
		checkstr.append(ServerInstance->Config->ServerName);
		checkstr.append(" 304 ");
		checkstr.append(user->nick);
		checkstr.append(" :CHECK modelist");
		for(unsigned int i=0; i < result.size(); i++)
		{
			checkstr.append(" ");
			checkstr.append(result[i]);
		}
		user->SendText(checkstr);
	}

	Version GetVersion()
	{
		return Version("CHECK command, view user/channel details", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleCheck)
