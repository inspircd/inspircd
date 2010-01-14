/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"
#include "socket.h"
#include "socketengine.h"
#include "command_parse.h"
#include "dns.h"
#include "exitcodes.h"

#ifndef WIN32
	#include <dirent.h>
#endif


// Version is a simple class for holding a modules version number
template<>
VersionBase<API_VERSION>::VersionBase(const std::string &desc, int flags)
: description(desc), Flags(flags)
{
}

template<>
VersionBase<API_VERSION>::VersionBase(const std::string &desc, int flags, const std::string& linkdata)
: description(desc), Flags(flags), link_data(linkdata)
{
}

template<>
bool VersionBase<API_VERSION>::CanLink(const std::string& other_data)
{
	return link_data == other_data;
}

Request::Request(Module* src, Module* dst, const char* idstr)
: id(idstr), source(src), dest(dst)
{
}

void Request::Send()
{
	if (dest)
		dest->OnRequest(*this);
}

Event::Event(Module* src, const std::string &eventid) : source(src), id(eventid) { }

void Event::Send()
{
	FOREACH_MOD(I_OnEvent,OnEvent(*this));
}

// These declarations define the behavours of the base class Module (which does nothing at all)

Module::Module() { }
CullResult Module::cull()
{
	return classbase::cull();
}
Module::~Module()
{
}

ModResult	Module::OnSendSnotice(char &snomask, std::string &type, const std::string &message) { return MOD_RES_PASSTHRU; }
void		Module::OnUserConnect(LocalUser*) { }
void		Module::OnUserQuit(User*, const std::string&, const std::string&) { }
void		Module::OnUserDisconnect(LocalUser*) { }
void		Module::OnUserJoin(Membership*, bool, bool, CUList&) { }
void		Module::OnPostJoin(Membership*) { }
void		Module::OnUserPart(Membership*, std::string&, CUList&) { }
void		Module::OnPreRehash(User*, const std::string&) { }
void		Module::OnModuleRehash(User*, const std::string&) { }
void		Module::OnRehash(User*) { }
ModResult	Module::OnUserPreJoin(User*, Channel*, const char*, std::string&, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnMode(User*, void*, int, const std::vector<std::string>&, const std::vector<TranslateType>&) { }
void		Module::OnOper(User*, const std::string&) { }
void		Module::OnPostOper(User*, const std::string&, const std::string &) { }
void		Module::OnInfo(User*) { }
void		Module::OnWhois(User*, User*) { }
ModResult	Module::OnUserPreInvite(User*, User*, Channel*, time_t) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreMessage(User*, void*, int, std::string&, char, CUList&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreNotice(User*, void*, int, std::string&, char, CUList&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreNick(User*, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnUserPostNick(User*, const std::string&) { }
ModResult	Module::OnPreMode(User*, User*, Channel*, const std::vector<std::string>&) { return MOD_RES_PASSTHRU; }
void		Module::On005Numeric(std::string&) { }
ModResult	Module::OnKill(User*, User*, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnLoadModule(Module*) { }
void		Module::OnUnloadModule(Module*) { }
void		Module::OnBackgroundTimer(time_t) { }
ModResult	Module::OnPreCommand(std::string&, std::vector<std::string>&, User *, bool, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnPostCommand(const std::string&, const std::vector<std::string>&, User *, CmdResult, const std::string&) { }
ModResult	Module::OnCheckReady(LocalUser*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserRegister(LocalUser*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserPreKick(User*, Membership*, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnUserKick(User*, Membership*, const std::string&, CUList&) { }
ModResult	Module::OnRawMode(User*, Channel*, const char, const std::string &, bool, int) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckInvite(User*, Channel*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckKey(User*, Channel*, const std::string&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckLimit(User*, Channel*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckChannelBan(User*, Channel*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnCheckBan(User*, Channel*, const std::string&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnExtBanCheck(User*, Channel*, char) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnStats(char, User*, string_list&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnChangeLocalUserHost(LocalUser*, const std::string&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnChangeLocalUserGECOS(LocalUser*, const std::string&) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnPreTopicChange(User*, Channel*, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnEvent(Event&) { }
void		Module::OnRequest(Request&) { }
ModResult	Module::OnPassCompare(Extensible* ex, const std::string &password, const std::string &input, const std::string& hashtype) { return MOD_RES_PASSTHRU; }
void		Module::OnGlobalOper(User*) { }
void		Module::OnPostConnect(User*) { }
ModResult	Module::OnAddBan(User*, Channel*, const std::string &) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnDelBan(User*, Channel*, const std::string &) { return MOD_RES_PASSTHRU; }
void		Module::OnStreamSocketAccept(StreamSocket*, irc::sockets::sockaddrs*, irc::sockets::sockaddrs*) { }
int		Module::OnStreamSocketWrite(StreamSocket*, std::string&) { return -1; }
void		Module::OnStreamSocketClose(StreamSocket*) { }
void		Module::OnStreamSocketConnect(StreamSocket*) { }
int		Module::OnStreamSocketRead(StreamSocket*, std::string&) { return -1; }
void		Module::OnUserMessage(User*, void*, int, const std::string&, char, const CUList&) { }
void		Module::OnUserNotice(User*, void*, int, const std::string&, char, const CUList&) { }
void 		Module::OnRemoteKill(User*, User*, const std::string&, const std::string&) { }
void		Module::OnUserInvite(User*, User*, Channel*, time_t) { }
void		Module::OnPostTopicChange(User*, Channel*, const std::string&) { }
void		Module::OnGetServerDescription(const std::string&, std::string&) { }
void		Module::OnSyncUser(User*, Module*, void*) { }
void		Module::OnSyncChannel(Channel*, Module*, void*) { }
void		Module::OnSyncNetwork(Module*, void*) { }
void		Module::ProtoSendMode(void*, TargetTypeFlags, void*, const std::vector<std::string>&, const std::vector<TranslateType>&) { }
void		Module::OnDecodeMetaData(Extensible*, const std::string&, const std::string&) { }
void		Module::ProtoSendMetaData(void*, Extensible*, const std::string&, const std::string&) { }
void		Module::OnWallops(User*, const std::string&) { }
void		Module::OnChangeHost(User*, const std::string&) { }
void		Module::OnChangeName(User*, const std::string&) { }
void		Module::OnChangeIdent(User*, const std::string&) { }
void		Module::OnAddLine(User*, XLine*) { }
void		Module::OnDelLine(User*, XLine*) { }
void		Module::OnExpireLine(XLine*) { }
void 		Module::OnCleanup(int, void*) { }
ModResult	Module::OnChannelPreDelete(Channel*) { return MOD_RES_PASSTHRU; }
void		Module::OnChannelDelete(Channel*) { }
ModResult	Module::OnSetAway(User*, const std::string &) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnUserList(User*, Channel*) { return MOD_RES_PASSTHRU; }
ModResult	Module::OnWhoisLine(User*, User*, int&, std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnBuildNeighborList(User*, UserChanList&, std::map<User*,bool>&) { }
void		Module::OnGarbageCollect() { }
void 		Module::OnText(User*, void*, int, const std::string&, char, CUList&) { }
void		Module::OnRunTestSuite() { }
void		Module::OnNamesListItem(User*, Membership*, std::string&, std::string&) { }
ModResult	Module::OnNumeric(User*, unsigned int, const std::string&) { return MOD_RES_PASSTHRU; }
void		Module::OnHookIO(StreamSocket*, ListenSocket*) { }
ModResult   Module::OnAcceptConnection(int, ListenSocket*, irc::sockets::sockaddrs*, irc::sockets::sockaddrs*) { return MOD_RES_PASSTHRU; }
void		Module::OnSendWhoLine(User*, User*, Channel*, std::string&) { }
ModResult	Module::OnChannelRestrictionApply(User*, Channel*, const char*) { return MOD_RES_PASSTHRU; }

ModuleManager::ModuleManager() : ModCount(0)
{
}

ModuleManager::~ModuleManager()
{
}

bool ModuleManager::Attach(Implementation i, Module* mod)
{
	if (std::find(EventHandlers[i].begin(), EventHandlers[i].end(), mod) != EventHandlers[i].end())
		return false;

	EventHandlers[i].push_back(mod);
	return true;
}

bool ModuleManager::Detach(Implementation i, Module* mod)
{
	EventHandlerIter x = std::find(EventHandlers[i].begin(), EventHandlers[i].end(), mod);

	if (x == EventHandlers[i].end())
		return false;

	EventHandlers[i].erase(x);
	return true;
}

void ModuleManager::Attach(Implementation* i, Module* mod, size_t sz)
{
	for (size_t n = 0; n < sz; ++n)
		Attach(i[n], mod);
}

void ModuleManager::DetachAll(Module* mod)
{
	for (size_t n = I_BEGIN + 1; n != I_END; ++n)
		Detach((Implementation)n, mod);
}

bool ModuleManager::SetPriority(Module* mod, Priority s)
{
	for (size_t n = I_BEGIN + 1; n != I_END; ++n)
		SetPriority(mod, (Implementation)n, s);

	return true;
}

bool ModuleManager::SetPriority(Module* mod, Implementation i, Priority s, Module** modules, size_t sz)
{
	/** To change the priority of a module, we first find its position in the vector,
	 * then we find the position of the other modules in the vector that this module
	 * wants to be before/after. We pick off either the first or last of these depending
	 * on which they want, and we make sure our module is *at least* before or after
	 * the first or last of this subset, depending again on the type of priority.
	 */
	size_t swap_pos = 0;
	size_t source = 0;
	bool swap = true;
	bool found = false;

	/* Locate our module. This is O(n) but it only occurs on module load so we're
	 * not too bothered about it
	 */
	for (size_t x = 0; x != EventHandlers[i].size(); ++x)
	{
		if (EventHandlers[i][x] == mod)
		{
			source = x;
			found = true;
			break;
		}
	}

	/* Eh? this module doesnt exist, probably trying to set priority on an event
	 * theyre not attached to.
	 */
	if (!found)
		return false;

	switch (s)
	{
		/* Dummy value */
		case PRIORITY_DONTCARE:
			swap = false;
		break;
		/* Module wants to be first, sod everything else */
		case PRIORITY_FIRST:
			if (prioritizationState != PRIO_STATE_FIRST)
				swap = false;
			else
				swap_pos = 0;
		break;
		/* Module wants to be last. */
		case PRIORITY_LAST:
			if (prioritizationState != PRIO_STATE_FIRST)
				swap = false;
			else if (EventHandlers[i].empty())
				swap_pos = 0;
			else
				swap_pos = EventHandlers[i].size() - 1;
		break;
		/* Place this module after a set of other modules */
		case PRIORITY_AFTER:
		{
			/* Find the latest possible position */
			swap_pos = 0;
			swap = false;
			for (size_t x = 0; x != EventHandlers[i].size(); ++x)
			{
				for (size_t n = 0; n < sz; ++n)
				{
					if ((modules[n]) && (EventHandlers[i][x] == modules[n]) && (x >= swap_pos) && (source <= swap_pos))
					{
						swap_pos = x;
						swap = true;
					}
				}
			}
		}
		break;
		/* Place this module before a set of other modules */
		case PRIORITY_BEFORE:
		{
			swap_pos = EventHandlers[i].size() - 1;
			swap = false;
			for (size_t x = 0; x != EventHandlers[i].size(); ++x)
			{
				for (size_t n = 0; n < sz; ++n)
				{
					if ((modules[n]) && (EventHandlers[i][x] == modules[n]) && (x <= swap_pos) && (source >= swap_pos))
					{
						swap = true;
						swap_pos = x;
					}
				}
			}
		}
		break;
	}

	/* Do we need to swap? */
	if (swap && (swap_pos != source))
	{
		// We are going to change positions; we'll need to run again to verify all requirements
		if (prioritizationState == PRIO_STATE_LAST)
			prioritizationState = PRIO_STATE_AGAIN;
		/* Suggestion from Phoenix, "shuffle" the modules to better retain call order */
		int incrmnt = 1;

		if (source > swap_pos)
			incrmnt = -1;

		for (unsigned int j = source; j != swap_pos; j += incrmnt)
		{
			if (( j + incrmnt > EventHandlers[i].size() - 1) || (j + incrmnt < 0))
				continue;

			std::swap(EventHandlers[i][j], EventHandlers[i][j+incrmnt]);
		}
	}

	return true;
}

std::string& ModuleManager::LastError()
{
	return LastModuleError;
}

CmdResult InspIRCd::CallCommandHandler(const std::string &commandname, const std::vector<std::string>& parameters, User* user)
{
	return this->Parser->CallHandler(commandname, parameters, user);
}

bool InspIRCd::IsValidModuleCommand(const std::string &commandname, int pcnt, User* user)
{
	return this->Parser->IsValidCommand(commandname, pcnt, user);
}

void InspIRCd::AddCommand(Command *f)
{
	if (!this->Parser->AddCommand(f))
	{
		throw ModuleException("Command "+std::string(f->name)+" already exists.");
	}
}

void ModuleManager::AddService(ServiceProvider& item)
{
	switch (item.service)
	{
		case SERVICE_COMMAND:
			if (!ServerInstance->Parser->AddCommand(static_cast<Command*>(&item)))
				throw ModuleException("Command "+std::string(item.name)+" already exists.");
			return;
		case SERVICE_CMODE:
		case SERVICE_UMODE:
			if (!ServerInstance->Modes->AddMode(static_cast<ModeHandler*>(&item)))
				throw ModuleException("Mode "+std::string(item.name)+" already exists.");
			return;
		case SERVICE_METADATA:
			ServerInstance->Extensions.Register(static_cast<ExtensionItem*>(&item));
			return;
		case SERVICE_DATA:
		case SERVICE_IOHOOK:
		{
			DataProviders.insert(std::make_pair(item.name, &item));
			std::string::size_type slash = item.name.find('/');
			if (slash != std::string::npos)
			{
				DataProviders.insert(std::make_pair(item.name.substr(0, slash), &item));
				DataProviders.insert(std::make_pair(item.name.substr(slash + 1), &item));
			}
			return;
		}
		default:
			throw ModuleException("Cannot add unknown service type");
	}
}

ServiceProvider* ModuleManager::FindService(ServiceType type, const std::string& name)
{
	switch (type)
	{
		case SERVICE_DATA:
		case SERVICE_IOHOOK:
		{
			std::multimap<std::string, ServiceProvider*>::iterator i = DataProviders.find(name);
			if (i != DataProviders.end() && i->second->service == type)
				return i->second;
			return NULL;
		}
		// TODO implement finding of the other types
		default:
			throw ModuleException("Cannot find unknown service type");
	}
}

dynamic_reference_base::dynamic_reference_base(Module* Creator, const std::string& Name)
	: name(Name), value(NULL), creator(Creator)
{
	ServerInstance->Modules->ActiveDynrefs.push_back(this);
}

dynamic_reference_base::~dynamic_reference_base()
{
	for(unsigned int i = 0; i < ServerInstance->Modules->ActiveDynrefs.size(); i++)
	{
		if (ServerInstance->Modules->ActiveDynrefs[i] == this)
		{
			unsigned int last = ServerInstance->Modules->ActiveDynrefs.size() - 1;
			if (i != last)
				ServerInstance->Modules->ActiveDynrefs[i] = ServerInstance->Modules->ActiveDynrefs[last];
			ServerInstance->Modules->ActiveDynrefs.erase(ServerInstance->Modules->ActiveDynrefs.begin() + last);
			return;
		}
	}
}

void dynamic_reference_base::SetProvider(const std::string& newname)
{
	name = newname;
	ClearCache();
}

void dynamic_reference_base::lookup()
{
	if (!*this)
		throw ModuleException("Dynamic reference to '" + name + "' failed to resolve");
}

dynamic_reference_base::operator bool()
{
	if (!value)
	{
		std::multimap<std::string, ServiceProvider*>::iterator i = ServerInstance->Modules->DataProviders.find(name);
		if (i != ServerInstance->Modules->DataProviders.end())
			value = static_cast<DataProvider*>(i->second);
	}
	return value;
}

void InspIRCd::SendMode(const std::vector<std::string>& parameters, User *user)
{
	this->Modes->Process(parameters, user);
}

bool InspIRCd::AddResolver(Resolver* r, bool cached)
{
	if (!cached)
		return this->Res->AddResolverClass(r);
	else
	{
		r->TriggerCachedResult();
		delete r;
		return true;
	}
}

Module* ModuleManager::Find(const std::string &name)
{
	std::map<std::string, Module*>::iterator modfind = Modules.find(name);

	if (modfind == Modules.end())
		return NULL;
	else
		return modfind->second;
}

const std::vector<std::string> ModuleManager::GetAllModuleNames(int filter)
{
	std::vector<std::string> retval;
	for (std::map<std::string, Module*>::iterator x = Modules.begin(); x != Modules.end(); ++x)
		if (!filter || (x->second->GetVersion().Flags & filter))
			retval.push_back(x->first);
	return retval;
}

ConfigReader::ConfigReader()
{
	this->error = 0;
}


ConfigReader::~ConfigReader()
{
}

static ConfigTag* SlowGetTag(const std::string &tag, int index)
{
	ConfigTagList tags = ServerInstance->Config->ConfTags(tag);
	while (tags.first != tags.second)
	{
		if (!index)
			return tags.first->second;
		tags.first++;
		index--;
	}
	return NULL;
}

std::string ConfigReader::ReadValue(const std::string &tag, const std::string &name, const std::string &default_value, int index, bool allow_linefeeds)
{
	std::string result = default_value;
	if (!SlowGetTag(tag, index)->readString(name, result, allow_linefeeds))
	{
		this->error = CONF_VALUE_NOT_FOUND;
	}
	return result;
}

std::string ConfigReader::ReadValue(const std::string &tag, const std::string &name, int index, bool allow_linefeeds)
{
	return ReadValue(tag, name, "", index, allow_linefeeds);
}

bool ConfigReader::ReadFlag(const std::string &tag, const std::string &name, const std::string &default_value, int index)
{
	bool def = (default_value == "yes");
	return SlowGetTag(tag, index)->getBool(name, def);
}

bool ConfigReader::ReadFlag(const std::string &tag, const std::string &name, int index)
{
	return ReadFlag(tag, name, "", index);
}


int ConfigReader::ReadInteger(const std::string &tag, const std::string &name, const std::string &default_value, int index, bool need_positive)
{
	int v = atoi(default_value.c_str());
	int result = SlowGetTag(tag, index)->getInt(name, v);

	if ((need_positive) && (result < 0))
	{
		this->error = CONF_INT_NEGATIVE;
		return 0;
	}

	return result;
}

int ConfigReader::ReadInteger(const std::string &tag, const std::string &name, int index, bool need_positive)
{
	return ReadInteger(tag, name, "", index, need_positive);
}

long ConfigReader::GetError()
{
	long olderr = this->error;
	this->error = 0;
	return olderr;
}

int ConfigReader::Enumerate(const std::string &tag)
{
	ServerInstance->Logs->Log("MODULE", DEBUG, "Module is using ConfigReader::Enumerate on %s; this is slow!",
		tag.c_str());
	int i=0;
	while (SlowGetTag(tag, i)) i++;
	return i;
}

FileReader::FileReader(const std::string &filename)
{
	LoadFile(filename);
}

FileReader::FileReader()
{
}

std::string FileReader::Contents()
{
	std::string x;
	for (file_cache::iterator a = this->fc.begin(); a != this->fc.end(); a++)
	{
		x.append(*a);
		x.append("\r\n");
	}
	return x;
}

unsigned long FileReader::ContentSize()
{
	return this->contentsize;
}

void FileReader::CalcSize()
{
	unsigned long n = 0;
	for (file_cache::iterator a = this->fc.begin(); a != this->fc.end(); a++)
		n += (a->length() + 2);
	this->contentsize = n;
}

void FileReader::LoadFile(const std::string &filename)
{
	file_cache c;
	c.clear();
	if (ServerInstance->Config->ReadFile(c,filename.c_str()))
	{
		this->fc = c;
		this->CalcSize();
	}
}


FileReader::~FileReader()
{
}

bool FileReader::Exists()
{
	return (!(fc.size() == 0));
}

std::string FileReader::GetLine(int x)
{
	if ((x<0) || ((unsigned)x>fc.size()))
		return "";
	return fc[x];
}

int FileReader::FileSize()
{
	return fc.size();
}
