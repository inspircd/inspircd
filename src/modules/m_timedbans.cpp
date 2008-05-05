/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Adds timed bans */

#include "inspircd.h"

/** Holds a timed ban
 */
class TimedBan : public classbase
{
 public:
	std::string channel;
	std::string mask;
	time_t expire;
};

typedef std::vector<TimedBan> timedbans;
timedbans TimedBanList;

/** Handle /TBAN
 */
class CommandTban : public Command
{
 public:
	CommandTban (InspIRCd* Instance) : Command(Instance,"TBAN", 0, 3)
	{
		this->source = "m_timedbans.so";
		syntax = "<channel> <duration> <banmask>";
		TRANSLATE4(TR_TEXT, TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		Channel* channel = ServerInstance->FindChan(parameters[0]);
		if (channel)
		{
			int cm = channel->GetStatus(user);
			if ((cm == STATUS_HOP) || (cm == STATUS_OP))
			{
				if (!ServerInstance->IsValidMask(parameters[2]))
				{
					user->WriteServ("NOTICE "+std::string(user->nick)+" :Invalid ban mask");
					return CMD_FAILURE;
				}
				for (BanList::iterator i = channel->bans.begin(); i != channel->bans.end(); i++)
				{
					if (!strcasecmp(i->data,parameters[2]))
					{
						user->WriteServ("NOTICE "+std::string(user->nick)+" :The ban "+std::string(parameters[2])+" is already on the banlist of "+std::string(parameters[0]));
						return CMD_FAILURE;
					}
				}
				TimedBan T;
				std::string channelname = parameters[0];
				long duration = ServerInstance->Duration(parameters[1]);
				unsigned long expire = duration + time(NULL);
				if (duration < 1)
				{
					user->WriteServ("NOTICE "+std::string(user->nick)+" :Invalid ban time");
					return CMD_FAILURE;
				}
				std::string mask = parameters[2];
				const char *setban[32];
				setban[0] = parameters[0];
				setban[1] = "+b";
				setban[2] = parameters[2];
				// use CallCommandHandler to make it so that the user sets the mode
				// themselves
				ServerInstance->CallCommandHandler("MODE",setban,3,user);
				/* Check if the ban was actually added (e.g. banlist was NOT full) */
				bool was_added = false;
				for (BanList::iterator i = channel->bans.begin(); i != channel->bans.end(); i++)
					if (!strcasecmp(i->data,mask.c_str()))
						was_added = true;
				if (was_added)
				{
					CUList tmp;
					T.channel = channelname;
					T.mask = mask;
					T.expire = expire;
					TimedBanList.push_back(T);
					channel->WriteAllExcept(user, true, '@', tmp, "NOTICE %s :%s added a timed ban on %s lasting for %ld seconds.", channel->name, user->nick, mask.c_str(), duration);
					if (ServerInstance->Config->AllowHalfop)
						channel->WriteAllExcept(user, true, '%', tmp, "NOTICE %s :%s added a timed ban on %s lasting for %ld seconds.", channel->name, user->nick, mask.c_str(), duration);
					return CMD_SUCCESS;
				}
				return CMD_FAILURE;
			}
			else user->WriteNumeric(482, "%s %s :You must be at least a%soperator to change modes on this channel",user->nick, channel->name,
					ServerInstance->Config->AllowHalfop ? " half-" : " channel ");
			return CMD_FAILURE;
		}
		user->WriteNumeric(401, "%s %s :No such channel",user->nick, parameters[0]);
		return CMD_FAILURE;
	}
};

class ModuleTimedBans : public Module
{
	CommandTban* mycommand;
 public:
	ModuleTimedBans(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new CommandTban(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		TimedBanList.clear();
		Implementation eventlist[] = { I_OnDelBan, I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}
	
	virtual ~ModuleTimedBans()
	{
		TimedBanList.clear();
	}

	virtual int OnDelBan(User* source, Channel* chan, const std::string &banmask)
	{
		irc::string listitem = banmask.c_str();
		irc::string thischan = chan->name;
		for (timedbans::iterator i = TimedBanList.begin(); i < TimedBanList.end(); i++)
		{
			irc::string target = i->mask.c_str();
			irc::string tchan = i->channel.c_str();
			if ((listitem == target) && (tchan == thischan))
			{
				TimedBanList.erase(i);
				break;
			}
		}
		return 0;
	}

	virtual void OnBackgroundTimer(time_t curtime)
	{
		timedbans::iterator safei;
		for (timedbans::iterator i = TimedBanList.begin(); i < TimedBanList.end();)
		{
			/* Safe copy of iterator, so we can erase as we iterate */
			safei = i;
			++i;

			if (curtime > safei->expire)
			{
				Channel* cr = ServerInstance->FindChan(safei->channel);
				if (cr)
				{
					const char *setban[3];
					std::string mask = safei->mask;

					setban[0] = safei->channel.c_str();
					setban[1] = "-b";
					setban[2] = mask.c_str();

					CUList empty;
					cr->WriteAllExcept(ServerInstance->FakeClient, true, '@', empty, "NOTICE %s :*** Timed ban on %s expired.", cr->name, safei->mask.c_str());
					if (ServerInstance->Config->AllowHalfop)
						cr->WriteAllExcept(ServerInstance->FakeClient, true, '%', empty, "NOTICE %s :*** Timed ban on %s expired.", cr->name, safei->mask.c_str());

					/* Removes the ban item for us, no ::erase() needed */
					ServerInstance->PI->SendModeStr(safei->channel, std::string("-b ") + setban[2]);
					ServerInstance->SendMode(setban, 3, ServerInstance->FakeClient);

					if (ServerInstance->Modes->GetLastParse().empty())
						TimedBanList.erase(safei);
				}
				else
				{
					/* Where the hell did our channel go?! */
					TimedBanList.erase(safei);
				}
			}
		}
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 2, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleTimedBans)

