/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "modules/ctctags.h"
#include "modules/exemption.h"
#include "modules/extban.h"

class ModuleOpModerated final
	: public Module
	, public CTCTags::EventListener
{
private:
	CheckExemption::EventProvider exemptionprov;
	ExtBan::Acting extban;
	SimpleChannelMode mode;

	ModResult HandleMessage(User* user, MessageTarget& target, CUList& exemptions)
	{
		// We only handle messages from local users.
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		// We only handle messages non-statusmsg messages sent to channels.
		if (target.type != MessageTarget::TYPE_CHANNEL || target.status)
			return MOD_RES_PASSTHRU;

		// Exempt opers who have the appropriate privileges.
		if (user->HasPrivPermission("channels/ignore-opmoderated"))
			return MOD_RES_PASSTHRU;

		// Exempt channel members who have the appropriate privileges.
		Channel* const chan = target.Get<Channel>();
		if (chan->GetPrefixValue(user) > VOICE_VALUE)
			return MOD_RES_PASSTHRU;

		// Check for channel members who are exempted by a module.
		if (exemptionprov.Check(user, chan, "opmoderated") == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		if (!extban.GetStatus(user, chan).check(!chan->IsModeSet(mode)))
			target.status = '@';

		return MOD_RES_PASSTHRU;
	}

public:
	ModuleOpModerated()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds channel mode U (opmoderated) which hides the messages of unprivileged users from other unprivileged users.")
		, CTCTags::EventListener(this)
		, exemptionprov(this)
		, extban(this, "opmoderated", 'u')
		, mode(this, "opmoderated", 'U')
	{
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		return HandleMessage(user, target, details.exemptions);
	}

	ModResult OnUserPreTagMessage(User* user, MessageTarget& target, CTCTags::TagMessageDetails& details) override
	{
		return HandleMessage(user, target, details.exemptions);
	}
};

MODULE_INIT(ModuleOpModerated)
