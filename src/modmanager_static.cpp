/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


#define MODNAME "cmd_all"

#include "inspircd.h"
#include "exitcodes.h"
#include <iostream>

#ifdef INSPIRCD_STATIC

typedef std::map<std::string, AllModuleList*> modmap;
static std::vector<AllCommandList::fn>* cmdlist = NULL;
static modmap* modlist = NULL;

AllCommandList::AllCommandList(fn cmd)
{
	if (!cmdlist)
		cmdlist = new std::vector<AllCommandList::fn>();
	cmdlist->push_back(cmd);
}

AllModuleList::AllModuleList(AllModuleList::fn mod, const std::string& Name) : init(mod), name(Name)
{
	if (!modlist)
		modlist = new modmap();
	modlist->insert(std::make_pair(Name, this));
}

class AllModule : public Module
{
	std::vector<Command*> cmds;
 public:
	AllModule()
	{
		if (!cmdlist)
			return;
		try
		{
			cmds.reserve(cmdlist->size());
			for(std::vector<AllCommandList::fn>::iterator i = cmdlist->begin(); i != cmdlist->end(); ++i)
			{
				Command* c = (*i)(this);
				cmds.push_back(c);
			}
		}
		catch (...)
		{
			this->AllModule::~AllModule();
			throw;
		}
	}

	~AllModule()
	{
		stdalgo::delete_all(cmds);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("All commands", VF_VENDOR|VF_CORE);
	}
};

MODULE_INIT(AllModule)

bool ModuleManager::Load(const std::string& inputname, bool defer)
{
	const std::string name = ExpandModName(inputname);
	modmap::iterator it = modlist->find(name);
	if (it == modlist->end())
		return false;
	Module* mod = NULL;

	ServiceList newservices;
	if (!defer)
		this->NewServices = &newservices;

	try
	{
		mod = (*it->second->init)();
		mod->ModuleSourceFile = name;
		mod->ModuleDLLManager = NULL;
		mod->dying = false;
		Modules[name] = mod;
		this->NewServices = NULL;
		if (defer)
		{
			ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, "New module introduced: %s", name.c_str());
			return true;
		}
		else
		{
			ConfigStatus confstatus;

			AttachAll(mod);
			AddServices(newservices);
			mod->init();
			mod->ReadConfig(confstatus);
		}
	}
	catch (CoreException& modexcept)
	{
		this->NewServices = NULL;

		if (mod)
			DoSafeUnload(mod);
		ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, "Unable to load " + name + ": " + modexcept.GetReason());
		return false;
	}

	FOREACH_MOD(OnLoadModule, (mod));
	PrioritizeHooks();
	ServerInstance->ISupport.Build();
	return true;
}

void ModuleManager::LoadCoreModules(std::map<std::string, ServiceList>& servicemap)
{
	for (modmap::const_iterator i = modlist->begin(); i != modlist->end(); ++i)
	{
		const std::string modname = i->first;
		if (modname[0] == 'c')
		{
			this->NewServices = &servicemap[modname];
			Load(modname, true);
		}
	}
	this->NewServices = NULL;
}

#endif
