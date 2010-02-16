#include "inspircd.h"

#ifdef PURE_STATIC

static std::vector<AllCommandList::fn>* cmdlist = NULL;
static std::vector<AllModuleList*>* modlist = NULL;

AllCommandList::AllCommandList(fn cmd)
{
	if (!cmdlist)
		cmdlist = new std::vector<AllCommandList::fn>();
	cmdlist->push_back(cmd);
}

AllModuleList::AllModuleList(AllModuleList::fn mod, const std::string& Name) : init(mod), name(Name)
{
	if (!modlist)
		modlist = new std::vector<AllModuleList*>();
	modlist->push_back(this);
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
				ServerInstance->AddCommand(c);
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
		for(std::vector<Command*>::iterator i = cmds.begin(); i != cmds.end(); ++i)
			delete *i;
	}

	Version GetVersion()
	{
		return Version("All commands", VF_VENDOR|VF_CORE);
	}
};

MODULE_INIT(AllModule)

bool ModuleManager::Load(const std::string& name, bool)
{
	for(std::vector<AllModuleList*>::iterator i = modlist->begin(); i != modlist->end(); ++i)
	{
		if ((**i).name == name)
		{
			Module* c = NULL;
			try
			{
				c = (*(**i).init)();
				Modules[name] = c;
				c->init();
				FOREACH_MOD(I_OnLoadModule,OnLoadModule(c));
				return true;
			}
			catch (CoreException& modexcept)
			{
				if (c)
					DoSafeUnload(c);
				delete c;
				ServerInstance->Logs->Log("MODULE", DEFAULT, "Unable to load " + (**i).name + ": " + modexcept.GetReason());
			}
		}
	}
	return false;
}

namespace {
	struct UnloadAction : public HandlerBase0<void>
	{
		Module* const mod;
		UnloadAction(Module* m) : mod(m) {}
		void Call()
		{
			ServerInstance->Modules->DoSafeUnload(mod);
			ServerInstance->GlobalCulls.Apply();
			ServerInstance->GlobalCulls.AddItem(this);
		}
	};

	struct ReloadAction : public HandlerBase0<void>
	{
		Module* const mod;
		HandlerBase1<void, bool>* const callback;
		ReloadAction(Module* m, HandlerBase1<void, bool>* c)
			: mod(m), callback(c) {}
		void Call()
		{
			std::string name = mod->ModuleSourceFile;
			ServerInstance->Modules->DoSafeUnload(mod);
			ServerInstance->GlobalCulls.Apply();
			bool rv = ServerInstance->Modules->Load(name.c_str());
			callback->Call(rv);
			ServerInstance->GlobalCulls.AddItem(this);
		}
	};
}

bool ModuleManager::Unload(Module* mod)
{
	if (!CanUnload(mod))
		return false;
	ServerInstance->AtomicActions.AddAction(new UnloadAction(mod));
	return true;
}

void ModuleManager::Reload(Module* mod, HandlerBase1<void, bool>* callback)
{
	if (CanUnload(mod))
		ServerInstance->AtomicActions.AddAction(new ReloadAction(mod, callback));
	else
		callback->Call(false);
}

void ModuleManager::LoadAll()
{
	ModCount = 0;
	for(std::vector<AllModuleList*>::iterator i = modlist->begin(); i != modlist->end(); ++i)
	{
		Module* c = NULL;
		try
		{
			c = (*(**i).init)();
			c->ModuleSourceFile = (**i).name;
			c->ModuleDLLManager = NULL;
			Modules[(**i).name] = c;
			c->init();
		}
		catch (CoreException& modexcept)
		{
			if (c)
				DoSafeUnload(c);
			delete c;
			ServerInstance->Logs->Log("MODULE", DEFAULT, "Unable to load " + (**i).name + ": " + modexcept.GetReason());
		}
	}

	/* We give every module a chance to re-prioritize when we introduce a new one,
	 * not just the one thats loading, as the new module could affect the preference
	 * of others
	 */
	for(int tries = 0; tries < 20; tries++)
	{
		prioritizationState = tries > 0 ? PRIO_STATE_LAST : PRIO_STATE_FIRST;
		for (std::map<std::string, Module*>::iterator n = Modules.begin(); n != Modules.end(); ++n)
			n->second->Prioritize();

		if (prioritizationState == PRIO_STATE_LAST)
			break;
		if (tries == 19)
			ServerInstance->Logs->Log("MODULE", DEFAULT, "Hook priority dependency loop detected");
	}

	ServerInstance->BuildISupport();
}

void ModuleManager::UnloadAll()
{
}

#endif
