/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Val Lorentz <progval+git@progval.net>
 *   Copyright (C) 2019, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015-2016 Attila Molnar <attilamolnar@hush.com>
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


#pragma once

namespace Whois
{
	class EventListener;
	class LineEventListener;
	class Context;
}

enum
{
	// InspIRCd-specific.
	RPL_WHOISCOUNTRY = 344,
	RPL_WHOISGATEWAY = 350,
	RPL_CHANNELSMSG = 651,

	// From ircu.
	RPL_WHOISACCOUNT = 330,

	// From oftc-hybrid.
	RPL_WHOISCERTFP = 276,

	// From ircd-hybrid.
	RPL_WHOISACTUALLY = 338,

	// From RFC 1459.
	RPL_WHOISUSER = 311,
	RPL_WHOISSERVER = 312,
	RPL_WHOISOPERATOR = 313,
	RPL_WHOISIDLE = 317,
	RPL_ENDOFWHOIS = 318,
	RPL_WHOISCHANNELS = 319,

	// From UnrealIRCd.
	RPL_WHOISREGNICK = 307,
	RPL_WHOISHELPOP = 310,
	RPL_WHOISSPECIAL = 320,
	RPL_WHOISBOT = 335,
	RPL_WHOISMODES = 379,
	RPL_WHOISSECURE = 671
};

class Whois::EventListener
	: public Events::ModuleEventListener
{
public:
	EventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/whois", eventprio)
	{
	}

	/** Called whenever a /WHOIS is performed by a local user.
	 * @param whois Whois context, can be used to send numerics
	 */
	virtual void OnWhois(Context& whois) = 0;
};

class Whois::LineEventListener
	: public Events::ModuleEventListener
{
public:
	LineEventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/whoisline", eventprio)
	{
	}

	/** Called whenever a line of WHOIS output is sent to a user.
	 * You may change the numeric and the text of the output by changing
	 * the values numeric and text, but you cannot change the user the
	 * numeric is sent to.
	 * @param whois Whois context, can be used to send numerics
	 * @param numeric Numeric being sent
	 * @return MOD_RES_DENY to drop the line completely so that the user does not
	 * receive it, or MOD_RES_PASSTHRU to allow the line to be sent.
	 */
	virtual ModResult OnWhoisLine(Context& whois, Numeric::Numeric& numeric) = 0;
};

class Whois::Context
{
protected:
	/** User doing the WHOIS
	 */
	LocalUser* const source;

	/** User being WHOISed
	 */
	User* const target;

public:
	Context(LocalUser* sourceuser, User* targetuser)
		: source(sourceuser)
		, target(targetuser)
	{
	}

	/** Returns true if the user is /WHOISing himself
	 * @return True if whois source is the same user as the whois target, false if they are different users
	 */
	bool IsSelfWhois() const { return (source == target); }

	/** Returns the LocalUser who has done the /WHOIS
	 * @return LocalUser doing the /WHOIS
	 */
	LocalUser* GetSource() const { return source; }

	/** Returns the target of the /WHOIS
	 * @return User who was /WHOIS'd
	 */
	User* GetTarget() const { return target; }

	/** Send a line of WHOIS data to the source of the WHOIS
	 */
	template <typename... Param>
	void SendLine(unsigned int numeric, Param&&... p)
	{
		Numeric::Numeric n(numeric);
		n.push(target->nick);
		n.push(std::forward<Param>(p)...);
		SendLine(n);
	}

	/** Send a line of WHOIS data to the source of the WHOIS
	 * @param numeric Numeric to send
	 */
	virtual void SendLine(Numeric::Numeric& numeric) = 0;
};
