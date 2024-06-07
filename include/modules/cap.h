/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015-2016, 2018 Attila Molnar <attilamolnar@hush.com>
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

#include "extension.h"

namespace Cap
{
	static constexpr unsigned int MAX_CAPS = (sizeof(intptr_t) * 8) - 1;
	static constexpr intptr_t CAP_302_BIT = (intptr_t)1 << MAX_CAPS;
	static constexpr unsigned int MAX_VALUE_LENGTH = 100;

	typedef intptr_t Ext;
	class ExtItem : public IntExtItem
	{
	public:
		ExtItem(Module* mod);
		void FromInternal(Extensible* container, const std::string& value) noexcept override;
		std::string ToHuman(const Extensible* container, void* item) const noexcept override;
		std::string ToInternal(const Extensible* container, void* item) const noexcept override;
	};

	class Capability;

	enum Protocol
	{
		/** Supports capability negotiation protocol v3.1, or none
		 */
		CAP_LEGACY,

		/** Supports capability negotiation v3.2
		 */
		CAP_302
	};

	class EventListener : public Events::ModuleEventListener
	{
	public:
		EventListener(Module* mod, unsigned int eventprio = DefaultPriority)
			: ModuleEventListener(mod, "event/cap", eventprio)
		{
		}

		/** Called whenever a new client capability becomes available or unavailable
		 * @param cap Capability being added or removed
		 * @param add If true, the capability is being added, otherwise its being removed
		 */
		virtual void OnCapAddDel(Capability* cap, bool add) = 0;

		/** Called whenever the value of a cap changes.
		 * @param cap Capability whose value changed
		 */
		virtual void OnCapValueChange(Capability* cap) { }
	};

	class Manager : public DataProvider
	{
	public:
		Manager(Module* mod)
			: DataProvider(mod, "capmanager")
		{
		}

		/** Register a client capability.
		 * Modules should call Capability::SetActive(true) instead of this method.
		 * @param cap Capability to register
		 */
		virtual void AddCap(Capability* cap) = 0;

		/** Unregister a client capability.
		 * Modules should call Capability::SetActive(false) instead of this method.
		 * @param cap Capability to unregister
		 */
		virtual void DelCap(Capability* cap) = 0;

		/** Find a capability by name
		 * @param name Capability to find
		 * @return Capability object pointer if found, NULL otherwise
		 */
		virtual Capability* Find(const std::string& name) const = 0;

		/** Notify manager when a value of a cap changed
		 * @param cap Cap whose value changed
		 */
		virtual void NotifyValueChange(Capability* cap) = 0;
	};

	/** Represents a client capability.
	 *
	 * Capabilities offer extensions to the client to server protocol. They must be negotiated with clients before they have any effect on the protocol.
	 * Each cap must have a unique name that is used during capability negotiation.
	 *
	 * After construction the cap is ready to be used by clients without any further setup, like other InspIRCd services.
	 * The get() method accepts a user as parameter and can be used to check whether that user has negotiated usage of the cap. This is only known for local users.
	 *
	 * The cap module must be loaded for the capability to work. The IsRegistered() method can be used to query whether the cap is actually online or not.
	 * The capability can be deactivated and reactivated with the SetActive() method. Deactivated caps behave as if they don't exist.
	 *
	 * It is possible to implement special behavior by inheriting from this class and overriding some of its methods.
	 */
	class Capability : public ServiceProvider, private dynamic_reference_base::CaptureHook
	{
		typedef size_t Bit;

		/** Bit allocated to this cap, undefined if the cap is unregistered
		 */
		Bit bit;

		/** Extension containing all caps set by a user. NULL if the cap is unregistered.
		 */
		ExtItem* extitem;

		/** True if the cap is active. Only active caps are registered in the manager.
		 */
		bool active = true;

		/** Reference to the cap manager object
		 */
		dynamic_reference<Manager> manager;

		void OnCapture() override
		{
			if (active)
				SetActive(true);
		}

		void Unregister()
		{
			bit = 0;
			extitem = nullptr;
		}

		Ext AddToMask(Ext mask) const { return (mask | GetMask()); }
		Ext DelFromMask(Ext mask) const { return (mask & (~GetMask())); }
		Bit GetMask() const { return bit; }

		friend class ManagerImpl;

	protected:
		/** Notify the manager that the value of the capability changed.
		 * Must be called if the value of the cap changes for any reason.
		 */
		void NotifyValueChange()
		{
			if (IsRegistered())
				manager->NotifyValueChange(this);
		}

	public:
		/** Constructor, initializes the capability.
		 * Caps are active by default.
		 * @param mod Module providing the cap
		 * @param Name Raw name of the cap as used in the protocol (CAP LS, etc.)
		 */
		Capability(Module* mod, const std::string& Name)
			: ServiceProvider(mod, Name, SERVICE_CUSTOM)
			, manager(mod, "capmanager")
		{
			Unregister();
		}

		~Capability() override
		{
			SetActive(false);
		}

		void RegisterService() override
		{
			manager.SetCaptureHook(this);
			SetActive(true);
		}

		/** Check whether a user has the capability turned on.
		 * This method is safe to call if the cap is unregistered and will return false.
		 * @param user User to check
		 * @return True if the user is using this capability, false otherwise
		 */
		bool IsEnabled(User* user) const
		{
			if (!IsRegistered())
				return false;
			Ext caps = extitem->Get(user);
			return ((caps & GetMask()) != 0);
		}

		/** Turn the capability on/off for a user. If the cap is not registered this method has no effect.
		 * @param user User to turn the cap on/off for
		 * @param val True to turn the cap on, false to turn it off
		 */
		void Set(User* user, bool val)
		{
			if (!IsRegistered())
				return;
			Ext curr = extitem->Get(user);
			extitem->Set(user, (val ? AddToMask(curr) : DelFromMask(curr)));
		}

		/** Activate or deactivate the capability.
		 * If activating, the cap is marked as active and if the manager is available the cap is registered in the manager.
		 * If deactivating, the cap is marked as inactive and if it is registered, it will be unregistered.
		 * Users who had the cap turned on will have it turned off automatically.
		 * @param activate True to activate the cap, false to deactivate it
		 */
		void SetActive(bool activate)
		{
			active = activate;
			if (manager)
			{
				if (activate)
					manager->AddCap(this);
				else
					manager->DelCap(this);
			}
		}

		/** Get the name of the capability that's used in the protocol
		 * @return Name of the capability as used in the protocol
		 */
		const std::string& GetName() const { return name; }

		/** Check whether the capability is active. The cap must be active and registered to be used by users.
		 * @return True if the cap is active, false if it has been deactivated
		 */
		bool IsActive() const { return active; }

		/** Check whether the capability is registered
		 * The cap must be active and the manager must be available for a cap to be registered.
		 * @return True if the cap is registered in the manager, false otherwise
		 */
		bool IsRegistered() const { return (extitem != nullptr); }

		/** Get the CAP negotiation protocol version of a user.
		 * The cap must be registered for this to return anything other than CAP_LEGACY.
		 * @param user User whose negotiation protocol version to query
		 * @return One of the Capability::Protocol enum indicating the highest supported capability negotiation protocol version
		 */
		Protocol GetProtocol(LocalUser* user) const
		{
			return ((IsRegistered() && (extitem->Get(user) & CAP_302_BIT)) ? CAP_302 : CAP_LEGACY);
		}

		/** Called when a user requests to turn this capability on or off.
		 * @param user User requesting to change the state of the cap
		 * @param add True if requesting to turn the cap on, false if requesting to turn it off
		 * @return True to allow the request, false to reject it
		 */
		virtual bool OnRequest(LocalUser* user, bool add)
		{
			return true;
		}

		/** Called when a user requests a list of all capabilities and this capability is about to be included in the list.
		 * The default behavior always includes the cap in the list.
		 * @param user User querying a list capabilities
		 * @return True to add this cap to the list sent to the user, false to not list it
		 */
		virtual bool OnList(LocalUser* user)
		{
			return true;
		}

		/** Query the value of this capability for a user
		 * @param user User who will get the value of the capability
		 * @return Value to show to the user. If NULL, the capability has no value (default).
		 */
		virtual const std::string* GetValue(LocalUser* user) const
		{
			return nullptr;
		}
	};

	/** Reference to a cap. The cap may be provided by another module.
	 */
	class Reference
	{
		dynamic_reference_nocheck<Capability> ref;

	public:
		/** Constructor, initializes the capability reference
		 * @param mod Module creating this object
		 * @param Name Raw name of the cap as used in the protocol (CAP LS, etc.)
		 */
		Reference(Module* mod, const std::string& Name)
			: ref(mod, "cap/" + Name)
		{
		}

		/** Retrieves the underlying cap. */
		operator const Cap::Capability*() const
		{
			return ref ? *ref : nullptr;
		}

		/** Check whether a user has the referenced capability turned on.
		 * @param user User to check
		 * @return True if the user is using the referenced capability, false otherwise
		 */
		bool IsEnabled(LocalUser* user)
		{
			if (ref)
				return ref->IsEnabled(user);
			return false;
		}

		/** Turn the capability on/off for a user. If the cap is not registered this method has no effect.
		 * @param user User to turn the cap on/off for
		 * @param val True to turn the cap on, false to turn it off
		 */
		void Set(User* user, bool val)
		{
			if (ref)
				ref->Set(user, val);
		}

	};

	class MessageBase : public ClientProtocol::Message
	{
	public:
		MessageBase(const std::string& subcmd)
			: ClientProtocol::Message("CAP", ServerInstance->Config->GetServerName())
		{
			PushParamPlaceholder();
			PushParam(subcmd);
		}

		void SetUser(LocalUser* user)
		{
			if (user->connected & User::CONN_NICK)
				ReplaceParamRef(0, user->nick);
			else
				ReplaceParam(0, "*");
		}
	};
}
