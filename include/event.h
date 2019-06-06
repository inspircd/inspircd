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

namespace Events
{
	class ModuleEventListener;
	class ModuleEventProvider;
}

/** Provider of one or more cross-module events.
 * Modules who wish to provide events for other modules create instances of this class and use
 * one of the macros below to fire the event, passing the instance of the event provider class
 * to the macro.
 * Event providers are identified using a unique identifier string.
 */
class Events::ModuleEventProvider : public ServiceProvider, private dynamic_reference_base::CaptureHook
{
 public:
	struct Comp
	{
		bool operator()(ModuleEventListener* one, ModuleEventListener* two) const;
	};

	typedef insp::flat_multiset<ModuleEventListener*, Comp, std::less<ModuleEventListener*> > SubscriberList;

	/** Constructor
	 * @param mod Module providing the event(s)
	 * @param eventid Identifier of the event or event group provided, must be unique
	 */
	ModuleEventProvider(Module* mod, const std::string& eventid)
		: ServiceProvider(mod, eventid, SERVICE_DATA)
		, prov(mod, eventid)
	{
		prov.SetCaptureHook(this);
	}

	/** Get list of objects subscribed to this event
	 * @return List of subscribed objects
	 */
	const SubscriberList& GetSubscribers() const { return prov->subscribers; }

	friend class ModuleEventListener;

 private:
	void OnCapture() CXX11_OVERRIDE
	{
		// If someone else holds the list from now on, clear mine. See below for more info.
		if (*prov != this)
			subscribers.clear();
	}

	/** Reference to the active provider for this event. In case multiple event providers
	 * exist for the same event, only one of them contains the list of subscribers.
	 * To handle the case when we are not the ones with the list, we get it from the provider
	 * where the dynref points to.
	 */
	dynamic_reference_nocheck<ModuleEventProvider> prov;

	/** List of objects subscribed to the event(s) provided by us, or empty if multiple providers
	 * exist with the same name and we are not the ones holding the list.
	 */
	SubscriberList subscribers;
};

/** Base class for abstract classes describing cross-module events.
 * Subscribers should NOT inherit directly from this class.
 */
class Events::ModuleEventListener : private dynamic_reference_base::CaptureHook
{
	/** Reference to the provider, can be NULL if none of the provider modules are loaded
	 */
	dynamic_reference_nocheck<ModuleEventProvider> prov;

	const unsigned int eventpriority;

	/** Called by the dynref when the event provider becomes available
	 */
	void OnCapture() CXX11_OVERRIDE
	{
		prov->subscribers.insert(this);
	}

 public:
	static const unsigned int DefaultPriority = 100;

	/** Constructor
	 * @param mod Module subscribing
	 * @param eventid Identifier of the event to subscribe to
	 * @param eventprio The priority to give this event listener
	 */
	ModuleEventListener(Module* mod, const std::string& eventid, unsigned int eventprio = DefaultPriority)
		: prov(mod, eventid)
		, eventpriority(eventprio)
	{
		prov.SetCaptureHook(this);
		// If the dynamic_reference resolved at construction our capture handler wasn't called
		if (prov)
			ModuleEventListener::OnCapture();
	}

	~ModuleEventListener()
	{
		if (prov)
			prov->subscribers.erase(this);
	}

	/** Retrieves the module which created this listener. */
	const Module* GetModule() const { return prov.creator; }

	friend struct ModuleEventProvider::Comp;
};

inline bool Events::ModuleEventProvider::Comp::operator()(Events::ModuleEventListener* one, Events::ModuleEventListener* two) const
{
	return (one->eventpriority < two->eventpriority);
}

/**
 * Run the given hook provided by a module
 *
 * FOREACH_MOD_CUSTOM(accountevprov, AccountEventListener, OnAccountChange, MOD_RESULT, (user, newaccount))
 */
#define FOREACH_MOD_CUSTOM(prov, listenerclass, func, params) do { \
	const ::Events::ModuleEventProvider::SubscriberList& _handlers = (prov).GetSubscribers(); \
	for (::Events::ModuleEventProvider::SubscriberList::const_iterator _i = _handlers.begin(); _i != _handlers.end(); ++_i) \
	{ \
		listenerclass* _t = static_cast<listenerclass*>(*_i); \
		const Module* _m = _t->GetModule(); \
		if (_m && !_m->dying) \
			_t->func params ; \
	} \
} while (0);

/**
 * Run the given hook provided by a module until some module returns MOD_RES_ALLOW or MOD_RES_DENY.
 * If no module does that, result is set to MOD_RES_PASSTHRU.
 *
 * Example: ModResult MOD_RESULT;
 * FIRST_MOD_RESULT_CUSTOM(httpevprov, HTTPRequestEventListener, OnHTTPRequest, MOD_RESULT, (request));
 */
#define FIRST_MOD_RESULT_CUSTOM(prov, listenerclass, func, result, params) do { \
	result = MOD_RES_PASSTHRU; \
	const ::Events::ModuleEventProvider::SubscriberList& _handlers = (prov).GetSubscribers(); \
	for (::Events::ModuleEventProvider::SubscriberList::const_iterator _i = _handlers.begin(); _i != _handlers.end(); ++_i) \
	{ \
		listenerclass* _t = static_cast<listenerclass*>(*_i); \
		const Module* _m = _t->GetModule(); \
		if (!_m || _m->dying) \
			continue; \
		result = _t->func params ; \
		if (result != MOD_RES_PASSTHRU) \
			break; \
	} \
} while (0);
