/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2025 Sadie Powell <sadie@witchery.services>
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
#include "utility/map.h"

#include "core_info.h"

namespace
{
	void TokenDifference(ISupport::TokenMap& tokendiff, const ISupport::TokenMap& oldtokens, const ISupport::TokenMap& newtokens)
	{
		std::map<std::string, std::pair<std::optional<std::string>, std::optional<std::string>>, irc::insensitive_swo> changedtokens;
		insp::map::difference(oldtokens, newtokens, changedtokens);
		for (const auto& [name, values] : changedtokens)
		{
			if (values.first && !values.second)
			{
				// The token was removed.
				tokendiff["-" + name];
			}
			else if (values.second)
			{
				// The token was added or its value changed.
				tokendiff[name] = *values.second;
			}
		}
	}
}

ISupportManager::ISupportManager(Module* mod)
	: operext(mod, "isupport", ExtensionType::USER)
	, isupportevprov(mod)
{
}

void ISupportManager::AppendValue(std::string& buffer, const std::string& value)
{
	// If this token has no value then we have nothing to do.
	if (value.empty())
		return;

	// This function implements value escaping according to the rules of the ISUPPORT draft:
	// https://tools.ietf.org/html/draft-brocklesby-irc-isupport-03
	buffer.push_back('=');
	for (const auto chr : value)
	{
		// The value must be escaped if:
		//   (1) It is a banned character in an IRC <middle> parameter (NUL, LF, CR, SPACE).
		//   (2) It has special meaning within an ISUPPORT token (EQUALS, BACKSLASH).
		if (chr == '\0' || chr == '\n' || chr == '\r' || chr == ' ' || chr == '=' || chr == '\\')
			buffer.append(FMT::format("\\x{:02X}", chr));
		else
			buffer.push_back(chr);
	}
}

void ISupportManager::BuildClass(ISupport::TokenMap& newtokens, NumericList& newnumerics,
	NumericList& diffnumerics, const std::shared_ptr<ConnectClass> &klass, bool dead)
{
	ServerInstance->Logs.Debug(MODNAME, "Rebuilding isupport for class {}{}",
		klass->GetName(), dead ? " (dead)" : "");

	isupportevprov.Call(&ISupport::EventListener::OnBuildClassISupport, klass, newtokens);

	// Transform the map into a list of numerics ready to be sent to clients.
	BuildNumerics(newtokens, newnumerics);

	// Extract the tokens which have been updated.
	auto oldtokens = cachedtokens.find(klass);
	if (oldtokens != cachedtokens.end())
	{
		// Build the updated numeric diff to send to to existing users.
		ISupport::TokenMap difftokens;
		TokenDifference(difftokens, oldtokens->second, newtokens);
		BuildNumerics(difftokens, diffnumerics);
	}
}

void ISupportManager::BuildOper(ISupport::TokenMap& newtokens, NumericList& newnumerics,
	NumericList& diffnumerics, LocalUser* user)
{
	ServerInstance->Logs.Debug(MODNAME, "Rebuilding isupport for {}oper {}",
		user->IsOper() ? "" : "ex-", user->nick);

	auto classtokens = cachedtokens.find(user->GetClass());
	if (classtokens == cachedtokens.end())
		return; // Should never happen.

	newtokens = classtokens->second;
	if (user->IsOper())
		isupportevprov.Call(&ISupport::EventListener::OnBuildOperISupport, user, newtokens);

	// Extract the old tokens (falling back to the class tokens).
	const auto* ext = operext.Get(user);
	const auto& oldtokens = ext ? ext->first : classtokens->second;

	// Build the updated numeric diff to send to to the user.
	ISupport::TokenMap difftokens;
	TokenDifference(difftokens, oldtokens, newtokens);
	BuildNumerics(difftokens, diffnumerics);
}

void ISupportManager::Build()
{
	// Modules can add new tokens and also edit or remove existing tokens.
	ISupport::TokenMap tokens = {
		{ "AWAYLEN",     ConvToStr(ServerInstance->Config->Limits.MaxAway)    },
		{ "CASEMAPPING", ServerInstance->Config->CaseMapping                  },
		{ "CHANNELLEN",  ConvToStr(ServerInstance->Config->Limits.MaxChannel) },
		{ "CHANTYPES",   "#"                                                  },
		{ "HOSTLEN",     ConvToStr(ServerInstance->Config->Limits.MaxHost)    },
		{ "KICKLEN",     ConvToStr(ServerInstance->Config->Limits.MaxKick)    },
		{ "LINELEN",     ConvToStr(ServerInstance->Config->Limits.MaxLine)    },
		{ "MODES",       ConvToStr(ServerInstance->Config->Limits.MaxModes)   },
		{ "NETWORK",     ServerInstance->Config->Network                      },
		{ "NAMELEN",     ConvToStr(ServerInstance->Config->Limits.MaxReal)    },
		{ "NICKLEN",     ConvToStr(ServerInstance->Config->Limits.MaxNick)    },
		{ "TOPICLEN",    ConvToStr(ServerInstance->Config->Limits.MaxTopic)   },
		{ "USERLEN",     ConvToStr(ServerInstance->Config->Limits.MaxUser)    },
	};
	isupportevprov.Call(&ISupport::EventListener::OnBuildISupport, tokens);

	NumericMap diffnumerics;
	NumericMap newnumerics;
	TokenMap newtokens;
	for (const auto& klass : ServerInstance->Config->Classes)
	{
		newtokens[klass] = tokens;
		BuildClass(newtokens[klass], newnumerics[klass], diffnumerics[klass], klass, false);
	}

	// Send out the numeric diffs.
	if (!diffnumerics.empty())
	{
		for (LocalUser* user : ServerInstance->Users.GetLocalUsers())
		{
			const auto& klass = user->GetClass();
			if (!(user->connected & User::CONN_FULL))
				continue; // User hasn't received 005 yet.

			auto numerics = diffnumerics.find(klass);
			if (numerics == diffnumerics.end())
			{
				// The user is in a class which has been removed from the server
				// config; we need to build a class for them.
				newtokens[klass] = tokens;
				BuildClass(newtokens[klass], newnumerics[klass], diffnumerics[klass], klass, true);
				numerics = diffnumerics.find(klass);
			}

			if (user->IsOper())
				continue; // Server operators are handled later.

			for (const auto& numeric : numerics->second)
				user->WriteNumeric(numeric);
		}
	}

	// Apply the new ISUPPORT values.
	cachednumerics.swap(newnumerics);
	cachedtokens.swap(newtokens);

	// Notify operators of the change.
	for (auto* oper : ServerInstance->Users.all_opers)
	{
		auto* loper = IS_LOCAL(oper);
		if (loper)
			SendOper(loper);
	}
}

void ISupportManager::BuildNumerics(ISupport::TokenMap& tokens, std::vector<Numeric::Numeric>& numerics)
{
	Numeric::Numeric numeric(RPL_ISUPPORT);
	for (ISupport::TokenMap::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
	{
		numeric.push(it->first);
		std::string& token = numeric.GetParams().back();
		AppendValue(token, it->second);

		if (numeric.GetParams().size() == 12 || std::distance(it, tokens.cend()) == 1)
		{
			// Reached maximum number of tokens for this line or the current token
			// is the last one; finalize the line and store it for later use.
			numeric.push("are supported by this server");
			numerics.push_back(numeric);
			numeric.GetParams().clear();
		}
	}
}

void ISupportManager::ChangeClass(LocalUser* user, const std::shared_ptr<ConnectClass>& oldclass, const std::shared_ptr<ConnectClass>& newclass)
{
	auto oldtokens = cachedtokens.find(oldclass);
	auto newtokens = cachedtokens.find(newclass);
	if (oldtokens == cachedtokens.end() || newtokens == cachedtokens.end())
		return; // Should never happen.

	ISupport::TokenMap difftokens;
	if (user->IsOper())
	{
		// Fetch the new oper tokens.
		auto newopertokens = newtokens->second;
		isupportevprov.Call(&ISupport::EventListener::OnBuildOperISupport, user, newopertokens);

		// Diff with the old tokens.
		const auto& oldopertokens = operext.Get(user)->first;
		TokenDifference(difftokens, oldopertokens, newopertokens);

		// Build the numerics and store for later.
		NumericList newopernumerics;
		BuildNumerics(newopertokens, newopernumerics);
		operext.Set(user, std::make_pair(newopertokens, newopernumerics));
	}
	else
		TokenDifference(difftokens, oldtokens->second, newtokens->second);

	std::vector<Numeric::Numeric> diffnumerics;
	BuildNumerics(difftokens, diffnumerics);

	for (const auto& numeric : diffnumerics)
		user->WriteNumeric(numeric);
}

void ISupportManager::SendTo(LocalUser* user)
{
	NumericList* numerics;
	if (user->IsOper())
	{
		auto* ext = operext.Get(user);
		numerics = (ext ? &ext->second : nullptr);
	}
	else
	{
		auto it = cachednumerics.find(user->GetClass());
		numerics = (it == cachednumerics.end() ? nullptr : &it->second);
	}

	if (!numerics)
		return; // Should never happen.

	for (const auto& numeric : *numerics)
		user->WriteNumeric(numeric);
}

void ISupportManager::SendOper(LocalUser* user)
{
	NumericList diffnumerics;
	NumericList newnumerics;
	ISupport::TokenMap newtokens;
	BuildOper(newtokens, newnumerics, diffnumerics, user);

	for (const auto& numeric : diffnumerics)
		user->WriteNumeric(numeric);

	// Apply the new ISUPPORT values. If the user is still an oper we need to
	// store the numerics for if the user executes /VERSION later. Otherwise,
	// we can just delete it.
	if (user->IsOper())
		operext.Set(user, std::make_pair(newtokens, newnumerics));
	else
		operext.Unset(user);
}
