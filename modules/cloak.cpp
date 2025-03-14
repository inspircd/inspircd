/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023-2024 Sadie Powell <sadie@witchery.services>
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
			const auto cloak = cloakmethod->Cloak(parameters[0]);
			if (!cloak)
				continue;

			noterpl.SendIfCap(user, stdrplcap, this, "CLOAK_RESULT", parameters[0], cloak->ToString(), FMT::format("Cloak #{} for {} is {} (method: {})",
				++count, parameters[0], cloak->ToString(), cloakmethod->GetName()));
		}

		if (!count)
		{
			failrpl.SendIfCap(user, stdrplcap, this, "UNABLE_TO_CLOAK", parameters[0], FMT::format("There are no methods available for cloaking {}",
				parameters[0]));
		}

		return CmdResult::SUCCESS;
	}
};

class CloakExtItem final
	: public SimpleExtItem<Cloak::List>
{
public:
	CloakExtItem(Module *mod)
		: SimpleExtItem<Cloak::List>(mod, "cloaks", ExtensionType::USER)
	{
	}

	/** @copydoc ExtensionItem::FromInternal */
	void FromInternal(Extensible* container, const std::string& value) noexcept override
	{
		if (container->extype != this->extype)
			return;

		if (value.empty())
		{
			Unset(container, false);
			return;
		}

		auto* cloaks = new Cloak::List();
		irc::spacesepstream stream(value);
		for (std::string cloak; stream.GetToken(cloak); )
			cloaks->push_back(Cloak::Info::FromString(Percent::Decode(cloak)));

		if (cloaks->empty())
		{
			// The remote sent an empty list.
			delete cloaks;
			Unset(container, false);
		}
		else
		{
			// The remote sent a non-zero list.
			Set(container, cloaks, false);
		}
	}

	std::string ToHuman(const Extensible* container, void* item) const noexcept override
	{
		auto* cloaks = static_cast<Cloak::List*>(item);
		if (cloaks->empty())
			return {};

		std::string value;
		for (const auto& cloak : *cloaks)
			value.append(cloak.ToString()).push_back(' ');
		value.pop_back();

		return value;
	}

	std::string ToInternal(const Extensible* container, void* item) const noexcept override
	{
		auto* cloaks = static_cast<Cloak::List*>(item);
		if (cloaks->empty())
			return {};

		std::string value;
		for (const auto& cloak : *cloaks)
			value.append(Percent::Encode(cloak.ToString())).push_back(' ');
		value.pop_back();

		return value;
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
	CloakExtItem ext;

	std::optional<Cloak::Info> GetFrontCloak(LocalUser* user)
	{
		auto* cloaks = GetCloaks(user);
		return cloaks ? std::make_optional(cloaks->front()) : std::nullopt;
	}

public:
	bool recloaking = false;

	CloakAPI(Module* Creator, CloakMethodList& cm, ModeHandler* mh)
		: Cloak::APIBase(Creator)
		, cloakmethods(cm)
		, cloakmode(mh)
		, ext(Creator)
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
				const auto cloak = cloakmethod->Cloak(user);
				if (!cloak)
				{
					cloaks->push_back(*cloak);

					ServerInstance->Logs.Debug(MODNAME, "Cloaked {} ({}) [{}] as {} using the {} method.",
						user->uuid, user->GetAddress(), user->GetRealHost(), cloak->ToString(),
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

		const auto oldcloak = GetFrontCloak(user); // This should always be non-nullopt.
		ext.Unset(user);

		if (!resetdisplay || !user->IsModeSet(cloakmode))
			return; // Not resetting the display or not cloaked.

		const auto newcloak = GetFrontCloak(user);
		if (oldcloak.has_value() == newcloak.has_value() && *oldcloak == *newcloak)
			return; // New front cloak is the same.

		Modes::ChangeList changelist;
		changelist.push_remove(cloakmode);
		if (newcloak)
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
			{
				user->ChangeDisplayedUser(user->GetRealUser());
				user->ChangeDisplayedHost(user->GetRealHost());
			}
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
			auto &cloak = cloaks->front();
			if (cloak.username.empty())
				user->ChangeDisplayedUser(cloak.username);
			user->ChangeDisplayedHost(cloak.hostname);
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
				auto &cloak = cloaks->front();
				if (cloak.username.empty())
					user->ChangeDisplayedUser(cloak.username);
				user->ChangeDisplayedHost(cloak.hostname);
			}
			else
			{
				// The user has no cloak list; unset mode and revert to the real host.
				DisableMode(user);
				user->ChangeDisplayedUser(user->GetRealUser());
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
			if (cloak.hostname == user->GetDisplayedHost() && (cloak.username.empty() || cloak.username == user->GetDisplayedUser()))
				continue; // This is checked by the core.

			const auto &ruser = cloak.username.empty() ? user->GetRealUser() : cloak.username;
			const auto &duser = cloak.username.empty() ? user->GetDisplayedUser() : cloak.username;

			std::string cloakmask = FMT::format("{}!{}@{}", user->nick, ruser, cloak.hostname);
			if (InspIRCd::Match(cloakmask, mask))
				return MOD_RES_DENY;

			cloakmask = FMT::format("{}!{}@{}", user->nick, duser, cloak.hostname);
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
