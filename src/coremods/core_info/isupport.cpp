/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016-2020 Sadie Powell <sadie@witchery.services>
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
#include "core_info.h"

namespace
{
	void TokenDifference(ISupport::TokenMap& tokendiff, const ISupport::TokenMap& oldtokens, const ISupport::TokenMap& newtokens)
	{
		std::map<std::string, std::pair<std::optional<std::string>, std::optional<std::string>>, irc::insensitive_swo> changedtokens;
		stdalgo::map::difference(oldtokens, newtokens, changedtokens);
		for (auto& [name, values] : changedtokens)
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
	: isupportevprov(mod)
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
	for (std::string::const_iterator iter = value.begin(); iter != value.end(); ++iter)
	{
		// The value must be escaped if:
		//   (1) It is a banned character in an IRC <middle> parameter (NUL, LF, CR, SPACE).
		//   (2) It has special meaning within an ISUPPORT token (EQUALS, BACKSLASH).
		if (*iter == '\0' || *iter == '\n' || *iter == '\r' || *iter == ' ' || *iter == '=' || *iter == '\\')
			buffer.append(InspIRCd::Format("\\x%X", *iter));
		else
			buffer.push_back(*iter);
	}
}

void ISupportManager::Build()
{
	// Modules can add new tokens and also edit or remove existing tokens.
	ISupport::TokenMap tokens = {
		{ "AWAYLEN",     ConvToStr(ServerInstance->Config->Limits.MaxAway)  },
		{ "CASEMAPPING", ServerInstance->Config->CaseMapping                },
		{ "CHANNELLEN",  ConvToStr(ServerInstance->Config->Limits.ChanMax)  },
		{ "CHANTYPES",   "#"                                                },
		{ "HOSTLEN",     ConvToStr(ServerInstance->Config->Limits.MaxHost)  },
		{ "KICKLEN",     ConvToStr(ServerInstance->Config->Limits.MaxKick)  },
		{ "LINELEN",     ConvToStr(ServerInstance->Config->Limits.MaxLine)  },
		{ "MAXTARGETS",  ConvToStr(ServerInstance->Config->MaxTargets)      },
		{ "MODES",       ConvToStr(ServerInstance->Config->Limits.MaxModes) },
		{ "NETWORK",     ServerInstance->Config->Network                    },
		{ "NAMELEN",     ConvToStr(ServerInstance->Config->Limits.MaxReal)  },
		{ "NICKLEN",     ConvToStr(ServerInstance->Config->Limits.NickMax)  },
		{ "PREFIX",      ServerInstance->Modes.BuildPrefixes()              },
		{ "STATUSMSG",   ServerInstance->Modes.BuildPrefixes(false)         },
		{ "TOPICLEN",    ConvToStr(ServerInstance->Config->Limits.MaxTopic) },
		{ "USERLEN",     ConvToStr(ServerInstance->Config->Limits.IdentMax) },
	};
	isupportevprov.Call(&ISupport::EventListener::OnBuildISupport, tokens);

	// Transform the map into a list of numerics ready to be sent to clients.
	std::vector<Numeric::Numeric> numerics;
	BuildNumerics(tokens, numerics);

	// Extract the tokens which have been updated
	ISupport::TokenMap difftokens;
	TokenDifference(difftokens, cachedtokens, tokens);

	// Send the updated numerics to users.
	std::vector<Numeric::Numeric> diffnumerics;
	BuildNumerics(difftokens, diffnumerics);
	for (LocalUser* user : ServerInstance->Users.GetLocalUsers())
	{
		if (user->registered & REG_ALL)
		{
			for (auto&& diffnumeric : diffnumerics)
				user->WriteNumeric(diffnumeric);
		}
	}

	// Apply the new ISUPPORT values.
	std::swap(numerics, cachednumerics);
	std::swap(tokens, cachedtokens);
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

void ISupportManager::SendTo(LocalUser* user)
{
	for (auto&& cachednumeric : cachednumerics)
		user->WriteNumeric(cachednumeric);
}
