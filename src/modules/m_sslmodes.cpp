/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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
#include "ssl.h"

/* $ModDesc: Provides channel mode +z to allow for Secure/SSL only channels */

/** Handle channel mode +z
 */
class SSLMode : public ModeHandler
{
 public:
	SSLMode(Module* Creator) : ModeHandler(Creator, "sslonly", 'z', PARAM_NONE, MODETYPE_CHANNEL) { }

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
						UserCertificateRequest req(i->first, creator);
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
	}

	void init()
	{
		ServerInstance->Modules->AddService(sslm);
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if(chan && chan->IsModeSet('z'))
		{
			UserCertificateRequest req(user, this);
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
		if ((mask.length() > 2) && (mask[0] == 'z') && (mask[1] == ':'))
		{
			UserCertificateRequest req(user, this);
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
		return Version("Provides channel mode +z to allow for Secure/SSL only channels", VF_VENDOR);
	}
};


MODULE_INIT(ModuleSSLModes)

