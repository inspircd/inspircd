/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Shawn Smith <shawn@inspircd.org>
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

/* $ModDesc: Provides user and channel mode +z to allow for Secure/SSL-only channels, queries and notices. */

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

/** Handle user mode +z
*/

class SSLModeUser : public ModeHandler
{
public:
	SSLModeUser(Module* Creator) : ModeHandler(Creator, "sslqueries", 'z', PARAM_NONE, MODETYPE_USER) {}
	ModeAction OnModeChange(User* user, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('z'))
			{
				/* Check to make sure the user is on an SSL connection */
				UserCertificateRequest req(user, creator);
				req.Send();
				if (!req.cert)
					return MODEACTION_DENY;

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
	SSLModeUser sslquery;

 public:
	ModuleSSLModes() : sslm(this), sslquery(this) { }

	void init()
	{
		ServerInstance->Modules->AddService(sslm);
		ServerInstance->Modules->AddService(sslquery);
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnCheckBan, I_On005Numeric, I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven)
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
				user->WriteServ( "489 %s %s :Cannot join channel; SSL users only (+z)", user->nick.c_str(), cname.c_str());
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type =! TYPE_USER)
			return MOD_RES_PASSTHRU;

		User* target = (User*)dest;

		/* If one or more of the parties involved is a ulined service, we wont stop it. */
		if (ServerInstance->ULine(user->server) || ServerInstance->ULine(target->server))
			return MOD_RES_PASSTHRU;

		/* If the target is +z */
		if (target->IsModeSet('z'))
		{
			UserCertificateRequest req(user, this);
			req.Send();
			/* The sending user is not on an SSL connection */
			if (!req.cert)
			{
				user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user (+z set)", user->nick.c_str(), target->nick.c_str());
				return MOD_RES_DENY;
			}
		}
		/* If the user is +z */
		else if (user->IsModeSet('z'))
		{
			UserCertificateRequest req(target, this);
			req.Send();

			if (!req.cert)
			{
				user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You must remove usermode 'z' before you are able to send private messages to a non-ssl user.", user->nick.c_str(), target->nick.c_str());
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNotice(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
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

	void On005Numeric(std::map<std::string, std::string>& tokens)
	{
		tokens["EXTBAN"].push_back('z');
	}

	Version GetVersion()
	{
		return Version("Provides user and channel mode +z to allow for Secure/SSL-only channels, queries and notices.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSSLModes)
