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
	 * @param numeric The numeric of the line being sent
	 * @param text The text of the numeric, including any parameters
	 * @return MOD_RES_DENY to drop the line completely so that the user does not
	 * receive it, or MOD_RES_PASSTHRU to allow the line to be sent.
	 */
	virtual ModResult OnWhoisLine(Context& whois, unsigned int& numeric, std::string& text) = 0;
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
	 * @param numeric Numeric to send
	 * @param format Format string for the numeric
	 * @param ... Parameters for the format string
	 */
	void SendLine(unsigned int numeric, const char* format, ...) CUSTOM_PRINTF(3, 4)
	{
		std::string textbuffer;
		VAFORMAT(textbuffer, format, format)
		SendLine(numeric, textbuffer);
	}

	/** Send a line of WHOIS data to the source of the WHOIS
	 * @param numeric Numeric to send
	 * @param text Text of the numeric
	 */
	virtual void SendLine(unsigned int numeric, const std::string& text) = 0;
};
