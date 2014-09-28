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
#include "modules/ssl.h"

/** Handle channel mode +z
 */
class SSLMode : public ModeHandler
{
 public:
	UserCertificateAPI API;

	SSLMode(Module* Creator)
		: ModeHandler(Creator, "sslonly", 'z', PARAM_NONE, MODETYPE_CHANNEL)
		, API(Creator)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet(this))
			{
				if (IS_LOCAL(source))
				{
					if (!API)
						return MODEACTION_DENY;

					const Channel::MemberMap& userlist = channel->GetUsers();
					for (Channel::MemberMap::const_iterator i = userlist.begin(); i != userlist.end(); ++i)
					{
						ssl_cert* cert = API->GetCertificate(i->first);
						if (!cert && !i->first->server->IsULine())
						{
							source->WriteNumeric(ERR_ALLMUSTSSL, "%s :all members of the channel must be connected via SSL", channel->name.c_str());
							return MODEACTION_DENY;
						}
					}
				}
				channel->SetMode(this, true);
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
				channel->SetMode(this, false);
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

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if(chan && chan->IsModeSet(sslm))
		{
			if (!sslm.API)
				return MOD_RES_DENY;

			ssl_cert* cert = sslm.API->GetCertificate(user);
			if (cert)
			{
				// Let them in
				return MOD_RES_PASSTHRU;
			}
			else
			{
				// Deny
				user->WriteNumeric(489, "%s :Cannot join channel; SSL users only (+z)", cname.c_str());
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask) CXX11_OVERRIDE
	{
		if ((mask.length() > 2) && (mask[0] == 'z') && (mask[1] == ':'))
		{
			if (!sslm.API)
				return MOD_RES_DENY;

			ssl_cert* cert = sslm.API->GetCertificate(user);
			if (cert && InspIRCd::Match(cert->GetFingerprint(), mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('z');
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +z to allow for Secure/SSL only channels", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSSLModes)
