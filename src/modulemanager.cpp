/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2015, 2019-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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


#include <filesystem>

#include <fmt/color.h>

#include "inspircd.h"
#include "dynamic.h"
#include "exitcodes.h"

bool ModuleManager::Load(const std::string& modname, bool defer)
{
	/* Don't allow people to specify paths for modules, it doesn't work as expected */
	if (modname.find_first_of("\\/") != std::string::npos)
	{
		LastModuleError = "You can't load modules with a path: " + modname;
		return false;
	}

	const std::string filename = ExpandModName(modname);
	const std::string moduleFile = ServerInstance->Config->Paths.PrependModule(filename);

	std::error_code ec;
	if (!std::filesystem::is_regular_file(moduleFile, ec))
	{
		LastModuleError = "Module file could not be found: " + filename;
		ServerInstance->Logs.Error("MODULE", LastModuleError);
		return false;
	}

	if (Modules.find(filename) != Modules.end())
	{
		LastModuleError = "Module " + filename + " is already loaded, cannot load a module twice!";
		ServerInstance->Logs.Error("MODULE", LastModuleError);
		return false;
	}

	Module* newmod = nullptr;
	auto* newhandle = new DLLManager(moduleFile);
	ServiceList newservices;
	if (!defer)
		this->NewServices = &newservices;

	try
	{
		newmod = newhandle->CallInit();
		this->NewServices = nullptr;

		if (newmod)
		{
			newmod->ModuleSourceFile = filename;
			newmod->ModuleDLLManager = newhandle;
			Modules[filename] = newmod;

			if (!defer)
			{
				AttachAll(newmod);
				AddServices(newservices);

				ConfigStatus confstatus;
				newmod->init();
				newmod->ReadConfig(confstatus);
			}

			const char* version = newhandle->GetVersion();
			if (!version)
				version = "unknown";

			ServerInstance->Logs.Normal("MODULE", "New module introduced: {} (version {}, properties {})",
				filename, version, newmod->GetPropertyString());
		}
		else
		{
			LastModuleError = "Unable to load " + filename + ": " + newhandle->LastError();
			ServerInstance->Logs.Error("MODULE", LastModuleError);
			delete newhandle;
			return false;
		}
	}
	catch (const CoreException& modexcept)
	{
		this->NewServices = nullptr;

		// failure in module constructor
		if (newmod)
		{
			DoSafeUnload(newmod);
			ServerInstance->GlobalCulls.AddItem(newhandle);
		}
		else
			delete newhandle;
		LastModuleError = "Unable to load " + filename + ": " + modexcept.GetReason();
		ServerInstance->Logs.Error("MODULE", LastModuleError);
		return false;
	}

	if (defer)
		return true;

	FOREACH_MOD(OnLoadModule, (newmod));
	PrioritizeHooks();
	return true;
}

/* We must load the modules AFTER initializing the socket engine, now */
void ModuleManager::LoadCoreModules(std::map<std::string, ServiceList>& servicemap)
{
	fmt::print("Loading core modules ");
	fflush(stdout);

	try
	{
		for (const auto& entry : std::filesystem::directory_iterator(ServerInstance->Config->Paths.Module))
		{
			if (!entry.is_regular_file())
				continue;

			const std::string name = entry.path().filename().string();
			if (!InspIRCd::Match(name, "core_*" DLL_EXTENSION))
				continue;

			fmt::print(".");
			fflush(stdout);
			this->NewServices = &servicemap[name];

			if (!Load(name, true))
			{
				fmt::println("");
				fmt::println("[{}] {}", fmt::styled("*", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)), LastError());
				fmt::println("");
				ServerInstance->Exit(EXIT_STATUS_MODULE);
			}
		}
	}
	catch (const std::filesystem::filesystem_error& err)
	{
		fmt::println("failed: {}", err.what());
		ServerInstance->Exit(EXIT_STATUS_MODULE);
	}

	fmt::println("");
}
