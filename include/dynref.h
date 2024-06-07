/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Attila Molnar <attilamolnar@hush.com>
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

#include "base.h"

class CoreExport dynamic_reference_base
	: public insp::intrusive_list_node<dynamic_reference_base>
{
public:
	class CaptureHook
	{
	public:
		/** Called when the target of the dynamic_reference has been acquired
		 */
		virtual void OnCapture() = 0;
	};

private:
	std::string name;
	CaptureHook* hook = nullptr;
	void resolve();
	static void* operator new(std::size_t) = delete;
	static void* operator new[](std::size_t) = delete;
protected:
	ServiceProvider* value = nullptr;
public:
	ModuleRef creator;
	dynamic_reference_base(Module* Creator, const std::string& Name);
	dynamic_reference_base(const dynamic_reference_base&) = default;
	~dynamic_reference_base();

	dynamic_reference_base& operator=(const dynamic_reference_base& rhs)
	{
		SetProvider(rhs.GetProvider());
		return *this;
	}

	inline const std::string& GetProvider() const { return name; }
	void ClearProvider();
	void SetProvider(const std::string& newname);

	/** Set handler to call when the target object becomes available
	 * @param h Handler to call
	 */
	void SetCaptureHook(CaptureHook* h) { hook = h; }

	void check();
	operator bool() const { return (value != nullptr); }
	static void reset_all();
};

inline void dynamic_reference_base::check()
{
	if (!value)
		throw ModuleException(creator, "Dynamic reference to '" + name + "' failed to resolve. Are you missing a module?");
}

template<typename T>
class dynamic_reference
	: public dynamic_reference_base
{
public:
	dynamic_reference(Module* Creator, const std::string& Name)
		: dynamic_reference_base(Creator, Name) {}

	inline T* operator->()
	{
		check();
		return static_cast<T*>(value);
	}

	T* operator*()
	{
		return operator->();
	}

	const T* operator->() const
	{
		return static_cast<T*>(value);
	}

	const T* operator*() const
	{
		return operator->();
	}
};

template<typename T>
class dynamic_reference_nocheck
	: public dynamic_reference_base
{
public:
	dynamic_reference_nocheck(Module* Creator, const std::string& Name)
		: dynamic_reference_base(Creator, Name) {}

	T* operator->()
	{
		return static_cast<T*>(value);
	}

	T* operator*()
	{
		return operator->();
	}

	const T* operator->() const
	{
		return static_cast<T*>(value);
	}

	const T* operator*() const
	{
		return operator->();
	}
};

class ModeHandler;
class ChanModeReference final
	: public dynamic_reference_nocheck<ModeHandler>
{
public:
	ChanModeReference(Module* mod, const std::string& modename)
		: dynamic_reference_nocheck<ModeHandler>(mod, "mode/" + modename) {}
};

class UserModeReference final
	: public dynamic_reference_nocheck<ModeHandler>
{
public:
	UserModeReference(Module* mod, const std::string& modename)
		: dynamic_reference_nocheck<ModeHandler>(mod, "umode/" + modename) {}
};
