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

/* $ModDesc: Provides support for unreal-style oper-override */

class ModuleOverride : public Module
{
	bool RequireKey;
	bool NoisyOverride;

 public:
	void init()
	{
		// read our config options (main config file)
		OnRehash(NULL);
		ServerInstance->SNO->EnableSnomask('v', "OVERRIDE");
		Implementation eventlist[] = { I_OnRehash, I_OnPreMode, I_On005Numeric, I_OnUserPreJoin, I_OnChannelPermissionCheck };
		ServerInstance->Modules->Attach(eventlist, this, 5);
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

	void OnChannelPermissionCheck(User* source,Channel* chan, PermissionData& perm)
	{
		if (IS_LOCAL(source) && source->HasPermission("override/" + perm.name))
		{
			ServerInstance->SNO->WriteGlobalSno('v',source->nick+" used oper override for "+perm.name+" on "+chan->name);

			perm.result = MOD_RES_ALLOW;
		}
	}

	ModResult OnPreMode(User* source, Extensible* dest, irc::modestacker& modes)
	{
		Channel* channel = dynamic_cast<Channel*>(dest);
		if (!source || !channel)
			return MOD_RES_PASSTHRU;
		if (!IS_OPER(source) || !IS_LOCAL(source))
			return MOD_RES_PASSTHRU;

		unsigned int mode = channel->GetPrefixValue(source);

		if (mode < HALFOP_VALUE && source->HasPermission("override/mode"))
		{
			irc::modestacker tmp(modes);
			std::string msg = std::string(source->nick)+" overriding modes:" + tmp.popModeLine(FORMAT_USER);
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
				if (chan->IsModeSet('i') && user->HasPermission("override/invite"))
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

				if (chan->IsModeSet('k') && user->HasPermission("override/key") && keygiven != chan->GetModeParameter('k'))
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

				if (chan->IsModeSet('l') && (chan->GetUserCounter() >= atoi(chan->GetModeParameter('l').c_str())) && user->HasPermission("override/limit"))
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

				if (chan->IsBanned(user) && user->HasPermission("override/ban"))
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
