/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides a module for testing the server while linked in a network */

#include "inspircd.h"

struct vtbase
{
	virtual void isok(const char* name, int impl, Module* basemod, std::vector<std::string>& allmods) = 0;
	virtual ~vtbase() {}
};

template<typename T> struct vtable : public vtbase
{
	union u {
		T function;
		struct v {
			size_t delta;
			size_t vtoff;
		} v;
	} u;
	vtable(T t) {
		u.function = t;
	}
	/** member function pointer dereference from vtable; depends on the GCC 4.4 ABI (x86_64) */
	template<typename E> void* read(E* obj)
	{
		if (u.v.delta & 1)
		{
			uint8_t* optr = reinterpret_cast<uint8_t*>(obj);
			optr += u.v.vtoff;
			uint8_t* vptr = *reinterpret_cast<uint8_t**>(optr);
			vptr += u.v.delta - 1;
			return *reinterpret_cast<void**>(vptr);
		}
		else
			return reinterpret_cast<void*>(u.v.delta);
	}
	void isok(const char* name, int impl, Module* basemod, std::vector<std::string>& allmods)
	{
		void* base = read(basemod);
		int users = 0;
		std::string modusers;
		for(unsigned int i=0; i < allmods.size(); ++i)
		{
			Module* mod = ServerInstance->Modules->Find(allmods[i]);
			void* fptr = read(mod);
			for(EventHandlerIter j = ServerInstance->Modules->EventHandlers[impl].begin();
				j != ServerInstance->Modules->EventHandlers[impl].end(); j++)
			{
				if (mod == *j)
				{
					users++;
					modusers.push_back(' ');
					modusers.append(mod->ModuleSourceFile);
					if (fptr == base)
					{
						ServerInstance->SNO->WriteToSnoMask('a', "Module %s implements %s but uses default function",
							mod->ModuleSourceFile.c_str(), name);
					}
					goto done;
				}
			}
			if (fptr != base)
			{
				ServerInstance->SNO->WriteToSnoMask('a', "Module %s does not implement %s but overrides function",
					mod->ModuleSourceFile.c_str(), name);
			}
			done:;
		}
		ServerInstance->SNO->WriteToSnoMask('a', "Hook I_%s used by %d modules:%s", name, users, modusers.c_str());
	}
};

template<typename T> vtbase* vtinit(T t)
{
	return new vtable<T>(t);
}

static void checkall(Module* noimpl)
{
	std::vector<std::string> allmods = ServerInstance->Modules->GetAllModuleNames(0);
	int seen = 0;
#define CHK(name) do { \
	seen++; \
	vtbase* vt = vtinit(&Module::name); \
	vt->isok(#name, I_ ## name, noimpl, allmods); \
	delete vt; \
} while (0)
	CHK(On005Numeric);
	CHK(OnAcceptConnection);
	CHK(OnAddLine);
	CHK(OnBackgroundTimer);
	CHK(OnBuildNeighborList);
	CHK(OnChangeHost);
	CHK(OnChangeIdent);
	CHK(OnChangeName);
	CHK(OnChannelDelete);
	CHK(OnChannelPreDelete);
	CHK(OnCheckBan);
	CHK(OnCheckChannelBan);
	CHK(OnCheckJoin);
	CHK(OnCheckReady);
	CHK(OnDecodeMetaData);
	CHK(OnDelLine);
	CHK(OnEvent);
	CHK(OnExpireLine);
	CHK(OnExtBanCheck);
	CHK(OnGarbageCollect);
	CHK(OnGetServerDescription);
	CHK(OnInfo);
	CHK(OnKill);
	CHK(OnLoadModule);
	CHK(OnMode);
	CHK(OnModuleRehash);
	CHK(OnNamesListItem);
	CHK(OnOper);
	CHK(OnPassCompare);
	CHK(OnPermissionCheck);
	CHK(OnPostCommand);
	CHK(OnPostConnect);
	CHK(OnPostJoin);
	CHK(OnPostOper);
	CHK(OnPostTopicChange);
	CHK(OnPreCommand);
	CHK(OnPreMode);
	CHK(OnRawMode);
	CHK(OnRemoteKill);
	CHK(OnSendSnotice);
	CHK(OnSendWhoLine);
	CHK(OnSetAway);
	CHK(OnSetConnectClass);
	CHK(OnStats);
	CHK(OnSyncChannel);
	CHK(OnSyncNetwork);
	CHK(OnSyncUser);
	CHK(OnText);
	CHK(OnUnloadModule);
	CHK(OnUserConnect);
	CHK(OnUserDisconnect);
	CHK(OnUserInit);
	CHK(OnUserInvite);
	CHK(OnUserJoin);
	CHK(OnUserKick);
	CHK(OnUserMessage);
	CHK(OnUserNotice);
	CHK(OnUserPart);
	CHK(OnUserPostNick);
	CHK(OnUserPreMessage);
	CHK(OnUserPreNick);
	CHK(OnUserPreNotice);
	CHK(OnUserQuit);
	CHK(OnUserRegister);
	CHK(OnWallops);
	CHK(OnWhois);
	CHK(OnWhoisLine);
	if (seen != I_END - 1)
		ServerInstance->SNO->WriteToSnoMask('a', "Only checked %d/%d hooks", seen, I_END - 1);
}

class CommandTest : public Command
{
 public:
	CommandTest(Module* parent) : Command(parent, "TEST", 1)
	{
		syntax = "<action> <parameters>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		if (parameters[0] == "flood")
		{
			unsigned int count = parameters.size() > 1 ? atoi(parameters[1].c_str()) : 100;
			std::string line = parameters.size() > 2 ? parameters[2] : ":z.z NOTICE !flood :Flood text";
			for(unsigned int i=0; i < count; i++)
				user->Write(line);
		}
		else if (parameters[0] == "freeze" && IS_LOCAL(user) && parameters.size() > 1)
		{
			IS_LOCAL(user)->CommandFloodPenalty += atoi(parameters[1].c_str());
		}
		else if (parameters[0] == "sizes")
		{
// work around printf's lack of a size_t format specification
#define szl(x) static_cast<unsigned long>(sizeof(x))
			user->SendText(":z.z NOTICE !info :User=%lu/%lu/%lu Channel=%lu Membership=%lu ban=%lu",
				szl(LocalUser), szl(RemoteUser), szl(FakeUser), szl(Channel),
				szl(Membership), szl(BanItem));
		}
		else if (parameters[0] == "check")
		{
			checkall(creator);
			ServerInstance->SNO->WriteToSnoMask('a', "Module check complete");
		}
		return CMD_SUCCESS;
	}
};

class ModuleTest : public Module
{
	CommandTest cmd;
 public:
	ModuleTest() : cmd(this) {}

	void init()
	{
		if (!strstr(ServerInstance->Config->ServerName.c_str(), ".test"))
			throw ModuleException("Don't load modules without reading their descriptions!");
		ServerInstance->AddCommand(&cmd);
	}

	Version GetVersion()
	{
		return Version("Provides a module for testing the server while linked in a network", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleTest)

