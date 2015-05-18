/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Adam <Adam@anope.org>
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
#include "modules/ssl.h"

class ModuleChannelTypes : public Module
{
	enum { ERR_USERNOTONSERV = 504 };

	std::map<unsigned char, std::set<std::string> > prefixinfos;

 public:
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTagList tags = ServerInstance->Config->ConfTags("chantype");

		for(ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;

			std::string prefix = tag->getString("prefix"),
				types = tag->getString("type");

			if (ServerInstance->Config->ChannelPrefixes.find(prefix) == std::string::npos)
				continue;

			irc::commasepstream sep = types;
			for (std::string type; sep.GetToken(type);)
				prefixinfos[prefix[0]].insert(type);
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements chanenl types", VF_VENDOR);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		std::map<unsigned char, std::set<std::string> >::iterator it = prefixinfos.find(cname[0]);
		if (it == prefixinfos.end())
			return MOD_RES_PASSTHRU;

		const std::set<std::string> &types = it->second;
		if (types.count("nocreate") && !user->IsOper() && !chan)
		{
			user->WriteNumeric(384, "%s :Only an IRC operator may create %s", cname.c_str(), cname.c_str());
			return MOD_RES_DENY;
		}
		if (types.count("oper") && !user->IsOper())
		{
			user->WriteNumeric(ERR_CANTJOINOPERSONLY, "%s :Only IRC operators may join %s", cname.c_str(), cname.c_str());
			return MOD_RES_DENY;
		}
		if (types.count("ssl") && !SSLClientCert::GetCertificate(&user->eh))
		{
			user->WriteNumeric(489, "%s :Cannot join channel; SSL users only", cname.c_str());
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleChannelTypes)
