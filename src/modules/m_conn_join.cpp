/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Forces users to join the specified channel(s) on connect */

class ModuleConnJoin : public Module
{
	private:
		std::string JoinChan;
		std::vector<std::string> Joinchans;


		int tokenize(const std::string &str, std::vector<std::string> &tokens)
		{
			// skip delimiters at beginning.
			std::string::size_type lastPos = str.find_first_not_of(",", 0);
			// find first "non-delimiter".
			std::string::size_type pos = str.find_first_of(",", lastPos);

			while (std::string::npos != pos || std::string::npos != lastPos)
			{
				// found a token, add it to the vector.
				tokens.push_back(str.substr(lastPos, pos - lastPos));
				// skip delimiters. Note the "not_of"
				lastPos = str.find_first_not_of(",", pos);
				// find next "non-delimiter"
				pos = str.find_first_of(",", lastPos);
			}
			return tokens.size();
		}

	public:
		ModuleConnJoin(InspIRCd* Me)
			: Module(Me)
		{
			OnRehash(NULL);
			Implementation eventlist[] = { I_OnPostConnect, I_OnRehash };
			ServerInstance->Modules->Attach(eventlist, this, 2);
		}

		void Prioritize()
		{
			ServerInstance->Modules->SetPriority(this, I_OnPostConnect, PRIORITY_LAST);
		}


		virtual void OnRehash(User* user)
		{
			ConfigReader* conf = new ConfigReader(ServerInstance);
			JoinChan = conf->ReadValue("autojoin", "channel", 0);
			Joinchans.clear();
			if (!JoinChan.empty())
				tokenize(JoinChan,Joinchans);
			delete conf;
		}

		virtual ~ModuleConnJoin()
		{
		}

		virtual Version GetVersion()
		{
			return Version("$Id$", VF_VENDOR,API_VERSION);
		}

		virtual void OnPostConnect(User* user)
		{
			if (!IS_LOCAL(user))
				return;

			for(std::vector<std::string>::iterator it = Joinchans.begin(); it != Joinchans.end(); it++)
				if (ServerInstance->IsChannel(it->c_str(), ServerInstance->Config->Limits.ChanMax))
					Channel::JoinUser(ServerInstance, user, it->c_str(), false, "", false, ServerInstance->Time());
		}

};


MODULE_INIT(ModuleConnJoin)
