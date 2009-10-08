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
#include "ssl.h"

/* $ModDesc: Provides support for unreal-style channel mode +z */

/** Handle channel mode +z
 */
class SSLMode : public ModeHandler
{
 public:
	SSLMode(Module* Creator) : ModeHandler(Creator, 'z', PARAM_NONE, MODETYPE_CHANNEL) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('z'))
			{
				if (IS_LOCAL(source))
				{
					const UserMembList* userlist = channel->GetUsers();
					for(UserMembCIter i = userlist->begin(); i != userlist->end(); i++)
					{
						SSLCertificateRequest req(i->first, creator);
						req.Send();
						if(!req.cert && !ServerInstance->ULine(i->first->server))
						{
							source->WriteNumeric(ERR_ALLMUSTSSL, "%s %s :all members of the channel must be connected via SSL", source->nick.c_str(), channel->name.c_str());
							return MODEACTION_DENY;
						}
					}
				}
				channel->SetMode('z',true);
				return MODEACTION_ALLOW;
			}
			else
			{
				return MODEACTION_DENY;
			}
		}
		else
		{
			if (channel->IsModeSet('z'))
			{
				channel->SetMode('z',false);
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
	ModuleSSLModes()
		: sslm(this)
	{
		if (!ServerInstance->Modes->AddMode(&sslm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if(chan && chan->IsModeSet('z'))
		{
			SSLCertificateRequest req(user, this);
			req.Send();
			if (req.cert)
			{
				// Let them in
				return MOD_RES_PASSTHRU;
			}
			else
			{
				// Deny
				user->WriteServ( "489 %s %s :Cannot join channel; SSL users only (+z)", user->nick.c_str(), cname);
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask)
	{
		if (mask[0] == 'z' && mask[1] == ':')
		{
			SSLCertificateRequest req(user, this);
			req.Send();
			if (req.cert && InspIRCd::Match(req.cert->GetFingerprint(), mask.substr(2)))
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
		return Version("Provides support for unreal-style channel mode +z", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};


MODULE_INIT(ModuleSSLModes)

