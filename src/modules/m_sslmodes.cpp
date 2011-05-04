/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "ssl.h"

/* $ModDesc: Provides support for unreal-style channel mode +z */

/** Handle channel mode +z
 */
class SSLMode : public ModeHandler
{
 public:
	dynamic_reference<UserCertificateProvider> sslinfo;
	SSLMode(Module* Creator) : ModeHandler(Creator, "sslonly", 'z', PARAM_NONE, MODETYPE_CHANNEL),
		sslinfo("sslinfo") { fixed_letter = false; }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet(this))
			{
				if (IS_LOCAL(source))
				{
					const UserMembList* userlist = channel->GetUsers();
					for(UserMembCIter i = userlist->begin(); i != userlist->end(); i++)
					{
						if ((!sslinfo || !sslinfo->GetCert(i->first)) && !ServerInstance->ULine(i->first->server))
						{
							source->WriteNumeric(ERR_ALLMUSTSSL, "%s %s :all members of the channel must be connected via SSL", source->nick.c_str(), channel->name.c_str());
							return MODEACTION_DENY;
						}
					}
				}
				channel->SetMode(this,true);
				return MODEACTION_ALLOW;
			}
			else
			{
				return MODEACTION_DENY;
			}
		}
		else
		{
			if (channel->IsModeSet(this))
			{
				channel->SetMode(this,false);
				return MODEACTION_ALLOW;
			}

			return MODEACTION_DENY;
		}
	}
};

class ModuleSSLModes : public Module
{

	SSLMode sslm;

 public:
	ModuleSSLModes() : sslm(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(sslm);
		Implementation eventlist[] = { I_OnCheckJoin, I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (!join.chan || join.result != MOD_RES_PASSTHRU || !join.chan->IsModeSet(&sslm))
			return;
		if (!sslm.sslinfo || !sslm.sslinfo->GetCert(join.user))
		{
			join.ErrorNumeric(489, "%s :Cannot join channel; SSL users only (+z)", join.chan->name.c_str());
			join.result = MOD_RES_DENY;
		}
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask)
	{
		if (mask[0] == 'z' && mask[1] == ':' && sslm.sslinfo)
		{
			if (InspIRCd::Match(sslm.sslinfo->GetFingerprint(user), mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	~ModuleSSLModes()
	{
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('z');
	}

	Version GetVersion()
	{
		return Version("Provides support for unreal-style channel mode +z", VF_VENDOR);
	}
};


MODULE_INIT(ModuleSSLModes)

