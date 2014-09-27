/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Googolplexed <googol@googolplexed.net>
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
#include "caller.h"

class ModuleNationalCodePage : public Module,public HandlerBase1<bool, const std::string&>
{
	caller1<bool, const std::string&> rememberer;
	const unsigned char * lowermap_rememberer;
	std::bitset<256> m_allowedchar;
	unsigned char m_casemap[256];
	std::string charset,casemapping;

public:
	ModuleNationalCodePage()
		: rememberer(ServerInstance->IsNick), lowermap_rememberer(national_case_insensitive_map) {
	}

	//Check if Nick is valid using the current codepage
	bool Call(const std::string& nick)
	{
		for(std::string::const_iterator it = nick.begin(); it != nick.end(); ++it)
		{
			//Check if character in normal range
			if ((*it >= 'A') && (*it <= '}'))
				continue;

			//Check if character is okay, but needs to be after the initial
			if ((((*it >= '0') && (*it <= '9')) || (*it == '-')) && (it != nick.begin()))
				continue;

			//std::string's iterator returns signed chars.
			//codepoints are handled unsigned
			//Check if character is in the allowed range
			if(m_allowedchar[(unsigned char) *it] == true)
				continue;

			return false; //Uncaught character, not valid
		}
		return true;
	}

	void init() CXX11_OVERRIDE
	{
		national_case_insensitive_map = m_casemap;
		ServerInstance->IsNick = this;
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["CASEMAPPING"] = charset;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		//Copy the existing generic ASCII casemap into place
		memcpy(m_casemap, rfc_case_insensitive_map, 256);
		m_allowedchar.reset();
		ConfigTag* nationalcodepage = ServerInstance->Config->ConfValue("nationalcodepage");
		charset = nationalcodepage->getString("codepage");

		//Use InspIRCd config engine to read tags for characters in decimal
		//Split a range of codepoints in decimal form
		ConfigTagList chars = ServerInstance->Config->ConfTags("cpchars");
		for (ConfigIter i = chars.first; i != chars.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string range = tag->getString("range");
			irc::portparser coderange(range, false);
			int j=-1;
			while(0 != (j = coderange.GetToken()))
			{
				//For each codepoint, add it to the allowed list
				m_allowedchar[j] = true;
			}
		}
		//Load in Upper->lowercase conversions.
		ConfigTagList casemap = ServerInstance->Config->ConfTags("casemap");
		for (ConfigIter i = casemap.first; i != casemap.second; ++i)
		{
			ConfigTag* tag = i->second;
			unsigned char lower = (unsigned char) tag->getInt("lower",0,1,256);
			unsigned char upper = (unsigned char) tag->getInt("upper",0,1,256);
			m_casemap[upper] = lower;
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("8bit national character codepage support", VF_VENDOR|VF_COMMON);
	}

	~ModuleNationalCodePage()
	{
		ServerInstance->IsNick = rememberer;
		national_case_insensitive_map = lowermap_rememberer;
	}

};
MODULE_INIT(ModuleNationalCodePage)
