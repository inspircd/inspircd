/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

class CoreExport dynamic_reference_base : public interfacebase, public insp::intrusive_list_node<dynamic_reference_base>
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
	CaptureHook* hook;
	void resolve();
 protected:
	ServiceProvider* value;
 public:
	ModuleRef creator;
	dynamic_reference_base(Module* Creator, const std::string& Name);
	~dynamic_reference_base();
	inline const std::string& GetProvider() { return name; }
	void SetProvider(const std::string& newname);

	/** Set handler to call when the target object becomes available
	 * @param h Handler to call
	 */
	void SetCaptureHook(CaptureHook* h) { hook = h; }

	void check();
	operator bool() { return (value != NULL); }
	static void reset_all();
};

inline void dynamic_reference_base::check()
{
	if (!value)
		throw ModuleException("Dynamic reference to '" + name + "' failed to resolve");
}

template<typename T>
class dynamic_reference : public dynamic_reference_base
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
class dynamic_reference_nocheck : public dynamic_reference_base
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
class ChanModeReference : public dynamic_reference_nocheck<ModeHandler>
{
 public:
	ChanModeReference(Module* mod, const std::string& modename)
		: dynamic_reference_nocheck<ModeHandler>(mod, "mode/" + modename) {}
};

class UserModeReference : public dynamic_reference_nocheck<ModeHandler>
{
 public:
	UserModeReference(Module* mod, const std::string& modename)
		: dynamic_reference_nocheck<ModeHandler>(mod, "umode/" + modename) {}
};
