/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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

#include "event.h"

namespace Whois
{
	class EventListener;
	class LineEventListener;
	class Context;
}

class Whois::EventListener : public Events::ModuleEventListener
{
 public:
	EventListener(Module* mod)
		: ModuleEventListener(mod, "event/whois")
	{
	}

	/** Called whenever a /WHOIS is performed by a local user.
	 * @param whois Whois context, can be used to send numerics
	 */
	virtual void OnWhois(Context& whois) = 0;
};

class Whois::LineEventListener : public Events::ModuleEventListener
{
 public:
	LineEventListener(Module* mod)
		: ModuleEventListener(mod, "event/whoisline")
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
	Context(LocalUser* src, User* targ)
		: source(src)
		, target(targ)
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
	template <typename T1>
	void SendLine(unsigned int numeric, T1 p1)
	{
		Numeric::Numeric n(numeric);
		n.push(target->nick);
		n.push(p1);
		SendLine(n);
	}

	template <typename T1, typename T2>
	void SendLine(unsigned int numeric, T1 p1, T2 p2)
	{
		Numeric::Numeric n(numeric);
		n.push(target->nick);
		n.push(p1);
		n.push(p2);
		SendLine(n);
	}

	template <typename T1, typename T2, typename T3>
	void SendLine(unsigned int numeric, T1 p1, T2 p2, T3 p3)
	{
		Numeric::Numeric n(numeric);
		n.push(target->nick);
		n.push(p1);
		n.push(p2);
		n.push(p3);
		SendLine(n);
	}

	template <typename T1, typename T2, typename T3, typename T4>
	void SendLine(unsigned int numeric, T1 p1, T2 p2, T3 p3, T4 p4)
	{
		Numeric::Numeric n(numeric);
		n.push(target->nick);
		n.push(p1);
		n.push(p2);
		n.push(p3);
		n.push(p4);
		SendLine(n);
	}

	/** Send a line of WHOIS data to the source of the WHOIS
	 * @param numeric Numeric to send
	 */
	virtual void SendLine(Numeric::Numeric& numeric) = 0;
};
