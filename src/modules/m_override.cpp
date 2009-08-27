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
#include "m_override.h"

/* $ModDesc: Provides support for unreal-style oper-override */

typedef std::map<std::string,std::string> override_t;

class ModuleOverride : public Module
{
	override_t overrides;
	bool RequireKey;
	bool NoisyOverride;
	bool OverriddenMode;
	bool OverOther;
	int OverOps, OverDeops, OverVoices, OverDevoices, OverHalfops, OverDehalfops;

 public:

	ModuleOverride(InspIRCd* Me)
		: Module(Me)
	{
		// read our config options (main config file)
		OnRehash(NULL);
		ServerInstance->SNO->EnableSnomask('G', "GODMODE");
		if (!ServerInstance->Modules->PublishFeature("Override", this))
		{
			throw ModuleException("m_override: Unable to publish feature 'Override'");
		}
		OverriddenMode = OverOther = false;
		OverOps = OverDeops = OverVoices = OverDevoices = OverHalfops = OverDehalfops = 0;
		Implementation eventlist[] = { I_OnRehash, I_OnAccessCheck, I_On005Numeric, I_OnUserPreJoin, I_OnUserPreKick, I_OnPostCommand, I_OnLocalTopicChange, I_OnRequest };
		ServerInstance->Modules->Attach(eventlist, this, 8);
	}

	virtual void OnRehash(User* user)
	{
		// on a rehash we delete our classes for good measure and create them again.
		ConfigReader Conf(ServerInstance);

		// re-read our config options on a rehash
		NoisyOverride = Conf.ReadFlag("override", "noisy", 0);
		RequireKey = Conf.ReadFlag("override", "requirekey", 0);

		overrides.clear();

		for (int j =0; j < Conf.Enumerate("type"); j++)
		{
			std::string typen = Conf.ReadValue("type","name",j);
			std::string tokenlist = Conf.ReadValue("type","override",j);
			overrides[typen] = tokenlist;
		}
	}


	virtual void OnPostCommand(const std::string &command, const std::vector<std::string> &parameters, User *user, CmdResult result, const std::string &original_line)
	{
		if (OverriddenMode)
		{
			if ((irc::string(command.c_str()) == "MODE") && (result == CMD_SUCCESS) && !ServerInstance->Modes->GetLastParse().empty())
			{
				std::string msg = std::string(user->nick)+" overriding modes: "+ServerInstance->Modes->GetLastParse()+" [Detail: ";
				if (OverOps)
					msg += ConvToStr(OverOps)+" op"+(OverOps != 1 ? "s" : "")+", ";
				if (OverDeops)
					msg += ConvToStr(OverDeops)+" deop"+(OverDeops != 1 ? "s" : "")+", ";
				if (OverVoices)
					msg += ConvToStr(OverVoices)+" voice"+(OverVoices != 1 ? "s" : "")+", ";
				if (OverDevoices)
					msg += ConvToStr(OverDevoices)+" devoice"+(OverDevoices != 1 ? "s" : "")+", ";
				if (OverHalfops)
					msg += ConvToStr(OverHalfops)+" halfop"+(OverHalfops != 1 ? "s" : "")+", ";
				if (OverDehalfops)
					msg += ConvToStr(OverDehalfops)+" dehalfop"+(OverDehalfops != 1 ? "s" : "")+", ";
				if (OverOther)
					msg += "others, ";
				msg.replace(msg.length()-2, 2, 1, ']');
				ServerInstance->SNO->WriteGlobalSno('G',msg);
			}

			OverriddenMode = OverOther = false;
			OverOps = OverDeops = OverVoices = OverDevoices = OverHalfops = OverDehalfops = 0;
		}
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" OVERRIDE");
	}

	virtual bool CanOverride(User* source, const char* token)
	{
		// checks to see if the oper's type has <type:override>
		override_t::iterator j = overrides.find(source->oper);

		if (j != overrides.end())
		{
			// its defined or * is set, return its value as a boolean for if the token is set
			return ((j->second.find(token, 0) != std::string::npos) || (j->second.find("*", 0) != std::string::npos));
		}

		// its not defined at all, count as false
		return false;
	}


	virtual int OnLocalTopicChange(User *source, Channel *channel, const std::string &topic)
	{
		if (IS_OPER(source) && CanOverride(source, "TOPIC"))
		{
			if (!channel->HasUser(source) || (channel->IsModeSet('t') && channel->GetStatus(source) < STATUS_HOP))
			{
				ServerInstance->SNO->WriteGlobalSno('G',std::string(source->nick)+" used oper override to change a topic on "+std::string(channel->name));
			}

			// Explicit allow
			return -1;
		}

		return 0;
	}

	virtual int OnUserPreKick(User* source, User* user, Channel* chan, const std::string &reason)
	{
		if (IS_OPER(source) && CanOverride(source,"KICK"))
		{
			// If the kicker's status is less than the target's,			or	the kicker's status is less than or equal to voice
			if ((chan->GetStatus(source) < chan->GetStatus(user))			|| (chan->GetStatus(source) <= STATUS_VOICE))
			{
				ServerInstance->SNO->WriteGlobalSno('G',std::string(source->nick)+" used oper override to kick "+std::string(user->nick)+" on "+std::string(chan->name)+" ("+reason+")");
			}
			/* Returning -1 explicitly allows the kick */
			return -1;
		}
		return 0;
	}

	virtual int OnAccessCheck(User* source,User* dest,Channel* channel,int access_type)
	{
		if (!IS_OPER(source))
			return ACR_DEFAULT;
		if (!source || !channel)
			return ACR_DEFAULT;

		int mode = STATUS_NORMAL;
		if (channel->HasUser(source))
			mode = channel->GetStatus(source);
		bool over_this = false;

		switch (access_type)
		{
			case AC_DEOP:
				if (mode < STATUS_OP && CanOverride(source,"MODEDEOP"))
				{
					over_this = true;
					OverDeops++;
				}
			break;
			case AC_OP:
				if (mode < STATUS_OP && CanOverride(source,"MODEOP"))
				{
					over_this = true;
					OverOps++;
				}
			break;
			case AC_VOICE:
				if (mode < STATUS_HOP && CanOverride(source,"MODEVOICE"))
				{
					over_this = true;
					OverVoices++;
				}
			break;
			case AC_DEVOICE:
				if (mode < STATUS_HOP && CanOverride(source,"MODEDEVOICE"))
				{
					over_this = true;
					OverDevoices++;
				}
			break;
			case AC_HALFOP:
				if (mode < STATUS_OP && CanOverride(source,"MODEHALFOP"))
				{
					over_this = true;
					OverHalfops++;
				}
			break;
			case AC_DEHALFOP:
				if (mode < STATUS_OP && CanOverride(source,"MODEDEHALFOP"))
				{
					over_this = true;
					OverDehalfops++;
				}
			break;
			case AC_GENERAL_MODE:
			{
				std::string modes = ServerInstance->Modes->GetLastParse();
				bool ohv_only = (modes.find_first_not_of("+-ohv") == std::string::npos);

				if (mode < STATUS_HOP && (ohv_only || CanOverride(source,"OTHERMODE")))
				{
					over_this = true;
					if (!ohv_only)
						OverOther = true;
				}
			}
			break;
		}

		if (over_this)
		{
			OverriddenMode = true;
			return ACR_ALLOW;
		}
		else
		{
			return ACR_DEFAULT;
		}
	}

	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (IS_LOCAL(user) && IS_OPER(user))
		{
			if (chan)
			{
				if ((chan->modes[CM_INVITEONLY]) && (CanOverride(user,"INVITE")))
				{
					irc::string x(chan->name.c_str());
					if (!user->IsInvited(x))
					{
						if (RequireKey && keygiven != "override")
						{
							// Can't join normally -- must use a special key to bypass restrictions
							user->WriteServ("NOTICE %s :*** You may not join normally. You must join with a key of 'override' to oper override.", user->nick.c_str());
							return 0;
						}

						if (NoisyOverride)
							chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper override to bypass invite-only", cname, user->nick.c_str());
						ServerInstance->SNO->WriteGlobalSno('G', user->nick+" used oper override to bypass +i on "+std::string(cname));
					}
					return -1;
				}

				if ((chan->modes[CM_KEY]) && (CanOverride(user,"KEY")) && keygiven != chan->GetModeParameter('k'))
				{
					if (RequireKey && keygiven != "override")
					{
						// Can't join normally -- must use a special key to bypass restrictions
						user->WriteServ("NOTICE %s :*** You may not join normally. You must join with a key of 'override' to oper override.", user->nick.c_str());
						return 0;
					}

					if (NoisyOverride)
						chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper override to bypass the channel key", cname, user->nick.c_str());
					ServerInstance->SNO->WriteGlobalSno('G', user->nick+" used oper override to bypass +k on "+std::string(cname));
					return -1;
				}

				if ((chan->modes[CM_LIMIT]) && (chan->GetUserCounter() >= atoi(chan->GetModeParameter('l').c_str())) && (CanOverride(user,"LIMIT")))
				{
					if (RequireKey && keygiven != "override")
					{
						// Can't join normally -- must use a special key to bypass restrictions
						user->WriteServ("NOTICE %s :*** You may not join normally. You must join with a key of 'override' to oper override.", user->nick.c_str());
						return 0;
					}

					if (NoisyOverride)
						chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper override to bypass the channel limit", cname, user->nick.c_str());
					ServerInstance->SNO->WriteGlobalSno('G', user->nick+" used oper override to bypass +l on "+std::string(cname));
					return -1;
				}

				if (chan->IsBanned(user) && CanOverride(user,"BANWALK"))
				{
					if (RequireKey && keygiven != "override")
					{
						// Can't join normally -- must use a special key to bypass restrictions
						user->WriteServ("NOTICE %s :*** You may not join normally. You must join with a key of 'override' to oper override.", user->nick.c_str());
						return 0;
					}

					if (NoisyOverride)
						chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper override to bypass channel ban", cname, user->nick.c_str());
					ServerInstance->SNO->WriteGlobalSno('G',"%s used oper override to bypass channel ban on %s", user->nick.c_str(), cname);
					return -1;
				}
			}
		}
		return 0;
	}

	virtual const char* OnRequest(Request* request)
	{
		if(strcmp(OVRREQID, request->GetId()) == 0)
		{
			OVRrequest* req = static_cast<OVRrequest*>(request);
			return this->CanOverride(req->requser,req->reqtoken.c_str()) ? "yes":"";
		}
		return NULL;
	}

	virtual ~ModuleOverride()
	{
		ServerInstance->Modules->UnpublishFeature("Override");
		ServerInstance->SNO->DisableSnomask('G');
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleOverride)
