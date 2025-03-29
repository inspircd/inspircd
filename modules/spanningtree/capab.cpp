/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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


#include <sstream>

#include "inspircd.h"
#include "dynamic.h"
#include "modules/extban.h"
#include "utility/map.h"

#include "treeserver.h"
#include "utils.h"
#include "link.h"
#include "main.h"

namespace
{
	// A map which holds the difference between local and remote tokens.
	typedef std::map<std::string, std::pair<std::optional<std::string>, std::optional<std::string>>, irc::insensitive_swo> TokenDiff;

	// Builds a list of the local modules with the specified property.
	CapabData::ModuleMap BuildModuleList(ModuleFlags property)
	{
		CapabData::ModuleMap modules;
		for (const auto& [name, module] : ServerInstance->Modules.GetModules())
		{
			if (!(module->properties & property))
				continue;

			// Replace m_foo.dylib with foo
			auto startpos = name.compare(0, 2, "m_", 2) ? 0 : 2;
			auto endpos = name.length() - strlen(DLL_EXTENSION);

			auto modname = name.substr(startpos, endpos - startpos);
			modules[modname] = SpanningTreeUtilities::BuildLinkString(module);
		}
		return modules;
	}

	// Compares the module data sent by a remote server to that of the local server.
	bool CompareModuleData(Module* mod, const Module::LinkData& otherdata, std::ostringstream& diffconfig)
	{
		Module::LinkDataDiff datadiff;
		mod->CompareLinkData(otherdata, datadiff);
		if (!datadiff.empty())
		{
			diffconfig << ' ' << ModuleManager::ShrinkModName(mod->ModuleFile) << " (";
			bool first = true;
			for (const auto& [key, values] : datadiff)
			{
				// Keys are separated by commas.
				if (!first)
					diffconfig << ", ";
				first = false;

				diffconfig << key;
				if (values.first && values.second)
				{
					// Exists on both but with a different value.
					diffconfig << " set to " <<  *values.first << " here and " << *values.second << " there";
				}
				else if (values.first && !values.second)
				{
					// Only exists on the local server.
					diffconfig << " only set here";
				}
				else if (!values.first && values.second)
				{
					// Only exists on the remote server.
					diffconfig << " only set there";
				}
			}

			diffconfig << ')';
			return false;
		}

		return true;
	}

	// Compares the lists of module on a remote server to the local server.
	bool CompareModules(ModuleFlags property, std::optional<CapabData::ModuleMap>& remote,
		std::ostringstream& out)
	{
		// If the remote didn't send a module list then don't compare.
		if (!remote)
			return true;

		// Retrieve the local module list.
		bool okay = true;
		ModuleManager::ModuleMap local;
		for (const auto& [name, module] : ServerInstance->Modules.GetModules())
		{
			if (module->properties & property)
				local[ModuleManager::ShrinkModName(name)] = module;
		}

		std::ostringstream diffconfig;
		std::ostringstream localmissing;
		std::ostringstream remotemissing;
		for (const auto& [name, linkdata] : *remote)
		{
			auto moditer = local.find(name);
			if (moditer == local.end())
			{
				// Only exists on the remote server.
				localmissing << ' ' << name;
				okay = false;
				continue;
			}

			// Parse the remote link data.
			Module::LinkData otherdata;
			irc::sepstream datastream(linkdata, '&');
			for (std::string datapair; datastream.GetToken(datapair); )
			{
				size_t split = datapair.find('=');
				if (split == std::string::npos)
					otherdata.emplace(datapair, "");
				else
					otherdata.emplace(datapair.substr(0, split), Percent::Decode(datapair.substr(split + 1)));
			}

			// Compare the link data.
			if (!CompareModuleData(moditer->second, otherdata, diffconfig))
				okay = false;
			local.erase(moditer);
		}

		for (const auto& [name, _] : local)
		{
			// Only exists on the local server.
			remotemissing << ' ' << name;
			okay = false;
		}

		if (!diffconfig.str().empty())
			out << " Loaded on both with different config:" << diffconfig.str() << '.';
		if (!localmissing.str().empty())
			out << " Not loaded on the local server:" << localmissing.str() << '.';
		if (!remotemissing.str().empty())
			out << " Not loaded on the remote server:" << remotemissing.str() << '.';

		return okay;
	}

	// Generates a capability list in the format "FOO=BAR BAZ=BAX".
	std::string FormatCapabilities(TreeSocket* ts)
	{
		std::unordered_map<std::string, std::string> capabilities = {
			{ "CASEMAPPING", ServerInstance->Config->CaseMapping                  },
			{ "MAXAWAY",     ConvToStr(ServerInstance->Config->Limits.MaxAway)    },
			{ "MAXCHANNEL",  ConvToStr(ServerInstance->Config->Limits.MaxChannel) },
			{ "MAXHOST",     ConvToStr(ServerInstance->Config->Limits.MaxHost)    },
			{ "MAXKICK",     ConvToStr(ServerInstance->Config->Limits.MaxKick)    },
			{ "MAXLINE",     ConvToStr(ServerInstance->Config->Limits.MaxLine)    },
			{ "MAXMODES",    ConvToStr(ServerInstance->Config->Limits.MaxModes)   },
			{ "MAXNICK",     ConvToStr(ServerInstance->Config->Limits.MaxNick)    },
			{ "MAXQUIT",     ConvToStr(ServerInstance->Config->Limits.MaxQuit)    },
			{ "MAXREAL",     ConvToStr(ServerInstance->Config->Limits.MaxReal)    },
			{ "MAXTOPIC",    ConvToStr(ServerInstance->Config->Limits.MaxTopic)   },
			{ "MAXUSER",     ConvToStr(ServerInstance->Config->Limits.MaxUser)    },
		};

		// If SHA256 hashing support is available then send a challenge token.
		if (ServerInstance->Modules.FindService(SERVICE_DATA, "hash/sha256"))
		{
			ts->SetOurChallenge(ServerInstance->GenRandomStr(20));
			capabilities["CHALLENGE"] = ts->GetOurChallenge();
		}

		ExtBan::ManagerRef extbanmgr(Utils->Creator);
		if (extbanmgr)
		{
			std::string& xbformat = capabilities["EXTBANFORMAT"];
			switch (extbanmgr->GetFormat())
			{
				case ExtBan::Format::ANY:
					xbformat = "any";
					break;
				case ExtBan::Format::NAME:
					xbformat = "name";
					break;
				case ExtBan::Format::LETTER:
					xbformat = "letter";
					break;
			}
		}

		auto first = true;
		std::stringstream capabilitystr;
		for (const auto& [capkey, capvalue] : capabilities)
		{
			if (!first)
				capabilitystr << ' ';
			capabilitystr << capkey << '=' << capvalue;
			first = false;
		}
		return capabilitystr.str();
	}

	// Generates a module list in the format "m_foo.so=bar m_bar.so=baz".
	std::string FormatModules(ModuleFlags property)
	{
		std::ostringstream modules;
		CapabData::ModuleMap mymodules = BuildModuleList(property);
		for (const auto& [module, linkdata] : mymodules)
		{
			modules << module;
			if (!linkdata.empty())
				modules << '=' << linkdata;
			modules << ' ';
		}
		return modules.str();
	}

	// Parses a module list in the format "m_foo.so=bar m_bar.so=baz" to a map.
	void ParseModules(const std::string& modlist, std::optional<CapabData::ModuleMap>& out)
	{
		CapabData::ModuleMap& map = out ? *out : out.emplace();
		irc::spacesepstream modstream(modlist);
		for (std::string mod; modstream.GetToken(mod); )
		{
			size_t split = mod.find('=');
			if (split == std::string::npos)
				map.emplace(mod, "");
			else
				map.emplace(mod.substr(0, split), mod.substr(split + 1));
		}
	}
}

std::string TreeSocket::BuildModeList(ModeType mtype)
{
	std::vector<std::string> modes;
	for (const auto& [_, mh] : ServerInstance->Modes.GetModes(mtype))
	{
		const PrefixMode* const pm = mh->IsPrefixMode();
		std::string mdesc;
		if (pm)
			mdesc.append("prefix:").append(ConvToStr(pm->GetPrefixRank())).push_back(':');
		else if (mh->IsListMode())
			mdesc.append("list:");
		else if (mh->NeedsParam(true))
			mdesc.append(mh->NeedsParam(false) ? "param:" : "param-set:");
		else
			mdesc.append("simple:");
		mdesc.append(mh->name);
		mdesc.push_back('=');
		if (pm)
		{
			if (pm->GetPrefix())
				mdesc.push_back(pm->GetPrefix());
		}
		mdesc.push_back(mh->GetModeChar());
		modes.push_back(mdesc);
	}
	std::sort(modes.begin(), modes.end());
	return insp::join(modes);
}

bool TreeSocket::BuildExtBanList(std::string& out)
{
	ExtBan::ManagerRef extbanmgr(Utils->Creator);
	if (!extbanmgr)
		return false;

	const auto& extbans = extbanmgr->GetNameMap();
	for (auto iter = extbans.begin(); iter != extbans.end(); ++iter)
	{
		if (iter != extbans.begin())
			out.push_back(' ');

		const ExtBan::Base* extban = iter->second;
		switch (extban->GetType())
		{
			case ExtBan::Type::ACTING:
				out.append("acting:");
				break;
			case ExtBan::Type::MATCHING:
				out.append("matching:");
				break;
		}

		out.append(extban->GetName());
		if (extban->GetLetter())
			out.append("=").push_back(extban->GetLetter());
	}
	return true;
}

void TreeSocket::SendCapabilities(int phase)
{
	if (capab->capab_phase >= phase)
		return;

	if (capab->capab_phase < 1 && phase >= 1)
		WriteLine(FMT::format("CAPAB START {}", (uint16_t)PROTO_NEWEST));

	capab->capab_phase = phase;
	if (phase < 2)
		return;

	WriteLine("CAPAB CAPABILITIES :" + FormatCapabilities(this));
	WriteLine("CAPAB MODULES :" + FormatModules(VF_COMMON));
	WriteLine("CAPAB MODSUPPORT :" + FormatModules(VF_OPTCOMMON));
	WriteLine("CAPAB CHANMODES :" + BuildModeList(MODETYPE_CHANNEL));
	WriteLine("CAPAB USERMODES :" + BuildModeList(MODETYPE_USER));

	std::string extbans;
	if (BuildExtBanList(extbans))
		WriteLine("CAPAB EXTBANS :" + extbans);

	this->WriteLine("CAPAB END");
}

/* Isolate and return the elements that are different between two comma separated lists */
void TreeSocket::ListDifference(const std::string& one, const std::string& two, char sep,
		std::string& mleft, std::string& mright)
{
	std::set<std::string> values;
	irc::sepstream sepleft(one, sep);
	irc::sepstream sepright(two, sep);
	std::string item;
	while (sepleft.GetToken(item))
	{
		values.insert(item);
	}
	while (sepright.GetToken(item))
	{
		if (!values.erase(item))
		{
			mright.push_back(sep);
			mright.append(item);
		}
	}
	for (const auto& value : values)
	{
		mleft.push_back(sep);
		mleft.append(value);
	}
}

bool TreeSocket::Capab(const CommandBase::Params& params)
{
	if (params.empty())
	{
		this->SendError("Invalid number of parameters for CAPAB - Mismatched version");
		return false;
	}
	if (irc::equals(params[0], "START"))
	{
		capab->requiredmodules.reset();
		capab->optionalmodules.reset();
		capab->CapKeys.clear();

		if (params.size() > 1)
			proto_version = ConvToNum<uint16_t>(params[1]);

		if (proto_version < PROTO_OLDEST)
		{
			const auto proto_str = proto_version ? ConvToStr(proto_version) : "1201 or older";
			SendError(FMT::format("CAPAB negotiation failed. Server is using protocol version {} which"
				" is too old to link with this server (protocol versions {} to {} are supported)",
				proto_str, (uint16_t)PROTO_OLDEST, (uint16_t)PROTO_NEWEST));
			return false;
		}

		SendCapabilities(2);
	}
	else if (irc::equals(params[0], "END"))
	{
		std::ostringstream errormsg;
		if (!CompareModules(VF_COMMON, this->capab->requiredmodules, errormsg))
		{
			SendError("CAPAB negotiation failed. Required modules incorrectly matched on these servers."
				+ errormsg.str());
			return false;
		}
		else if (!CompareModules(VF_OPTCOMMON, this->capab->optionalmodules, errormsg))
		{
			if (Utils->AllowOptCommon)
			{
				ServerInstance->SNO.WriteToSnoMask('l', "Optional modules do not match. Some features may not work globally!"
					+ errormsg.str());
			}
			else
			{
				SendError("CAPAB negotiation failed. Optional modules incorrectly matched on these servers and <options:allowmismatch> is not enabled."
					+ errormsg.str());
				return false;
			}
		}

		if (!capab->ChanModes.empty())
		{
			if (capab->ChanModes != BuildModeList(MODETYPE_CHANNEL))
			{
				std::string diffIneed;
				std::string diffUneed;
				ListDifference(capab->ChanModes, BuildModeList(MODETYPE_CHANNEL), ' ', diffIneed, diffUneed);
				if (diffIneed.length() || diffUneed.length())
				{
					std::string reason = "Channel modes not matched on these servers.";
					if (diffIneed.length())
						reason += " Not loaded here:" + diffIneed;
					if (diffUneed.length())
						reason += " Not loaded there:" + diffUneed;
					this->SendError("CAPAB negotiation failed: " + reason);
				}
			}
		}

		if (!capab->UserModes.empty())
		{
			if (capab->UserModes != BuildModeList(MODETYPE_USER))
			{
				std::string diffIneed;
				std::string diffUneed;
				ListDifference(capab->UserModes, BuildModeList(MODETYPE_USER), ' ', diffIneed, diffUneed);
				if (diffIneed.length() || diffUneed.length())
				{
					std::string reason = "User modes not matched on these servers.";
					if (diffIneed.length())
						reason += " Not loaded here:" + diffIneed;
					if (diffUneed.length())
						reason += " Not loaded there:" + diffUneed;
					this->SendError("CAPAB negotiation failed: " + reason);
				}
			}
		}

		if (!capab->ExtBans.empty())
		{
			std::string myextbans;
			if (BuildExtBanList(myextbans))
			{
				std::string missing_here;
				std::string missing_there;
				ListDifference(capab->ExtBans, myextbans, ' ', missing_here, missing_there);
				if (!missing_here.empty() || !missing_there.empty())
				{
					if (Utils->AllowOptCommon)
					{
						ServerInstance->SNO.WriteToSnoMask('l',
							"ExtBan lists do not match, some bans/exemptions may not work globally.{}{}{}{}",
							missing_here.length() ? " Not loaded here:" : "", missing_here,
							missing_there.length() ? " Not loaded there:" : "", missing_there);
					}
					else
					{
						std::string reason = "ExtBans not matched on these servers.";
						if (missing_here.length())
							reason += " Not loaded here:" + missing_here;
						if (missing_there.length())
							reason += " Not loaded there:" + missing_there;
						this->SendError("CAPAB negotiation failed: " + reason);
						return false;
					}
				}
			}
		}

		if (this->capab->CapKeys.find("CASEMAPPING") != this->capab->CapKeys.end())
		{
			const std::string casemapping = this->capab->CapKeys.find("CASEMAPPING")->second;
			if (casemapping != ServerInstance->Config->CaseMapping)
			{
				std::string reason = "The casemapping of the remote server differs to that of the local server."
					" Local casemapping: " + ServerInstance->Config->CaseMapping +
					" Remote casemapping: " + casemapping;
				this->SendError("CAPAB negotiation failed: " + reason);
				return false;
			}
		}

		/* Challenge response, store their challenge for our password */
		std::map<std::string, std::string>::iterator n = this->capab->CapKeys.find("CHALLENGE");
		if ((n != this->capab->CapKeys.end()) && (ServerInstance->Modules.FindService(SERVICE_DATA, "hash/sha256")))
		{
			/* Challenge-response is on now */
			this->SetTheirChallenge(n->second);
			if (!this->GetTheirChallenge().empty() && (this->LinkState == CONNECTING))
			{
				this->SendCapabilities(2);
				this->WriteLine(FMT::format("SERVER {} {} {} :{}",
					ServerInstance->Config->ServerName,
					TreeSocket::MakePass(capab->link->SendPass, capab->theirchallenge),
					ServerInstance->Config->ServerId,
					ServerInstance->Config->ServerDesc
				));
			}
		}
		else
		{
			// They didn't specify a challenge or we don't have sha256, we use plaintext
			if (this->LinkState == CONNECTING)
			{
				this->SendCapabilities(2);
				this->WriteLine(FMT::format("SERVER {} {} {} :{}",
					ServerInstance->Config->ServerName,
					capab->link->SendPass,
					ServerInstance->Config->ServerId,
					ServerInstance->Config->ServerDesc
				));
			}
		}
	}
	else if (irc::equals(params[0] , "MODULES"))
	{
		if (params.size() >= 2)
			ParseModules(params[1], capab->requiredmodules);
	}
	else if (irc::equals(params[0], "MODSUPPORT"))
	{
		if (params.size() >= 2)
			ParseModules(params[1], capab->optionalmodules);
	}
	else if (irc::equals(params[0], "CHANMODES") && (params.size() == 2))
	{
		capab->ChanModes = params[1];
	}
	else if (irc::equals(params[0], "USERMODES") && (params.size() == 2))
	{
		capab->UserModes = params[1];
	}
	else if (irc::equals(params[0], "EXTBANS") && (params.size() == 2))
	{
		capab->ExtBans = params[1];
	}
	else if (irc::equals(params[0], "CAPABILITIES") && (params.size() == 2))
	{
		irc::spacesepstream capabs(params[1]);
		std::string item;
		while (capabs.GetToken(item))
		{
			/* Process each key/value pair */
			std::string::size_type equals = item.find('=');
			if (equals != std::string::npos)
			{
				std::string var(item, 0, equals);
				std::string value(item, equals+1);
				capab->CapKeys[var] = value;
			}
		}
	}
	return true;
}
