/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2023 Sadie Powell <sadie@sadiepowell.dev>
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

namespace ISupport
{
	class API;
	class APIBase;
	class EventListener;
	class EventProvider;

	/* A mapping of ISUPPORT tokens to their values. */
	using TokenMap = insp::casemapped_map<std::string>;
}

enum
{
	// From draft-brocklesby-irc-isupport-03.
	RPL_ISUPPORT = 5,
};

/** Defines the interface for the ISupport API. */
class ISupport::APIBase
	: public Service::SimpleProvider
{
public:
	APIBase(const WeakModulePtr& mod)
		: Service::SimpleProvider(mod, "isupportapi")
	{
	}

	/** Sends a full ISupport numeric set to the specified user.
	 * @param user The user to send to.
	 */
	virtual void SendTo(LocalUser* user) = 0;
};

/** Allows modules to send ISupport messages to users. */
class ISupport::API final
	: public dynamic_reference<ISupport::APIBase>
{
public:
	API(const WeakModulePtr& mod)
		: dynamic_reference<ISupport::APIBase>(mod, "isupportapi")
	{
	}
};

class ISupport::EventListener
	: public Events::ModuleEventListener
{
protected:
	EventListener(const WeakModulePtr& mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "isupport", eventprio)
	{
	}

public:
	virtual void OnBuildISupport(TokenMap& tokens) { }
	virtual void OnBuildClassISupport(const std::shared_ptr<ConnectClass>& klass, TokenMap& tokens) { }
	virtual void OnBuildOperISupport(LocalUser* user, TokenMap& tokens) { }
	virtual ModResult OnSendISupportDiff(LocalUser* user, const TokenMap& tokens)
	{
		return MOD_RES_PASSTHRU;
	}
};

class ISupport::EventProvider final
	: public Events::ModuleEventProvider
{
public:
	EventProvider(const WeakModulePtr& mod)
		: Events::ModuleEventProvider(mod, "isupport")
	{
	}
};
