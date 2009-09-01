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
	Module* Parent;
 public:
	bool md_sent;
	CommandCheck (InspIRCd* Instance, Module* parent) : Command(Instance,"CHECK", "o", 1), Parent(parent)
	{
		this->source = "m_check.so";
		syntax = "<nickname>|<ip>|<hostmask>|<channel>";
	}

	std::string timestring(time_t time)
	{
		char timebuf[60];
		struct tm *mytime = gmtime(&time);
		strftime(timebuf, 59, "%Y-%m-%d %H:%M:%S UTC (%s)", mytime);
		return std::string(timebuf);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		User *targuser;
		Channel *targchan;
		std::string checkstr;
		std::string chliststr;

		checkstr = "304 " + std::string(user->nick) + " :CHECK";

		targuser = ServerInstance->FindNick(parameters[0]);
		targchan = ServerInstance->FindChan(parameters[0]);

		/*
		 * Syntax of a /check reply:
		 *  :server.name 304 target :CHECK START <target>
		 *  :server.name 304 target :CHECK <field> <value>
		 *  :server.name 304 target :CHECK END
		 */

		user->WriteServ(checkstr + " START " + parameters[0]);

		if (targuser)
		{
			/* /check on a user */
			user->WriteServ(checkstr + " nuh " + targuser->GetFullHost());
			user->WriteServ(checkstr + " realnuh " + targuser->GetFullRealHost());
			user->WriteServ(checkstr + " realname " + targuser->fullname);
			user->WriteServ(checkstr + " modes +" + targuser->FormatModes());
			user->WriteServ(checkstr + " snomasks +" + targuser->FormatNoticeMasks());
			user->WriteServ(checkstr + " server " + targuser->server);
			user->WriteServ(checkstr + " uid " + targuser->uuid);
			user->WriteServ(checkstr + " signon " + timestring(targuser->signon));
			user->WriteServ(checkstr + " nickts " + timestring(targuser->age));
			if (IS_LOCAL(targuser))
				user->WriteServ(checkstr + " lastmsg " + timestring(targuser->idle_lastmsg));

			if (IS_AWAY(targuser))
			{
				/* user is away */
				user->WriteServ(checkstr + " awaytime " + timestring(targuser->awaytime));
				user->WriteServ(checkstr + " awaymsg " + targuser->awaymsg);
			}

			if (IS_OPER(targuser))
			{
				/* user is an oper of type ____ */
				user->WriteServ(checkstr + " opertype " + irc::Spacify(targuser->oper.c_str()));
			}

			user->WriteServ(checkstr + " onip " + targuser->GetIPString());
			if (IS_LOCAL(targuser))
			{
				user->WriteServ(checkstr + " onport " + ConvToStr(targuser->GetServerPort()));
				std::string classname = targuser->GetClass()->name;
				if (!classname.empty())
					user->WriteServ(checkstr + " connectclass " + classname);
			}

			chliststr = targuser->ChannelList(targuser);
			std::stringstream dump(chliststr);

			ServerInstance->DumpText(user,checkstr + " onchans ", dump);

			std::deque<std::string> extlist;
			targuser->GetExtList(extlist);
			std::stringstream dumpkeys;
			for(std::deque<std::string>::iterator i = extlist.begin(); i != extlist.end(); i++)
			{
				md_sent = false;
				FOREACH_MOD_I(ServerInstance,I_OnSyncUserMetaData,OnSyncUserMetaData(targuser,Parent,(void*)user,*i, true));
				if (!md_sent)
					dumpkeys << " " << *i;
			}
			if (!dumpkeys.str().empty())
				ServerInstance->DumpText(user,checkstr + " metadata ", dumpkeys);
		}
		else if (targchan)
		{
			/* /check on a channel */
			user->WriteServ(checkstr + " timestamp " + timestring(targchan->age));

			if (targchan->topic[0] != 0)
			{
				/* there is a topic, assume topic related information exists */
				user->WriteServ(checkstr + " topic " + targchan->topic);
				user->WriteServ(checkstr + " topic_setby " + targchan->setby);
				user->WriteServ(checkstr + " topic_setat " + timestring(targchan->topicset));
			}

			user->WriteServ(checkstr + " modes " + targchan->ChanModes(true));
			user->WriteServ(checkstr + " membercount " + ConvToStr(targchan->GetUserCounter()));

			/* now the ugly bit, spool current members of a channel. :| */

			CUList *ulist= targchan->GetUsers();

			/* note that unlike /names, we do NOT check +i vs in the channel */
			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				char tmpbuf[MAXBUF];
				/*
				 * Unlike Asuka, I define a clone as coming from the same host. --w00t
				 */
				snprintf(tmpbuf, MAXBUF, "%-3lu %s%s (%s@%s) %s ", ServerInstance->Users->GlobalCloneCount(i->first), targchan->GetAllPrefixChars(i->first), i->first->nick.c_str(), i->first->ident.c_str(), i->first->dhost.c_str(), i->first->fullname.c_str());
				user->WriteServ(checkstr + " member " + tmpbuf);
			}

			std::deque<std::string> extlist;
			targchan->GetExtList(extlist);
			std::stringstream dumpkeys;
			for(std::deque<std::string>::iterator i = extlist.begin(); i != extlist.end(); i++)
			{
				md_sent = false;
				FOREACH_MOD_I(ServerInstance,I_OnSyncChannelMetaData,OnSyncChannelMetaData(targchan,Parent,(void*)user,*i, true));
				if (!md_sent)
					dumpkeys << " " << *i;
			}
			if (!dumpkeys.str().empty())
				ServerInstance->DumpText(user,checkstr + " metadata ", dumpkeys);
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
					user->WriteServ(checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost());
				}
				/* IP address */
				else if (InspIRCd::MatchCIDR(a->second->GetIPString(), parameters[0]))
				{
					/* same IP. */
					user->WriteServ(checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost());
				}
			}

			user->WriteServ(checkstr + " matches " + ConvToStr(x));
		}

		user->WriteServ(checkstr + " END " + parameters[0]);

		return CMD_LOCALONLY;
	}
};


class ModuleCheck : public Module
{
 private:
	CommandCheck *mycommand;
 public:
	ModuleCheck(InspIRCd* Me) : Module(Me)
	{
		mycommand = new CommandCheck(ServerInstance, this);
		ServerInstance->AddCommand(mycommand);
	}

	virtual ~ModuleCheck()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

	virtual void ProtoSendMetaData(void* opaque, TargetTypeFlags type, void* target, const std::string& name, const std::string& value)
	{
		User* user = static_cast<User*>(opaque);
		user->WriteServ("304 " + std::string(user->nick) + " :CHECK meta:" + name + " " + value);
		mycommand->md_sent = true;
	}
};

MODULE_INIT(ModuleCheck)
