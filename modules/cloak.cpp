/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023 Sadie Powell <sadie@witchery.services>
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
#include "clientprotocolevent.h"
#include "modules/cloak.h"
#include "modules/ircv3_replies.h"
#include "utility/map.h"

typedef std::vector<Cloak::MethodPtr> CloakMethodList;

class CommandCloak final
	: public SplitCommand
{
private:
	// The cloak engines from the config.
	CloakMethodList& cloakmethods;

	// API for sending a FAIL message.
	IRCv3::Replies::Note failrpl;

	// API for sending a NOTE message.
	IRCv3::Replies::Note noterpl;

	// Reference to the standard-replies cap.
	IRCv3::Replies::CapReference stdrplcap;

public:
	CommandCloak(Module* Creator, CloakMethodList& ce)
		: SplitCommand(Creator, "CLOAK", 1)
		, cloakmethods(ce)
		, failrpl(Creator)
		, noterpl(Creator)
		, stdrplcap(Creator)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<host>" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		size_t count = 0;
		for (const auto& cloakmethod : cloakmethods)
		{
			const std::string cloak = cloakmethod->Generate(parameters[0]);
			if (cloak.empty())
				continue;

			noterpl.SendIfCap(user, stdrplcap, this, "CLOAK_RESULT", parameters[0], cloak, fmt::format("Cloak #{} for {} is {} (method: {})",
				++count, parameters[0], cloak, cloakmethod->GetName()));
		}

		if (!count)
		{
			failrpl.SendIfCap(user, stdrplcap, this, "UNABLE_TO_CLOAK", parameters[0], fmt::format("There are no methods available for cloaking {}",
				parameters[0]));
		}

		return CmdResult::SUCCESS;
	}
};

class CloakAPI final
	: public Cloak::APIBase
{
private:
	// The cloak providers from the config.
	CloakMethodList& cloakmethods;

	// The mode which marks a user as being cloaked.
	ModeHandler* cloakmode;

	// Holds the list of cloaks for a user.
	ListExtItem<Cloak::List> ext;

	std::string GetFrontCloak(LocalUser* user)
	{
		auto* cloaks = GetCloaks(user);
		return cloaks ? cloaks->front() : "";
	}

public:
	bool recloaking = false;

	CloakAPI(Module* Creator, CloakMethodList& cm, ModeHandler* mh)
		: Cloak::APIBase(Creator)
		, cloakmethods(cm)
		, cloakmode(mh)
		, ext(Creator, "cloaks", ExtensionType::USER)
	{
	}

	Cloak::List* GetCloaks(LocalUser* user) override
	{
		if (user->quitting || !(user->connected & User::CONN_NICKUSER))
			return nullptr; // This user isn't at a point where they can change their cloak.

		if (!user->GetClass()->config->getBool("usecloak", true))
			return nullptr;

		auto* cloaks = ext.Get(user);
		if (!cloaks)
		{
			// The list doesn't exist so try to create it.
			cloaks = new Cloak::List();
			for (const auto& cloakmethod : cloakmethods)
			{
				const std::string cloak = cloakmethod->Generate(user);
				if (!cloak.empty())
				{
					cloaks->push_back(cloak);

					ServerInstance->Logs.Debug(MODNAME, "Cloaked {} ({}) [{}] as {} using the {} method.",
						user->uuid, user->GetAddress(), user->GetRealHost(), cloak,
						cloakmethod->GetName());
				}
				else
				{
					ServerInstance->Logs.Debug(MODNAME, "Unable to cloak {} ({}) [{}] using the {} method.",
						user->uuid, user->GetAddress(), user->GetRealHost(), cloakmethod->GetName());
				}
			}
			ext.Set(user, cloaks);
		}
		return cloaks->empty() ? nullptr : cloaks;
	}

	bool IsActiveCloak(const Cloak::Engine& engine) override
	{
		for (const auto& cloakmethod : cloakmethods)
		{
			if (cloakmethod->IsProvidedBy(engine))
				return true;
		}
		return false;
	}

	void ResetCloaks(LocalUser* user, bool resetdisplay) override
	{
		if (user->quitting || !(user->connected & User::CONN_NICKUSER))
			return; // This user isn't at a point where they can change their cloak.

		const std::string oldcloak = GetFrontCloak(user);
		ext.Unset(user);

		if (!resetdisplay || !user->IsModeSet(cloakmode))
			return; // Not resetting the display or not cloaked.

		const std::string newcloak = GetFrontCloak(user);
		if (oldcloak == newcloak)
			return; // New front cloak is the same.

		Modes::ChangeList changelist;
		changelist.push_remove(cloakmode);
		if (!newcloak.empty())
		{
			recloaking = true;
			changelist.push_add(cloakmode);
		}
		ServerInstance->Modes.Process(ServerInstance->FakeClient, nullptr, user, changelist);
		recloaking = false;
	}
};

class CloakMode final
	: public ModeHandler
{
private:
	// The public API for the cloak system.
	CloakAPI& cloakapi;

	// The number of times the last user has set/unset this mode at once.
	size_t prevcount = 0;

	// The time at which the last user set/unset this mode.
	time_t prevtime = 0;

	// The UUID of the last user to set/unset this mode.
	std::string prevuuid;

	bool CheckSpam(User* user)
	{
		if (cloakapi.recloaking)
			return false; // Recloaking is always allowed.

		if (user->uuid == prevuuid && prevtime == ServerInstance->Time())
		{
			// The user has changed this mode already recently. Have they done
			// it too much?
			return ++prevcount > 2;
		}

		// This is the first time the user has executed the mode recently so its fine.
		prevcount = 0;
		prevtime = ServerInstance->Time();
		prevuuid = user->uuid;
		return false;
	}

public:
	// Whether the mode has recently been changed.
	bool active = false;

	CloakMode(Module* Creator, CloakAPI& api)
		: ModeHandler(Creator, "cloak", 'x', PARAM_NONE, MODETYPE_USER)
		, cloakapi(api)
	{
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		// For remote users blindly allow this
		LocalUser* user = IS_LOCAL(dest);
		if (!user)
		{
			// Remote setters broadcast mode before host while local setters do the opposite.
			active = IS_LOCAL(source) ? change.adding : !change.adding;
			dest->SetMode(this, change.adding);
			return true;
		}

		// Don't allow the mode change if its a no-op or a spam change.
		if (change.adding == user->IsModeSet(this) || CheckSpam(user))
			return false;

		// Penalise changing the mode to avoid spam.
		if (source == dest && !cloakapi.recloaking)
			user->CommandFloodPenalty += 5'000;

		if (!change.adding)
		{
			// Remove the mode and restore their real host.
			user->SetMode(this, false);
			if (!cloakapi.recloaking)
				user->ChangeDisplayedHost(user->GetRealHost());
			return true;
		}

		// If a user is not fully connected and their displayed hostname is
		// different to their real hostname they probably had a vhost set on
		// them by services. We should avoid automatically setting cloak on
		// them in this case.
		if (!user->IsFullyConnected() && user->GetRealHost() != user->GetDisplayedHost())
			return false;

		auto* cloaks = cloakapi.GetCloaks(user);
		if (cloaks)
		{
			// We were able to generate cloaks for this user.
			user->ChangeDisplayedHost(cloaks->front());
			user->SetMode(this, true);
			return true;
		}
		return false;
	}
};

class ModuleCloak final
	: public Module
{
private:
	CloakAPI cloakapi;
	CloakMethodList cloakmethods;
	CommandCloak cloakcmd;
	CloakMode cloakmode;

	void DisableMode(User* user)
	{
		user->SetMode(cloakmode, false);

		auto* luser = IS_LOCAL(user);
		if (luser)
		{
			Modes::ChangeList changelist;
			changelist.push_remove(&cloakmode);
			ClientProtocol::Events::Mode modeevent(ServerInstance->FakeClient, nullptr, luser, changelist);
			luser->Send(modeevent);
		}
	}

public:
	ModuleCloak()
		: Module(VF_VENDOR | VF_COMMON, "Adds user mode x (cloak) which allows user hostnames to be hidden.")
		, cloakapi(this, cloakmethods, &cloakmode)
		, cloakcmd(this, cloakmethods)
		, cloakmode(this, cloakapi)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		auto tags = ServerInstance->Config->ConfTags("cloak");
		if (tags.empty())
			throw ModuleException(this, "You have loaded the cloak module but not configured any <cloak> tags!");

		bool primary = true;
		CloakMethodList newcloakmethods;
		for (const auto& [_, tag] : tags)
		{
			const std::string method = tag->getString("method");
			if (method.empty())
				throw ModuleException(this, "<cloak:method> must be set to the name of a cloak engine, at " + tag->source.str());

			auto* service = ServerInstance->Modules.FindDataService<Cloak::Engine>("cloak/" + method);
			if (!service)
				throw ModuleException(this, "<cloak> tag was set to non-existent cloak method \"" + method + "\", at " + tag->source.str());

			newcloakmethods.push_back(service->Create(tag, primary));
			primary = false;
		}

		// The cloak configuration was valid so we can apply it.
		cloakmethods.swap(newcloakmethods);
	}

	void CompareLinkData(const LinkData& otherdata, LinkDataDiff& diffs) override
	{
		LinkData data;
		this->GetLinkData(data);

		// If the only difference is the method then just include that.
		insp::map::difference(data, otherdata, diffs);
		auto it = diffs.find("method");
		if (it != diffs.end())
			diffs = { *it };
	}

	void GetLinkData(LinkData& data) override
	{
		if (cloakmethods.empty())
			return;

		Cloak::MethodPtr cloakmethod;
		for (const auto& method : cloakmethods)
		{
			if (method->IsLinkSensitive())
			{
				// This cloak method really wants to be the link method so prefer it.
				cloakmethod = method;
				break;
			}
		}

		// If no methods are link sensitive we just use the first.
		if (!cloakmethod)
			cloakmethod = cloakmethods.front();

		cloakmethod->GetLinkData(data);
		data["method"] = cloakmethod->GetName();
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnCheckBan, PRIORITY_LAST);
	}

	void OnChangeHost(User* user, const std::string& host) override
	{
		if (user->IsModeSet(cloakmode) && !cloakmode.active)
			DisableMode(user);

		cloakmode.active = false;
	}

	void OnChangeRemoteAddress(LocalUser* user) override
	{
		// Remove the cloaks so we can generate new ones.
		cloakapi.ResetCloaks(user, false);

		// If a user is using a cloak then update it.
		auto* cloaks = cloakapi.GetCloaks(user);
		if (user->IsModeSet(cloakmode))
		{
			if (cloaks)
			{
				// The user has a new cloak list; pick the first.
				user->ChangeDisplayedHost(cloaks->front());
			}
			else
			{
				// The user has no cloak list; unset mode and revert to the real host.
				DisableMode(user);
				user->ChangeDisplayedHost(user->GetRealHost());
			}
		}
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask) override
	{
		LocalUser* lu = IS_LOCAL(user);
		if (!lu)
			return MOD_RES_PASSTHRU; // We don't have cloaks for remote users.

		auto* cloaks = cloakapi.GetCloaks(lu);
		if (!cloaks)
			return MOD_RES_PASSTHRU; // No cloaks, nothing to check.

		// Check if they have a cloaked host but are not using it.
		for (const auto& cloak : *cloaks)
		{
			if (cloak == user->GetDisplayedHost())
				continue; // This is checked by the core.

			std::string cloakmask = user->nick + "!" + user->GetRealUser() + "@" + cloak;
			if (InspIRCd::Match(cloakmask, mask))
				return MOD_RES_DENY;

			cloakmask = user->nick + "!" + user->GetDisplayedUser() + "@" + cloak;
			if (InspIRCd::Match(cloakmask, mask))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void OnServiceDel(ServiceProvider& service) override
	{
		size_t methods = 0;
		for (auto it = cloakmethods.begin(); it != cloakmethods.end(); )
		{
			auto cloakmethod = *it;
			if (cloakmethod->IsProvidedBy(service))
			{
				it = cloakmethods.erase(it);
				methods++;
				continue;
			}
			it++;
		}

		if (methods)
		{
			ServerInstance->SNO.WriteGlobalSno('a', "The {} hash provider was unloaded; removing {} cloak methods until the next rehash.",
				service.name.substr(6), methods);
		}
	}

	void OnUserConnect(LocalUser* user) override
	{
		// Generate cloaks now if they do not already exist so opers can /CHECK
		// this user if need be.
		cloakapi.GetCloaks(user);
	}

	void OnPostChangeConnectClass(LocalUser* user, bool force) override
	{
		// Reset the cloaks so if the user is moving into a class with <connect usecloak="no"> they
		// will be decloaked.
		cloakapi.ResetCloaks(user, true);
	}
};

MODULE_INIT(ModuleCloak)
