/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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


#include "inspircd.h"
#include "listmode.h"
#include "modules/reload.h"

static Events::ModuleEventProvider* reloadevprov;

class CommandReloadmodule : public Command
{
	Events::ModuleEventProvider evprov;
 public:
	/** Constructor for reloadmodule.
	 */
	CommandReloadmodule(Module* parent)
		: Command(parent, "RELOADMODULE", 1)
		, evprov(parent, "event/reloadmodule")
	{
		reloadevprov = &evprov;
		flags_needed = 'o';
		syntax = "<modulename>";
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

namespace ReloadModule
{

class DataKeeper
{
	/** Data we save for each mode and extension provided by the module
	 */
	struct ProviderInfo
	{
		std::string itemname;
		union
		{
			ModeHandler* mh;
			ExtensionItem* extitem;
		};

		ProviderInfo(ModeHandler* mode)
			: itemname(mode->name)
			, mh(mode)
		{
		}

		ProviderInfo(ExtensionItem* ei)
			: itemname(ei->name)
			, extitem(ei)
		{
		}
	};

	struct InstanceData
	{
		/** Position of the ModeHandler or ExtensionItem that the serialized data belongs to
		 */
		size_t index;

		/** Serialized data
		 */
		std::string serialized;

		InstanceData(size_t Index, const std::string& Serialized)
			: index(Index)
			, serialized(Serialized)
		{
		}
	};

	struct ModesExts
	{
		/** Mode data for the object, one entry per mode set by the module being reloaded
		 */
		std::vector<InstanceData> modelist;

		/** Extensions for the object, one entry per extension set by the module being reloaded
		 */
		std::vector<InstanceData> extlist;

		bool empty() const { return ((modelist.empty()) && (extlist.empty())); }

		void swap(ModesExts& other)
		{
			modelist.swap(other.modelist);
			extlist.swap(other.extlist);
		}
	};

	struct OwnedModesExts : public ModesExts
	{
		/** User uuid or channel name
		 */
		std::string owner;

		OwnedModesExts(const std::string& Owner)
			: owner(Owner)
		{
		}
	};

	// Data saved for each channel
	struct ChanData : public OwnedModesExts
	{
		/** Type of data stored for each member who has any affected modes or extensions set
		 */
		typedef OwnedModesExts MemberData;

		/** List of data (modes and extensions) about each member
		 */
		std::vector<MemberData> memberdatalist;

		ChanData(Channel* chan)
			: OwnedModesExts(chan->name)
		{
		}
	};

	// Data saved for each user
	typedef OwnedModesExts UserData;

	/** Module being reloaded
	 */
	Module* mod;

	/** Stores all user and channel modes provided by the module
	 */
	std::vector<ProviderInfo> handledmodes[2];

	/** Stores all extensions provided by the module
	 */
	std::vector<ProviderInfo> handledexts;

	/** Stores all of the module data related to users
	 */
	std::vector<UserData> userdatalist;

	/** Stores all of the module data related to channels and memberships
	 */
	std::vector<ChanData> chandatalist;

	/** Data attached by modules
	 */
	ReloadModule::CustomData moddata;

	void SaveExtensions(Extensible* extensible, std::vector<InstanceData>& extdatalist);
	void SaveMemberData(Channel* chan, std::vector<ChanData::MemberData>& memberdatalist);
	static void SaveListModes(Channel* chan, ListModeBase* lm, size_t index, ModesExts& currdata);

	void CreateModeList(ModeType modetype);
	void DoSaveUsers();
	void DoSaveChans();

	/** Link previously saved extension names to currently available ExtensionItems
	 */
	void LinkExtensions();

	/** Link previously saved mode names to currently available ModeHandlers
	 * @param modetype Type of the modes to look for
	 */
	void LinkModes(ModeType modetype);

	void DoRestoreUsers();
	void DoRestoreChans();
	void DoRestoreModules();

	/** Restore previously saved modes and extensions on an Extensible.
	 * The extensions are set directly on the extensible, the modes are added into the provided mode change list.
	 * @param data Data to unserialize from
	 * @param extensible Object to restore
	 * @param modetype MODETYPE_USER if the object being restored is a User, MODETYPE_CHANNEL otherwise
	 * (for Channels and Memberships).
	 * @param modechange Mode change to populate with the modes
	 */
	void RestoreObj(const OwnedModesExts& data, Extensible* extensible, ModeType modetype, Modes::ChangeList& modechange);

	/** Restore all previously saved extensions on an Extensible
	 * @param list List of extensions and their serialized data to restore
	 * @param extensible Target Extensible
	 */
	void RestoreExtensions(const std::vector<InstanceData>& list, Extensible* extensible);

	/** Restore all previously saved modes on a User, Channel or Membership
	 * @param list List of modes to restore
	 * @param modetype MODETYPE_USER if the object being restored is a User, MODETYPE_CHANNEL otherwise
	 * @param modechange Mode change to populate with the modes
	 */
	void RestoreModes(const std::vector<InstanceData>& list, ModeType modetype, Modes::ChangeList& modechange);

	/** Restore all modes and extensions of all members on a channel
	 * @param chan Channel whose members are being restored
	 * @param memberdata Data to restore
	 * @param modechange Mode change to populate with prefix modes
	 */
	void RestoreMemberData(Channel* chan, const std::vector<ChanData::MemberData>& memberdatalist, Modes::ChangeList& modechange);

	/** Verify that a service which had its data saved is available and owned by the module that owned it previously
	 * @param service Service descriptor
	 * @param type Human-readable type of the service for log messages
	 */
	void VerifyServiceProvider(const ProviderInfo& service, const char* type);

 public:
	/** Save module state
	 * @param currmod Module whose data to save
	 */
	void Save(Module* currmod);

	/** Restore module state
	 * @param newmod Newly loaded instance of the module which had its data saved
	 */
	void Restore(Module* newmod);

	/** Handle reload failure
	 */
	void Fail();
};

void DataKeeper::DoSaveUsers()
{
	ModesExts currdata;

	const user_hash& users = ServerInstance->Users->GetUsers();
	for (user_hash::const_iterator i = users.begin(); i != users.end(); ++i)
	{
		User* const user = i->second;

		// Serialize user modes
		for (size_t j = 0; j < handledmodes[MODETYPE_USER].size(); j++)
		{
			ModeHandler* mh = handledmodes[MODETYPE_USER][j].mh;
			if (user->IsModeSet(mh))
				currdata.modelist.push_back(InstanceData(j, mh->GetUserParameter(user)));
		}

		// Serialize all extensions attached to the User
		SaveExtensions(user, currdata.extlist);

		// Add to list if the user has any modes or extensions set that we are interested in, otherwise we don't
		// have to do anything with this user when restoring
		if (!currdata.empty())
		{
			userdatalist.push_back(UserData(user->uuid));
			userdatalist.back().swap(currdata);
		}
	}
}

void DataKeeper::SaveExtensions(Extensible* extensible, std::vector<InstanceData>& extdata)
{
	const Extensible::ExtensibleStore& setexts = extensible->GetExtList();

	// Position of the extension saved in the handledexts list
	size_t index = 0;
	for (std::vector<ProviderInfo>::const_iterator i = handledexts.begin(); i != handledexts.end(); ++i, index++)
	{
		ExtensionItem* const item = i->extitem;
		Extensible::ExtensibleStore::const_iterator it = setexts.find(item);
		if (it == setexts.end())
			continue;

		std::string value = item->serialize(FORMAT_INTERNAL, extensible, it->second);
		// If the serialized value is empty the extension won't be saved and restored
		if (!value.empty())
			extdata.push_back(InstanceData(index, value));
	}
}

void DataKeeper::SaveListModes(Channel* chan, ListModeBase* lm, size_t index, ModesExts& currdata)
{
	const ListModeBase::ModeList* list = lm->GetList(chan);
	if (!list)
		return;

	for (ListModeBase::ModeList::const_iterator i = list->begin(); i != list->end(); ++i)
	{
		const ListModeBase::ListItem& listitem = *i;
		currdata.modelist.push_back(InstanceData(index, listitem.mask));
	}
}

void DataKeeper::DoSaveChans()
{
	ModesExts currdata;
	std::vector<OwnedModesExts> currmemberdata;

	const chan_hash& chans = ServerInstance->GetChans();
	for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); ++i)
	{
		Channel* const chan = i->second;

		// Serialize channel modes
		for (size_t j = 0; j < handledmodes[MODETYPE_CHANNEL].size(); j++)
		{
			ModeHandler* mh = handledmodes[MODETYPE_CHANNEL][j].mh;
			ListModeBase* lm = mh->IsListModeBase();
			if (lm)
				SaveListModes(chan, lm, j, currdata);
			else if (chan->IsModeSet(mh))
				currdata.modelist.push_back(InstanceData(j, chan->GetModeParameter(mh)));
		}

		// Serialize all extensions attached to the Channel
		SaveExtensions(chan, currdata.extlist);

		// Serialize all extensions attached to and all modes set on all members of the channel
		SaveMemberData(chan, currmemberdata);

		// Same logic as in DoSaveUsers() plus we consider the modes and extensions of all members
		if ((!currdata.empty()) || (!currmemberdata.empty()))
		{
			chandatalist.push_back(ChanData(chan));
			chandatalist.back().swap(currdata);
			chandatalist.back().memberdatalist.swap(currmemberdata);
		}
	}
}

void DataKeeper::SaveMemberData(Channel* chan, std::vector<OwnedModesExts>& memberdatalist)
{
	ModesExts currdata;
	const Channel::MemberMap& users = chan->GetUsers();
	for (Channel::MemberMap::const_iterator i = users.begin(); i != users.end(); ++i)
	{
		Membership* const memb = i->second;

		for (size_t j = 0; j < handledmodes[MODETYPE_CHANNEL].size(); j++)
		{
			ModeHandler* mh = handledmodes[MODETYPE_CHANNEL][j].mh;
			const PrefixMode* const pm = mh->IsPrefixMode();
			if ((pm) && (memb->HasMode(pm)))
				currdata.modelist.push_back(InstanceData(j, memb->user->uuid)); // Need to pass the user's uuid to the mode parser to set the mode later
		}

		SaveExtensions(memb, currdata.extlist);

		// Same logic as in DoSaveUsers()
		if (!currdata.empty())
		{
			memberdatalist.push_back(OwnedModesExts(memb->user->uuid));
			memberdatalist.back().swap(currdata);
		}
	}
}

void DataKeeper::RestoreMemberData(Channel* chan, const std::vector<ChanData::MemberData>& memberdatalist, Modes::ChangeList& modechange)
{
	for (std::vector<ChanData::MemberData>::const_iterator i = memberdatalist.begin(); i != memberdatalist.end(); ++i)
	{
		const ChanData::MemberData& md = *i;
		User* const user = ServerInstance->FindUUID(md.owner);
		if (!user)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "User %s is gone (while processing %s)", md.owner.c_str(), chan->name.c_str());
			continue;
		}

		Membership* const memb = chan->GetUser(user);
		if (!memb)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Member %s is no longer on channel %s", md.owner.c_str(), chan->name.c_str());
			continue;
		}

		RestoreObj(md, memb, MODETYPE_CHANNEL, modechange);
	}
}

void DataKeeper::CreateModeList(ModeType modetype)
{
	const ModeParser::ModeHandlerMap& modes = ServerInstance->Modes->GetModes(modetype);
	for (ModeParser::ModeHandlerMap::const_iterator i = modes.begin(); i != modes.end(); ++i)
	{
		ModeHandler* mh = i->second;
		if (mh->creator == mod)
			handledmodes[modetype].push_back(ProviderInfo(mh));
	}
}

void DataKeeper::Save(Module* currmod)
{
	this->mod = currmod;

	const ExtensionManager::ExtMap& allexts = ServerInstance->Extensions.GetExts();
	for (ExtensionManager::ExtMap::const_iterator i = allexts.begin(); i != allexts.end(); ++i)
	{
		ExtensionItem* ext = i->second;
		if (ext->creator == mod)
			handledexts.push_back(ProviderInfo(ext));
	}

	CreateModeList(MODETYPE_USER);
	DoSaveUsers();

	CreateModeList(MODETYPE_CHANNEL);
	DoSaveChans();

	FOREACH_MOD_CUSTOM(*reloadevprov, ReloadModule::EventListener, OnReloadModuleSave, (mod, this->moddata));

	ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Saved data about %lu users %lu chans %lu modules", (unsigned long)userdatalist.size(), (unsigned long)chandatalist.size(), (unsigned long)moddata.list.size());
}

void DataKeeper::VerifyServiceProvider(const ProviderInfo& service, const char* type)
{
	const ServiceProvider* sp = service.extitem;
	if (!sp)
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "%s \"%s\" is no longer available", type, service.itemname.c_str());
	else if (sp->creator != mod)
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "%s \"%s\" is now handled by %s", type, service.itemname.c_str(), (sp->creator ? sp->creator->ModuleSourceFile.c_str() : "<core>"));
}

void DataKeeper::LinkModes(ModeType modetype)
{
	std::vector<ProviderInfo>& list = handledmodes[modetype];
	for (std::vector<ProviderInfo>::iterator i = list.begin(); i != list.end(); ++i)
	{
		ProviderInfo& item = *i;
		item.mh = ServerInstance->Modes->FindMode(item.itemname, modetype);
		VerifyServiceProvider(item, (modetype == MODETYPE_USER ? "User mode" : "Channel mode"));
	}
}

void DataKeeper::LinkExtensions()
{
	for (std::vector<ProviderInfo>::iterator i = handledexts.begin(); i != handledexts.end(); ++i)
	{
		ProviderInfo& item = *i;
		item.extitem = ServerInstance->Extensions.GetItem(item.itemname);
		VerifyServiceProvider(item.extitem, "Extension");
	}
}

void DataKeeper::Restore(Module* newmod)
{
	this->mod = newmod;

	// Find the new extension items
	LinkExtensions();
	LinkModes(MODETYPE_USER);
	LinkModes(MODETYPE_CHANNEL);

	// Restore
	DoRestoreUsers();
	DoRestoreChans();
	DoRestoreModules();

	ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Restore finished");
}

void DataKeeper::Fail()
{
	this->mod = NULL;

	ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Restore failed, notifying modules");
	DoRestoreModules();
}

void DataKeeper::RestoreObj(const OwnedModesExts& data, Extensible* extensible, ModeType modetype, Modes::ChangeList& modechange)
{
	RestoreExtensions(data.extlist, extensible);
	RestoreModes(data.modelist, modetype, modechange);
}

void DataKeeper::RestoreExtensions(const std::vector<InstanceData>& list, Extensible* extensible)
{
	for (std::vector<InstanceData>::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		const InstanceData& id = *i;
		handledexts[id.index].extitem->unserialize(FORMAT_INTERNAL, extensible, id.serialized);
	}
}

void DataKeeper::RestoreModes(const std::vector<InstanceData>& list, ModeType modetype, Modes::ChangeList& modechange)
{
	for (std::vector<InstanceData>::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		const InstanceData& id = *i;
		modechange.push_add(handledmodes[modetype][id.index].mh, id.serialized);
	}
}

void DataKeeper::DoRestoreUsers()
{
	ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Restoring user data");
	Modes::ChangeList modechange;

	for (std::vector<UserData>::const_iterator i = userdatalist.begin(); i != userdatalist.end(); ++i)
	{
		const UserData& userdata = *i;
		User* const user = ServerInstance->FindUUID(userdata.owner);
		if (!user)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "User %s is gone", userdata.owner.c_str());
			continue;
		}

		RestoreObj(userdata, user, MODETYPE_USER, modechange);
		ServerInstance->Modes.Process(ServerInstance->FakeClient, NULL, user, modechange, ModeParser::MODE_LOCALONLY);
		modechange.clear();
	}
}

void DataKeeper::DoRestoreChans()
{
	ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Restoring channel data");
	Modes::ChangeList modechange;

	for (std::vector<ChanData>::const_iterator i = chandatalist.begin(); i != chandatalist.end(); ++i)
	{
		const ChanData& chandata = *i;
		Channel* const chan = ServerInstance->FindChan(chandata.owner);
		if (!chan)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Channel %s not found", chandata.owner.c_str());
			continue;
		}

		RestoreObj(chandata, chan, MODETYPE_CHANNEL, modechange);
		// Process the mode change before applying any prefix modes
		ServerInstance->Modes.Process(ServerInstance->FakeClient, chan, NULL, modechange, ModeParser::MODE_LOCALONLY);
		modechange.clear();

		// Restore all member data
		RestoreMemberData(chan, chandata.memberdatalist, modechange);
		ServerInstance->Modes.Process(ServerInstance->FakeClient, chan, NULL, modechange, ModeParser::MODE_LOCALONLY);
		modechange.clear();
	}
}

void DataKeeper::DoRestoreModules()
{
	for (ReloadModule::CustomData::List::iterator i = moddata.list.begin(); i != moddata.list.end(); ++i)
	{
		ReloadModule::CustomData::Data& data = *i;
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Calling module data handler %p", (void*)data.handler);
		data.handler->OnReloadModuleRestore(mod, data.data);
	}
}

} // namespace ReloadModule

class ReloadAction : public HandlerBase0<void>
{
	Module* const mod;
	const std::string uuid;
	const std::string passedname;

 public:
	ReloadAction(Module* m, const std::string& uid, const std::string& passedmodname)
		: mod(m)
		, uuid(uid)
		, passedname(passedmodname)
	{
	}

	void Call()
	{
		ReloadModule::DataKeeper datakeeper;
		datakeeper.Save(mod);

		DLLManager* dll = mod->ModuleDLLManager;
		std::string name = mod->ModuleSourceFile;
		ServerInstance->Modules->DoSafeUnload(mod);
		ServerInstance->GlobalCulls.Apply();
		delete dll;
		bool result = ServerInstance->Modules->Load(name);

		if (result)
		{
			Module* newmod = ServerInstance->Modules->Find(name);
			datakeeper.Restore(newmod);
		}
		else
			datakeeper.Fail();

		ServerInstance->SNO->WriteGlobalSno('a', "RELOAD MODULE: %s %ssuccessfully reloaded", passedname.c_str(), result ? "" : "un");
		User* user = ServerInstance->FindUUID(uuid);
		if (user)
			user->WriteNumeric(RPL_LOADEDMODULE, passedname, InspIRCd::Format("Module %ssuccessfully reloaded.", (result ? "" : "un")));

		ServerInstance->GlobalCulls.AddItem(this);
	}
};

CmdResult CommandReloadmodule::Handle (const std::vector<std::string>& parameters, User *user)
{
	Module* m = ServerInstance->Modules->Find(parameters[0]);
	if (m == creator)
	{
		user->WriteNumeric(RPL_LOADEDMODULE, parameters[0], "You cannot reload core_reloadmodule.so (unload and load it)");
		return CMD_FAILURE;
	}

	if (creator->dying)
		return CMD_FAILURE;

	if ((m) && (ServerInstance->Modules.CanUnload(m)))
	{
		ServerInstance->AtomicActions.AddAction(new ReloadAction(m, user->uuid, parameters[0]));
		return CMD_SUCCESS;
	}
	else
	{
		user->WriteNumeric(RPL_LOADEDMODULE, parameters[0], "Could not find module by that name");
		return CMD_FAILURE;
	}
}

COMMAND_INIT(CommandReloadmodule)
