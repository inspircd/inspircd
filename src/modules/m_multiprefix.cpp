/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "modules/cap.h"
#include "modules/names.h"
#include "modules/who.h"
#include "modules/whois.h"

class ModuleMultiPrefix final
	: public Module
	, public Names::EventListener
	, public Who::EventListener
	, public Whois::LineEventListener
{
private:
	Cap::Capability cap;

public:
	ModuleMultiPrefix()
		: Module(VF_VENDOR, "Provides the IRCv3 multi-prefix client capability.")
		, Names::EventListener(this)
		, Who::EventListener(this)
		, Whois::LineEventListener(this)
		, cap(this, "multi-prefix")
	{
	}

	ModResult OnNamesListItem(LocalUser* issuer, Membership* memb, std::string& prefixes, std::string& nick) override
	{
		if (cap.IsEnabled(issuer))
			prefixes = memb->GetAllPrefixChars();

		return MOD_RES_PASSTHRU;
	}

	ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric) override
	{
		if ((!memb) || (!cap.IsEnabled(source)))
			return MOD_RES_PASSTHRU;

		// Don't do anything if the user has only one prefix
		if (memb->modes.size() <= 1)
			return MOD_RES_PASSTHRU;

		size_t flag_index;
		if (!request.GetFieldIndex('f', flag_index))
			return MOD_RES_PASSTHRU;

		if (numeric.GetParams().size() <= flag_index)
			return MOD_RES_PASSTHRU;

		numeric.GetParams()[flag_index].append(memb->GetAllPrefixChars(), 1, std::string::npos);
		return MOD_RES_PASSTHRU;
	}

	ModResult OnWhoisLine(Whois::Context& whois, Numeric::Numeric& numeric) override
	{
		if (numeric.GetNumeric() != RPL_WHOISCHANNELS || !cap.IsEnabled(whois.GetSource()))
			return MOD_RES_PASSTHRU;

		// :testnet.inspircd.org 319 test Sadie :#test ~#inspircd
		if (numeric.GetParams().size() < 2 || numeric.GetParams().back().empty())
			return MOD_RES_PASSTHRU;

		std::stringstream newchannels;
		irc::spacesepstream channelstream(numeric.GetParams().back());
		for (std::string channel; channelstream.GetToken(channel); )
		{
			size_t hashpos = channel.find('#');
			if (!hashpos || hashpos == std::string::npos)
			{
				// The entry is malformed or the user has no privs.
				newchannels << channel << ' ';
				continue;
			}

			auto* chan = ServerInstance->Channels.Find(channel.substr(hashpos));
			if (!chan)
			{
				// Should never happen.
				newchannels << channel << ' ';
				continue;
			}

			Membership* memb = chan->GetUser(whois.GetTarget());
			if (!memb)
			{
				// Should never happen.
				newchannels << channel << ' ';
				continue;
			}

			newchannels << memb->GetAllPrefixChars() << chan->name << ' ';
		}

		numeric.GetParams().back() = newchannels.str();
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleMultiPrefix)
