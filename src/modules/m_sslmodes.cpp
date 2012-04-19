/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Shawn Smith <ShawnSmith0828@gmail.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

/* $ModDesc: Provides support for secure-only channels, queries and notices. */

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

/** Handles umode z
*/
class SSLModeUser : public ModeHandler
{
	public:
		dynamic_reference<UserCertificateProvider> sslinfo;
		SSLModeUser(Module* Creator) : ModeHandler(Creator, "secure_queries", 'z', PARAM_NONE, MODETYPE_USER), sslinfo("sslinfo") { fixed_letter = false; }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('z') && sslinfo->GetCert(dest))
			{
				dest->SetMode('z', true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('z'))
			{
				dest->SetMode('z', false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleSSLModes : public Module
{

	SSLMode sslm;
	SSLModeUser sslpm;

 public:
	ModuleSSLModes() : sslm(this), sslpm(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(sslm);
		ServerInstance->Modules->AddService(sslpm);
		Implementation eventlist[] = { I_OnCheckJoin, I_OnCheckBan, I_On005Numeric, I_OnUserPreMessage, I_OnUserPreNotice };
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

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			// If recieving is +z and sending is not ulined
			if (t->IsModeSet('z') && !ServerInstance->ULine(user->server))
			{
				if (!sslpm.sslinfo || !sslpm.sslinfo->GetCert(user))
				{
					user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user (+z set)", user->nick.c_str(), t->nick.c_str());
					return MOD_RES_DENY;
				}
			}
			else if (user->IsModeSet('z') && !ServerInstance->ULine(t->server))
			{
				// If sending is +z and recieving is not ulined
				if (!sslpm.sslinfo || !sslpm.sslinfo->GetCert(t))
				{
					user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You must remove usermode 'z' before you are able to send private messages to a non-ssl user.", user->nick.c_str(), t->nick.c_str());
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNotice(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
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
		return Version("Provides support for secure-only channels, queries and notices.", VF_VENDOR);
	}
};


MODULE_INIT(ModuleSSLModes)

