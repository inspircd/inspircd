/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
		for(unsigned int i=0; i < allmods.size(); ++i)
		{
			Module* mod = ServerInstance->Modules->Find(allmods[i]);
			void* fptr = read(mod);
			for(EventHandlerIter j = ServerInstance->Modules->EventHandlers[impl].begin();
				j != ServerInstance->Modules->EventHandlers[impl].end(); j++)
			{
				if (mod == *j)
				{
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
	}
};

template<typename T> vtbase* vtinit(T t)
{
	return new vtable<T>(t);
}

static void checkall(Module* noimpl)
{
	std::vector<std::string> allmods = ServerInstance->Modules->GetAllModuleNames(0);
#define CHK(name) do { \
	vtbase* vt = vtinit(&Module::name); \
	vt->isok(#name, I_ ## name, noimpl, allmods); \
	delete vt; \
} while (0)
	CHK(OnUserConnect);
	CHK(OnUserQuit);
	CHK(OnUserDisconnect);
	CHK(OnUserJoin);
	CHK(OnUserPart);
	CHK(OnRehash);
	CHK(OnSendSnotice);
	CHK(OnUserPreJoin);
	CHK(OnUserPreKick);
	CHK(OnUserKick);
	CHK(OnOper);
	CHK(OnInfo);
	CHK(OnWhois);
	CHK(OnUserPreInvite);
	CHK(OnUserInvite);
	CHK(OnUserPreMessage);
	CHK(OnUserPreNotice);
	CHK(OnUserPreNick);
	CHK(OnUserMessage);
	CHK(OnUserNotice);
	CHK(OnMode);
	CHK(OnGetServerDescription);
	CHK(OnSyncUser);
	CHK(OnSyncChannel);
	CHK(OnDecodeMetaData);
	CHK(OnWallops);
	CHK(OnAcceptConnection);
	CHK(OnChangeHost);
	CHK(OnChangeName);
	CHK(OnAddLine);
	CHK(OnDelLine);
	CHK(OnExpireLine);
	CHK(OnUserPostNick);
	CHK(OnPreMode);
	CHK(On005Numeric);
	CHK(OnKill);
	CHK(OnRemoteKill);
	CHK(OnLoadModule);
	CHK(OnUnloadModule);
	CHK(OnBackgroundTimer);
	CHK(OnPreCommand);
	CHK(OnCheckReady);
	CHK(OnCheckInvite);
	CHK(OnRawMode);
	CHK(OnCheckKey);
	CHK(OnCheckLimit);
	CHK(OnCheckBan);
	CHK(OnCheckChannelBan);
	CHK(OnExtBanCheck);
	CHK(OnStats);
	CHK(OnChangeLocalUserHost);
	CHK(OnPreTopicChange);
	CHK(OnPostTopicChange);
	CHK(OnEvent);
	CHK(OnGlobalOper);
	CHK(OnPostConnect);
	CHK(OnAddBan);
	CHK(OnDelBan);
	CHK(OnChangeLocalUserGECOS);
	CHK(OnUserRegister);
	CHK(OnChannelPreDelete);
	CHK(OnChannelDelete);
	CHK(OnPostOper);
	CHK(OnSyncNetwork);
	CHK(OnSetAway);
	CHK(OnPostCommand);
	CHK(OnPostJoin);
	CHK(OnWhoisLine);
	CHK(OnBuildNeighborList);
	CHK(OnGarbageCollect);
	CHK(OnText);
	CHK(OnPassCompare);
	CHK(OnRunTestSuite);
	CHK(OnNamesListItem);
	CHK(OnNumeric);
	CHK(OnHookIO);
	CHK(OnPreRehash);
	CHK(OnModuleRehash);
	CHK(OnSendWhoLine);
	CHK(OnChangeIdent);
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
	ModuleTest() : cmd(this)
	{
	}

	void init()
	{
		if (!strstr(ServerInstance->Config->ServerName.c_str(), ".test"))
			throw ModuleException("Don't load modules without reading their descriptions!");
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion()
	{
		return Version("Provides a module for testing the server while linked in a network", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleTest)

