/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Shawn Smith <ShawnSmith0828@gmail.com>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <brain@inspircd.org>
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
#include "modules/callerid.h"
#include "modules/ctctags.h"
#include "modules/extban.h"
#include "modules/ssl.h"
#include "numerichelper.h"

enum
{
	// From UnrealIRCd.
	ERR_SECUREONLYCHAN = 489,

	// InspIRCd-specific.
	ERR_ALLMUSTSSL = 490
};

class FingerprintExtBan final
	: public ExtBan::MatchingBase
{
private:
	UserCertificateAPI& sslapi;

public:
	FingerprintExtBan(Module* Creator, UserCertificateAPI& api)
		: ExtBan::MatchingBase(Creator, "fingerprint", 'z')
		, sslapi(api)
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text, const ExtBan::MatchConfig& config) override
	{
		if (!sslapi)
			return false;

		for (const auto& fingerprint : sslapi->GetFingerprints(user))
		{
			if (InspIRCd::Match(fingerprint, text))
				return true;
		}
		return false;
	}
};

/** Handle channel mode +z
 */
class SSLMode final
	: public ModeHandler
{
private:
	UserCertificateAPI& API;

public:
	SSLMode(Module* Creator, UserCertificateAPI& api)
		: ModeHandler(Creator, "sslonly", 'z', PARAM_NONE, MODETYPE_CHANNEL)
		, API(api)
	{
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (change.adding)
		{
			if (!channel->IsModeSet(this))
			{
				if (IS_LOCAL(source))
				{
					if (!API)
					{
						source->WriteNumeric(ERR_ALLMUSTSSL, channel->name, "Unable to determine whether all members of the channel are connected via TLS");
						return false;
					}

					size_t nonssl = 0;
					for (const auto& [u, _] : channel->GetUsers())
					{
						if (!API->IsSecure(u) && !u->server->IsService())
							nonssl++;
					}

					if (nonssl)
					{
						source->WriteNumeric(ERR_ALLMUSTSSL, channel->name, FMT::format("All members of the channel must be connected via TLS ({}/{} are non-TLS)",
							nonssl, channel->GetUsers().size()));
						return false;
					}
				}
				channel->SetMode(this, true);
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			if (channel->IsModeSet(this))
			{
				channel->SetMode(this, false);
				return true;
			}

			return false;
		}
	}
};

/** Handle user mode +z
*/
class SSLModeUser final
	: public ModeHandler
{
private:
	UserCertificateAPI& API;

public:
	SSLModeUser(Module* Creator, UserCertificateAPI& api)
		: ModeHandler(Creator, "sslqueries", 'z', PARAM_NONE, MODETYPE_USER)
		, API(api)
	{
	}

	bool OnModeChange(User* user, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (change.adding == dest->IsModeSet(this))
			return false;

		if (change.adding && IS_LOCAL(user) && (!API || !API->IsSecure(user)))
			return false;

		dest->SetMode(this, change.adding);
		return true;
	}
};

class ModuleSSLModes final
	: public Module
	, public CTCTags::EventListener
{
private:
	UserCertificateAPI api;
	CallerID::API calleridapi;
	SSLMode sslm;
	SSLModeUser sslquery;
	FingerprintExtBan extban;

public:
	ModuleSSLModes()
		: Module(VF_VENDOR, "Adds channel mode z (sslonly) which prevents users who are not connecting using TLS from joining the channel and user mode z (sslqueries) to prevent messages from non-TLS users.")
		, CTCTags::EventListener(this)
		, api(this)
		, calleridapi(this)
		, sslm(this, api)
		, sslquery(this, api)
		, extban(this, api)
	{
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (!override && chan && chan->IsModeSet(sslm))
		{
			if (!api)
			{
				user->WriteNumeric(ERR_SECUREONLYCHAN, cname, FMT::format("Cannot join channel; unable to determine if you are a TLS user (+{} is set)",
					sslm.GetModeChar()));
				return MOD_RES_DENY;
			}

			if (!api->IsSecure(user))
			{
				user->WriteNumeric(ERR_SECUREONLYCHAN, cname, FMT::format("Cannot join channel; TLS users only (+{} is set)",
					sslm.GetModeChar()));
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult HandleMessage(User* user, const MessageTarget& msgtarget)
	{
		if (msgtarget.type != MessageTarget::TYPE_USER)
			return MOD_RES_PASSTHRU;

		auto* target = msgtarget.Get<User>();

		/* If one or more of the parties involved is a services user, we won't stop it. */
		if (user->server->IsService() || target->server->IsService())
			return MOD_RES_PASSTHRU;

		/* If the target is +z */
		if (target->IsModeSet(sslquery))
		{
			const bool is_secure = api && api->IsSecure(user);
			const bool is_accepted = calleridapi && calleridapi->IsOnAcceptList(user, target);
			if (!is_secure && !is_accepted)
			{
				/* The sending user is not on an TLS connection */
				user->WriteNumeric(Numerics::CannotSendTo(target, "messages", &sslquery));
				return MOD_RES_DENY;
			}
		}
		/* If the user is +z */
		else if (user->IsModeSet(sslquery))
		{
			if (!api || !api->IsSecure(target))
			{
				user->WriteNumeric(Numerics::CannotSendTo(target, "messages", &sslquery, true));
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		return HandleMessage(user, target);
	}

	ModResult OnUserPreTagMessage(User* user, MessageTarget& target, CTCTags::TagMessageDetails& details) override
	{
		return HandleMessage(user, target);
	}
};

MODULE_INIT(ModuleSSLModes)
