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
	/**
	 * This is currently the neatest way we can build the initial ISUPPORT map. In
	 * the future we can use an initializer list here.
	 */
	ISupport::TokenMap tokens;

	tokens["AWAYLEN"] = ConvToStr(ServerInstance->Config->Limits.MaxAway);
	tokens["CASEMAPPING"] = ServerInstance->Config->CaseMapping;
	tokens["CHANMODES"] = ServerInstance->Modes.GiveModeList(MODETYPE_CHANNEL);
	tokens["CHANNELLEN"] = ConvToStr(ServerInstance->Config->Limits.ChanMax);
	tokens["CHANTYPES"] = "#";
	tokens["HOSTLEN"] = ConvToStr(ServerInstance->Config->Limits.MaxHost);
	tokens["KICKLEN"] = ConvToStr(ServerInstance->Config->Limits.MaxKick);
	tokens["LINELEN"] = ConvToStr(ServerInstance->Config->Limits.MaxLine);
	tokens["MAXTARGETS"] = ConvToStr(ServerInstance->Config->MaxTargets);
	tokens["MODES"] = ConvToStr(ServerInstance->Config->Limits.MaxModes);
	tokens["NETWORK"] = ServerInstance->Config->Network;
	tokens["NICKLEN"] = ConvToStr(ServerInstance->Config->Limits.NickMax);
	tokens["PREFIX"] = ServerInstance->Modes.BuildPrefixes();
	tokens["STATUSMSG"] = ServerInstance->Modes.BuildPrefixes(false);
	tokens["TOPICLEN"] = ConvToStr(ServerInstance->Config->Limits.MaxTopic);
	tokens["USERLEN"] = ConvToStr(ServerInstance->Config->Limits.IdentMax);

	// Modules can add new tokens and also edit or remove existing tokens
	FOREACH_MOD_CUSTOM(isupportevprov, ISupport::EventListener, OnBuildISupport, (tokens));

	// EXTBAN is a special case as we need to sort it and prepend a comma.
	ISupport::TokenMap::iterator extban = tokens.find("EXTBAN");
	if (extban != tokens.end())
	{
		std::sort(extban->second.begin(), extban->second.end());
		extban->second.insert(0, ",");
	}

	// Transform the map into a list of lines, ready to be sent to clients
	Numeric::Numeric numeric(RPL_ISUPPORT);
	unsigned int token_count = 0;
	cachedlines.clear();

	for (ISupport::TokenMap::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
	{
		numeric.push(it->first);
		std::string& token = numeric.GetParams().back();
		AppendValue(token, it->second);

		token_count++;

		if (token_count % 13 == 12 || it == --tokens.end())
		{
			// Reached maximum number of tokens for this line or the current token
			// is the last one; finalize the line and store it for later use
			numeric.push("are supported by this server");
			cachedlines.push_back(numeric);
			numeric.GetParams().clear();
		}
	}
}

void ISupportManager::SendTo(LocalUser* user)
{
	for (std::vector<Numeric::Numeric>::const_iterator i = cachedlines.begin(); i != cachedlines.end(); ++i)
		user->WriteNumeric(*i);
}
