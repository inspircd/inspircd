/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022, 2025 Sadie Powell <sadie@witchery.services>
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

/** The base class for InspIRCd exception types. */
class CoreException
	: public std::exception
{
private:
 	/** The reason this exception was thrown. */
	const std::string reason;

public:
	/** Creates a new instance of the CoreException class with the specified reason.
	 * @param message A message that contains the reason this exception was thrown.
	 */
	CoreException(const std::string& message)
		: reason(message)
	{
	}

	/** Retrieves a character array that contains the reason this exception was thrown. */
	const char* what() const noexcept override { return reason.c_str(); }

	/** Retrieves a string that contains the reason this exception was thrown. */
	const std::string& GetReason() const noexcept { return reason; }
};

class Module;

/** An generic exception which was thrown by a module. */
class CoreExport ModuleException
	: public CoreException
{
private:
	/* The module which threw this exception. */
	const Module* module;

public:

	/** Creates a new instance of the ModuleException class with the specified module instance and reason.
	 * @param mod The module which threw this exception.
	 * @param format A format string for a message that contains the reason this exception was thrown.
	 * @param args The arguments to format the message.
	 */
	template <typename... Args>
	ModuleException(const Module* mod, const char* format, Args&&... args)
		: ModuleException(mod, FMT::vformat(format, FMT::make_format_args(args...)))
	{
	}

	/** Creates a new instance of the ModuleException class with the specified module instance and reason.
	 * @param mod The module which threw this exception.
	 * @param message A message that contains the reason this exception was thrown.
	 */
	ModuleException(const Module* mod, const std::string& message)
		: CoreException(message)
		, module(mod)
	{
	}

	/** Retrieves the module which threw this exception. */
	const Module* GetModule() const noexcept { return module; }
};
