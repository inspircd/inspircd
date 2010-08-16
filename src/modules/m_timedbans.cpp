/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
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
class TimedBan
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
	CommandTban(Module* Creator) : Command(Creator,"TBAN", 3)
	{
		syntax = "<channel> <duration> <banmask>";
		TRANSLATE4(TR_TEXT, TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		Channel* channel = ServerInstance->FindChan(parameters[0]);
		if (!channel)
		{
			user->WriteNumeric(401, "%s %s :No such channel",user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
		int cm = channel->GetAccessRank(user);
		if (cm < HALFOP_VALUE)
		{
			user->WriteNumeric(482, "%s %s :You do not have permission to set bans on this channel",
				user->nick.c_str(), channel->name.c_str());
			return CMD_FAILURE;
		}
		if (!ServerInstance->IsValidMask(parameters[2]))
		{
			user->WriteServ("NOTICE "+std::string(user->nick)+" :Invalid ban mask");
			return CMD_FAILURE;
		}
		TimedBan T;
		std::string channelname = parameters[0];
		long duration = ServerInstance->Duration(parameters[1]);
		unsigned long expire = duration + ServerInstance->Time();
		if (duration < 1)
		{
			user->WriteServ("NOTICE "+std::string(user->nick)+" :Invalid ban time");
			return CMD_FAILURE;
		}
		std::string mask = parameters[2];
		std::vector<std::string> setban;
		setban.push_back(parameters[0]);
		setban.push_back("+b");
		setban.push_back(parameters[2]);
		// use CallCommandHandler to make it so that the user sets the mode
		// themselves
		ServerInstance->CallCommandHandler("MODE",setban,user);
		CUList tmp;
		T.channel = channelname;
		T.mask = mask;
		T.expire = expire;
		TimedBanList.push_back(T);
		channel->WriteAllExcept(ServerInstance->FakeClient, true, '@', tmp, "NOTICE %s :%s added a timed ban on %s lasting for %ld seconds.", channel->name.c_str(), user->nick.c_str(), mask.c_str(), duration);
		ServerInstance->PI->SendChannelNotice(channel, '@', user->nick + " added a timed ban on " + mask + " lasting for " + ConvToStr(duration) + " seconds.");
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleTimedBans : public Module
{
	CommandTban cmd;
 public:
	ModuleTimedBans() : cmd(this) {}

	void init()
	{
		ServerInstance->AddCommand(&cmd);
		TimedBanList.clear();
		Implementation eventlist[] = { I_OnDelBan, I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleTimedBans()
	{
		TimedBanList.clear();
	}

	virtual ModResult OnDelBan(User* source, Channel* chan, const std::string &banmask)
	{
		irc::string listitem = banmask.c_str();
		irc::string thischan = chan->name.c_str();
		for (timedbans::iterator i = TimedBanList.begin(); i != TimedBanList.end(); i++)
		{
			irc::string target = i->mask.c_str();
			irc::string tchan = i->channel.c_str();
			if ((listitem == target) && (tchan == thischan))
			{
				TimedBanList.erase(i);
				break;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void OnBackgroundTimer(time_t curtime)
	{
		for (timedbans::iterator i = TimedBanList.begin(); i != TimedBanList.end();)
		{
			if (curtime > i->expire)
			{
				std::string chan = i->channel;
				std::string mask = i->mask;
				Channel* cr = ServerInstance->FindChan(chan);
				i = TimedBanList.erase(i);
				if (cr)
				{
					std::vector<std::string> setban;
					setban.push_back(chan);
					setban.push_back("-b");
					setban.push_back(mask);

					CUList empty;
					std::string expiry = "*** Timed ban on " + chan + " expired.";
					cr->WriteAllExcept(ServerInstance->FakeClient, true, '@', empty, "NOTICE %s :%s", cr->name.c_str(), expiry.c_str());
					ServerInstance->PI->SendChannelNotice(cr, '@', expiry);

					ServerInstance->SendGlobalMode(setban, ServerInstance->FakeClient);
				}
			}
			else
				++i;
		}
	}

	virtual Version GetVersion()
	{
		return Version("Adds timed bans", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleTimedBans)

