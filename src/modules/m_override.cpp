/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2008 Geoff Bricker <geoff.bricker@gmail.com>
 *   Copyright (C) 2004-2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

/* $ModDesc: Provides support for unreal-style oper-override */

class ModuleOverride : public Module
{
	bool RequireKey;
	bool NoisyOverride;

 public:

	ModuleOverride()
			{
		// read our config options (main config file)
		OnRehash(NULL);
		ServerInstance->SNO->EnableSnomask('v', "OVERRIDE");
		Implementation eventlist[] = { I_OnRehash, I_OnPreMode, I_On005Numeric, I_OnUserPreJoin, I_OnUserPreKick, I_OnPreTopicChange };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}

	void OnRehash(User* user)
	{
		// on a rehash we delete our classes for good measure and create them again.
		ConfigReader Conf;

		// re-read our config options on a rehash
		NoisyOverride = Conf.ReadFlag("override", "noisy", 0);
		RequireKey = Conf.ReadFlag("override", "requirekey", 0);
	}

	void On005Numeric(std::string &output)
	{
		output.append(" OVERRIDE");
	}

	bool CanOverride(User* source, const char* token)
	{
		std::string tokenlist = source->oper->getConfig("override");

		// its defined or * is set, return its value as a boolean for if the token is set
		return ((tokenlist.find(token, 0) != std::string::npos) || (tokenlist.find("*", 0) != std::string::npos));
	}


	ModResult OnPreTopicChange(User *source, Channel *channel, const std::string &topic)
	{
		if (IS_LOCAL(source) && IS_OPER(source) && CanOverride(source, "TOPIC"))
		{
			if (!channel->HasUser(source) || (channel->IsModeSet('t') && channel->GetPrefixValue(source) < HALFOP_VALUE))
			{
				ServerInstance->SNO->WriteGlobalSno('v',std::string(source->nick)+" used oper override to change a topic on "+std::string(channel->name));
			}

			// Explicit allow
			return MOD_RES_ALLOW;
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string &reason)
	{
		if (IS_OPER(source) && CanOverride(source,"KICK"))
		{
			// If the kicker's status is less than the target's,			or	the kicker's status is less than or equal to voice
			if ((memb->chan->GetPrefixValue(source) < memb->getRank()) || (memb->chan->GetPrefixValue(source) <= VOICE_VALUE))
			{
				ServerInstance->SNO->WriteGlobalSno('v',std::string(source->nick)+" used oper override to kick "+std::string(memb->user->nick)+" on "+std::string(memb->chan->name)+" ("+reason+")");
				return MOD_RES_ALLOW;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreMode(User* source,User* dest,Channel* channel, const std::vector<std::string>& parameters)
	{
		if (!source || !channel)
			return MOD_RES_PASSTHRU;
		if (!IS_OPER(source) || !IS_LOCAL(source))
			return MOD_RES_PASSTHRU;

		unsigned int mode = channel->GetPrefixValue(source);

		if (mode < HALFOP_VALUE && CanOverride(source, "MODE"))
		{
			std::string msg = std::string(source->nick)+" overriding modes:";
			for(unsigned int i=0; i < parameters.size(); i++)
				msg += " " + parameters[i];
			ServerInstance->SNO->WriteGlobalSno('v',msg);
			return MOD_RES_ALLOW;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (IS_LOCAL(user) && IS_OPER(user))
		{
			if (chan)
			{
				if (chan->IsModeSet('i') && (CanOverride(user,"INVITE")))
				{
					irc::string x(chan->name.c_str());
					if (!IS_LOCAL(user)->IsInvited(x))
					{
						if (RequireKey && keygiven != "override")
						{
							// Can't join normally -- must use a special key to bypass restrictions
							user->WriteServ("NOTICE %s :*** You may not join normally. You must join with a key of 'override' to oper override.", user->nick.c_str());
							return MOD_RES_PASSTHRU;
						}

						if (NoisyOverride)
							chan->WriteChannelWithServ(ServerInstance->Config->ServerName.c_str(), "NOTICE %s :%s used oper override to bypass invite-only", cname, user->nick.c_str());
						ServerInstance->SNO->WriteGlobalSno('v', user->nick+" used oper override to bypass +i on "+std::string(cname));
					}
					return MOD_RES_ALLOW;
				}

				if (chan->IsModeSet('k') && (CanOverride(user,"KEY")) && keygiven != chan->GetModeParameter('k'))
				{
					if (RequireKey && keygiven != "override")
					{
						// Can't join normally -- must use a special key to bypass restrictions
						user->WriteServ("NOTICE %s :*** You may not join normally. You must join with a key of 'override' to oper override.", user->nick.c_str());
						return MOD_RES_PASSTHRU;
					}

					if (NoisyOverride)
						chan->WriteChannelWithServ(ServerInstance->Config->ServerName.c_str(), "NOTICE %s :%s used oper override to bypass the channel key", cname, user->nick.c_str());
					ServerInstance->SNO->WriteGlobalSno('v', user->nick+" used oper override to bypass +k on "+std::string(cname));
					return MOD_RES_ALLOW;
				}

				if (chan->IsModeSet('l') && (chan->GetUserCounter() >= atoi(chan->GetModeParameter('l').c_str())) && (CanOverride(user,"LIMIT")))
				{
					if (RequireKey && keygiven != "override")
					{
						// Can't join normally -- must use a special key to bypass restrictions
						user->WriteServ("NOTICE %s :*** You may not join normally. You must join with a key of 'override' to oper override.", user->nick.c_str());
						return MOD_RES_PASSTHRU;
					}

					if (NoisyOverride)
						chan->WriteChannelWithServ(ServerInstance->Config->ServerName.c_str(), "NOTICE %s :%s used oper override to bypass the channel limit", cname, user->nick.c_str());
					ServerInstance->SNO->WriteGlobalSno('v', user->nick+" used oper override to bypass +l on "+std::string(cname));
					return MOD_RES_ALLOW;
				}

				if (chan->IsBanned(user) && CanOverride(user,"BANWALK"))
				{
					if (RequireKey && keygiven != "override")
					{
						// Can't join normally -- must use a special key to bypass restrictions
						user->WriteServ("NOTICE %s :*** You may not join normally. You must join with a key of 'override' to oper override.", user->nick.c_str());
						return MOD_RES_PASSTHRU;
					}

					if (NoisyOverride)
						chan->WriteChannelWithServ(ServerInstance->Config->ServerName.c_str(), "NOTICE %s :%s used oper override to bypass channel ban", cname, user->nick.c_str());
					ServerInstance->SNO->WriteGlobalSno('v',"%s used oper override to bypass channel ban on %s", user->nick.c_str(), cname);
					return MOD_RES_ALLOW;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Provides support for unreal-style oper-override",VF_VENDOR);
	}
};

MODULE_INIT(ModuleOverride)
