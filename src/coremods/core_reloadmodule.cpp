/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
static ClientProtocol::Serializer* dummyserializer;

class DummySerializer final
	: public ClientProtocol::Serializer
{
	bool Parse(LocalUser* user, const std::string& line, ClientProtocol::ParseOutput& parseoutput) override
	{
		return false;
	}

	ClientProtocol::SerializedMessage Serialize(const ClientProtocol::Message& msg, const ClientProtocol::TagSelection& tagwl) const override
	{
		return {};
	}

public:
	DummySerializer(Module* mod)
		: ClientProtocol::Serializer(mod, "dummy")
	{
	}
};

class CommandReloadmodule final
	: public Command
{
	Events::ModuleEventProvider evprov;
	DummySerializer dummyser;

public:
	CommandReloadmodule(Module* parent)
		: Command(parent, "RELOADMODULE", 1)
		, evprov(parent, "event/reloadmodule")
		, dummyser(parent)
	{
		reloadevprov = &evprov;
		dummyserializer = &dummyser;
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<modulename>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override;
};

namespace ReloadModule
{

class DataKeeper final
{
	/** Data we save for each mode and extension provided by the module
	 */
	struct ProviderInfo final
	{
		std::string itemname;
		union
		{
			ModeHandler* mh;
			ExtensionItem* extitem;
			ClientProtocol::Serializer* serializer;
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

		ProviderInfo(ClientProtocol::Serializer* ser)
			: itemname(ser->name)
			, serializer(ser)
		{
		}
	};

	struct InstanceData final
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

	struct OwnedModesExts
		: public ModesExts
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
	struct ChanData final
		: public OwnedModesExts
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
	struct UserData final
		: public OwnedModesExts
	{
		static constexpr size_t UNUSED_INDEX = SIZE_MAX;
		size_t serializerindex;

		UserData(User* user, size_t serializeridx)
			: OwnedModesExts(user->uuid)
			, serializerindex(serializeridx)
		{
		}
	};

	/** Module being reloaded
	 */
	Module* mod;

	/** Stores all user and channel modes provided by the module
	 */
	std::vector<ProviderInfo> handledmodes[2];

	/** Stores all extensions provided by the module
	 */
	std::vector<ProviderInfo> handledexts;

	/** Stores all serializers provided by the module
	 */
	std::vector<ProviderInfo> handledserializers;

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
	size_t SaveSerializer(User* user);

	/** Get the index of a ProviderInfo representing the serializer in the handledserializers list.
	 * If the serializer is not already in the list it is added.
	 * @param serializer Serializer to get an index to.
	 * @return Index of the ProviderInfo representing the serializer.
	 */
	size_t GetSerializerIndex(ClientProtocol::Serializer* serializer);

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

	/** Link previously saved serializer names to currently available Serializers
	 */
	void LinkSerializers();

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

	/** Restore previously saved serializer on a User.
	 * Quit the user if the serializer cannot be restored.
	 * @param serializerindex Saved serializer index to restore.
	 * @param user User whose serializer to restore. If not local then calling this method is a no-op.
	 * @return True if the serializer didn't need restoring or was restored successfully.
	 * False if the serializer should have been restored but the required serializer is unavailable and the user was quit.
	 */
	bool RestoreSerializer(size_t serializerindex, User* user);

	/** Restore all modes and extensions of all members on a channel
	 * @param chan Channel whose members are being restored
	 * @param memberdatalist Data to restore
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

	for (const auto& [_, user] : ServerInstance->Users.GetUsers())
	{
		// Serialize user modes
		for (size_t j = 0; j < handledmodes[MODETYPE_USER].size(); j++)
		{
			ModeHandler* mh = handledmodes[MODETYPE_USER][j].mh;
			if (user->IsModeSet(mh))
				currdata.modelist.emplace_back(j, mh->GetUserParameter(user));
		}

		// Serialize all extensions attached to the User
		SaveExtensions(user, currdata.extlist);

		// Save serializer name if applicable and get an index to it
		size_t serializerindex = SaveSerializer(user);

		// Add to list if the user has any modes or extensions set that we are interested in, otherwise we don't
		// have to do anything with this user when restoring
		if ((!currdata.empty()) || (serializerindex != UserData::UNUSED_INDEX))
		{
			userdatalist.emplace_back(user, serializerindex);
			userdatalist.back().swap(currdata);
		}
	}
}

size_t DataKeeper::GetSerializerIndex(ClientProtocol::Serializer* serializer)
{
	for (size_t i = 0; i < handledserializers.size(); i++)
	{
		if (handledserializers[i].serializer == serializer)
			return i;
	}

	handledserializers.emplace_back(serializer);
	return handledserializers.size()-1;
}

size_t DataKeeper::SaveSerializer(User* user)
{
	LocalUser* const localuser = IS_LOCAL(user);
	if ((!localuser) || (!localuser->serializer))
		return UserData::UNUSED_INDEX;
	if (localuser->serializer->creator != mod)
		return UserData::UNUSED_INDEX;

	const size_t serializerindex = GetSerializerIndex(localuser->serializer);
	localuser->serializer = dummyserializer;
	return serializerindex;
}

void DataKeeper::SaveExtensions(Extensible* extensible, std::vector<InstanceData>& extdata)
{
	const Extensible::ExtensibleStore& setexts = extensible->GetExtList();

	// Position of the extension saved in the handledexts list
	size_t index = 0;
	for (const auto& prov : handledexts)
	{
		ExtensionItem* const item = prov.extitem;
		Extensible::ExtensibleStore::const_iterator it = setexts.find(item);
		if (it == setexts.end())
			continue;

		std::string value = item->ToInternal(extensible, it->second);
		// If the serialized value is empty the extension won't be saved and restored
		if (!value.empty())
			extdata.emplace_back(index, value);
	}
}

void DataKeeper::SaveListModes(Channel* chan, ListModeBase* lm, size_t index, ModesExts& currdata)
{
	const ListModeBase::ModeList* list = lm->GetList(chan);
	if (!list)
		return;

	for (const auto& listitem : *list)
		currdata.modelist.emplace_back(index, listitem.mask);
}

void DataKeeper::DoSaveChans()
{
	ModesExts currdata;
	std::vector<OwnedModesExts> currmemberdata;

	for (const auto& [_, chan] : ServerInstance->Channels.GetChans())
	{
		// Serialize channel modes
		for (size_t j = 0; j < handledmodes[MODETYPE_CHANNEL].size(); j++)
		{
			ModeHandler* mh = handledmodes[MODETYPE_CHANNEL][j].mh;
			ListModeBase* lm = mh->IsListModeBase();
			if (lm)
				SaveListModes(chan, lm, j, currdata);
			else if (chan->IsModeSet(mh))
				currdata.modelist.emplace_back(j, chan->GetModeParameter(mh));
		}

		// Serialize all extensions attached to the Channel
		SaveExtensions(chan, currdata.extlist);

		// Serialize all extensions attached to and all modes set on all members of the channel
		SaveMemberData(chan, currmemberdata);

		// Same logic as in DoSaveUsers() plus we consider the modes and extensions of all members
		if ((!currdata.empty()) || (!currmemberdata.empty()))
		{
			chandatalist.emplace_back(chan);
			chandatalist.back().swap(currdata);
			chandatalist.back().memberdatalist.swap(currmemberdata);
		}
	}
}

void DataKeeper::SaveMemberData(Channel* chan, std::vector<OwnedModesExts>& memberdatalist)
{
	ModesExts currdata;
	for (const auto& [_, memb] : chan->GetUsers())
	{
		for (size_t j = 0; j < handledmodes[MODETYPE_CHANNEL].size(); j++)
		{
			ModeHandler* mh = handledmodes[MODETYPE_CHANNEL][j].mh;
			const PrefixMode* const pm = mh->IsPrefixMode();
			if ((pm) && (memb->HasMode(pm)))
				currdata.modelist.emplace_back(j, memb->user->uuid); // Need to pass the user's uuid to the mode parser to set the mode later
		}

		SaveExtensions(memb, currdata.extlist);

		// Same logic as in DoSaveUsers()
		if (!currdata.empty())
		{
			memberdatalist.emplace_back(memb->user->uuid);
			memberdatalist.back().swap(currdata);
		}
	}
}

void DataKeeper::RestoreMemberData(Channel* chan, const std::vector<ChanData::MemberData>& memberdatalist, Modes::ChangeList& modechange)
{
	for (const auto& md : memberdatalist)
	{
		User* const user = ServerInstance->Users.FindUUID(md.owner);
		if (!user)
		{
			ServerInstance->Logs.Debug(MODNAME, "User {} is gone (while processing {})", md.owner, chan->name);
			continue;
		}

		Membership* const memb = chan->GetUser(user);
		if (!memb)
		{
			ServerInstance->Logs.Debug(MODNAME, "Member {} is no longer on channel {}", md.owner, chan->name);
			continue;
		}

		RestoreObj(md, memb, MODETYPE_CHANNEL, modechange);
	}
}

void DataKeeper::CreateModeList(ModeType modetype)
{
	for (const auto& [_, mh] : ServerInstance->Modes.GetModes(modetype))
	{
		if (mh->creator == mod)
			handledmodes[modetype].emplace_back(mh);
	}
}

void DataKeeper::Save(Module* currmod)
{
	this->mod = currmod;

	for (const auto& [_, ext] : ServerInstance->Extensions.GetExts())
	{
		if (ext->creator == mod)
			handledexts.emplace_back(ext);
	}

	CreateModeList(MODETYPE_USER);
	DoSaveUsers();

	CreateModeList(MODETYPE_CHANNEL);
	DoSaveChans();

	reloadevprov->Call(&ReloadModule::EventListener::OnReloadModuleSave, mod, this->moddata);

	ServerInstance->Logs.Debug(MODNAME, "Saved data about {} users {} chans {} modules", userdatalist.size(), chandatalist.size(), moddata.list.size());
}

void DataKeeper::VerifyServiceProvider(const ProviderInfo& service, const char* type)
{
	const ServiceProvider* sp = service.extitem;
	if (!sp)
		ServerInstance->Logs.Debug(MODNAME, "{} \"{}\" is no longer available", type, service.itemname);
	else if (sp->creator != mod)
		ServerInstance->Logs.Debug(MODNAME, "{} \"{}\" is now handled by {}", type, service.itemname, (sp->creator ? sp->creator->ModuleFile : "<core>"));
}

void DataKeeper::LinkModes(ModeType modetype)
{
	for (auto& item : handledmodes[modetype])
	{
		item.mh = ServerInstance->Modes.FindMode(item.itemname, modetype);
		VerifyServiceProvider(item, (modetype == MODETYPE_USER ? "User mode" : "Channel mode"));
	}
}

void DataKeeper::LinkExtensions()
{
	for (auto& item : handledexts)
	{
		item.extitem = ServerInstance->Extensions.GetItem(item.itemname);
		VerifyServiceProvider(item.extitem, "Extension");
	}
}

void DataKeeper::LinkSerializers()
{
	for (auto& item : handledserializers)
	{
		item.serializer = ServerInstance->Modules.FindDataService<ClientProtocol::Serializer>(item.itemname);
		VerifyServiceProvider(item.serializer, "Serializer");
	}
}

void DataKeeper::Restore(Module* newmod)
{
	this->mod = newmod;

	// Find the new extension items
	LinkExtensions();
	LinkModes(MODETYPE_USER);
	LinkModes(MODETYPE_CHANNEL);
	LinkSerializers();

	// Restore
	DoRestoreUsers();
	DoRestoreChans();
	DoRestoreModules();

	ServerInstance->Logs.Debug(MODNAME, "Restore finished");
}

void DataKeeper::Fail()
{
	this->mod = nullptr;

	ServerInstance->Logs.Debug(MODNAME, "Restore failed, notifying modules");
	DoRestoreModules();
}

void DataKeeper::RestoreObj(const OwnedModesExts& data, Extensible* extensible, ModeType modetype, Modes::ChangeList& modechange)
{
	RestoreExtensions(data.extlist, extensible);
	RestoreModes(data.modelist, modetype, modechange);
}

void DataKeeper::RestoreExtensions(const std::vector<InstanceData>& list, Extensible* extensible)
{
	for (const auto& id : list)
		handledexts[id.index].extitem->FromInternal(extensible, id.serialized);
}

void DataKeeper::RestoreModes(const std::vector<InstanceData>& list, ModeType modetype, Modes::ChangeList& modechange)
{
	for (const auto& id : list)
		modechange.push_add(handledmodes[modetype][id.index].mh, id.serialized);
}

bool DataKeeper::RestoreSerializer(size_t serializerindex, User* user)
{
	if (serializerindex == UserData::UNUSED_INDEX)
		return true;

	// The following checks are redundant
	LocalUser* const localuser = IS_LOCAL(user);
	if (!localuser)
		return true;
	if (localuser->serializer != dummyserializer)
		return true;

	const ProviderInfo& provinfo = handledserializers[serializerindex];
	if (!provinfo.serializer)
	{
		// Users cannot exist without a serializer
		ServerInstance->Users.QuitUser(user, "Serializer lost in reload");
		return false;
	}

	localuser->serializer = provinfo.serializer;
	return true;
}

void DataKeeper::DoRestoreUsers()
{
	ServerInstance->Logs.Debug(MODNAME, "Restoring user data");
	Modes::ChangeList modechange;

	for (const auto& userdata : userdatalist)
	{
		User* const user = ServerInstance->Users.FindUUID(userdata.owner);
		if (!user)
		{
			ServerInstance->Logs.Debug(MODNAME, "User {} is gone", userdata.owner);
			continue;
		}

		// Attempt to restore serializer first, if it fails it's a fatal error and RestoreSerializer() quits them
		if (!RestoreSerializer(userdata.serializerindex, user))
			continue;

		RestoreObj(userdata, user, MODETYPE_USER, modechange);
		ServerInstance->Modes.Process(ServerInstance->FakeClient, nullptr, user, modechange, ModeParser::MODE_LOCALONLY);
		modechange.clear();
	}
}

void DataKeeper::DoRestoreChans()
{
	ServerInstance->Logs.Debug(MODNAME, "Restoring channel data");
	Modes::ChangeList modechange;

	for (const auto& chandata : chandatalist)
	{
		Channel* const chan = ServerInstance->Channels.Find(chandata.owner);
		if (!chan)
		{
			ServerInstance->Logs.Debug(MODNAME, "Channel {} not found", chandata.owner);
			continue;
		}

		RestoreObj(chandata, chan, MODETYPE_CHANNEL, modechange);
		// Process the mode change before applying any prefix modes
		ServerInstance->Modes.Process(ServerInstance->FakeClient, chan, nullptr, modechange, ModeParser::MODE_LOCALONLY);
		modechange.clear();

		// Restore all member data
		RestoreMemberData(chan, chandata.memberdatalist, modechange);
		ServerInstance->Modes.Process(ServerInstance->FakeClient, chan, nullptr, modechange, ModeParser::MODE_LOCALONLY);
		modechange.clear();
	}
}

void DataKeeper::DoRestoreModules()
{
	for (const auto& data : moddata.list)
	{
		ServerInstance->Logs.Debug(MODNAME, "Calling module data handler {}", fmt::ptr(data.handler));
		data.handler->OnReloadModuleRestore(mod, data.data);
	}
}

} // namespace ReloadModule

class ReloadAction final
	: public ActionBase
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

	void Call() override
	{
		ReloadModule::DataKeeper datakeeper;
		datakeeper.Save(mod);

		std::string name = mod->ModuleFile;
		ServerInstance->Modules.DoSafeUnload(mod);
		ServerInstance->GlobalCulls.Apply();
		bool result = ServerInstance->Modules.Load(name);

		if (result)
		{
			Module* newmod = ServerInstance->Modules.Find(name);
			datakeeper.Restore(newmod);
			ServerInstance->SNO.WriteGlobalSno('a', "The {} module was reloaded.", passedname);
		}
		else
		{
			datakeeper.Fail();
			ServerInstance->SNO.WriteGlobalSno('a', "Failed to reload the {} module.", passedname);
		}

		auto* user = ServerInstance->Users.FindUUID(uuid);
		if (user)
		{
			if (result)
				user->WriteNumeric(RPL_LOADEDMODULE, passedname, INSP_FORMAT("The {} module was reloaded.", passedname));
			else
				user->WriteNumeric(ERR_CANTUNLOADMODULE, passedname, INSP_FORMAT("Failed to reload the {} module.", passedname));
		}

		ServerInstance->GlobalCulls.AddItem(this);
	}
};

CmdResult CommandReloadmodule::Handle(User* user, const Params& parameters)
{
	Module* m = ServerInstance->Modules.Find(parameters[0]);
	if (m == creator)
	{
		user->WriteNumeric(ERR_CANTUNLOADMODULE, parameters[0], "You cannot reload core_reloadmodule (unload and load it)");
		return CmdResult::FAILURE;
	}

	if (creator->dying)
		return CmdResult::FAILURE;

	if ((m) && (ServerInstance->Modules.CanUnload(m)))
	{
		ServerInstance->AtomicActions.AddAction(new ReloadAction(m, user->uuid, parameters[0]));
		return CmdResult::SUCCESS;
	}
	else
	{
		user->WriteNumeric(ERR_CANTUNLOADMODULE, parameters[0], "Could not find a loaded module by that name");
		return CmdResult::FAILURE;
	}
}

class CoreModReloadmodule final
	: public Module
{
private:
	CommandReloadmodule cmd;

public:
	CoreModReloadmodule()
		: Module(VF_CORE | VF_VENDOR, "Provides the RELOADMODULE command")
		, cmd(this)
	{
	}
};

MODULE_INIT(CoreModReloadmodule)
