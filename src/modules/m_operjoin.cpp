/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2004 Christopher Hall <typobox43@gmail.com>
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

/* $ModDesc: Forces opers to join the specified channel(s) on oper-up */

class ModuleOperjoin : public Module
{
	private:
		std::string operChan;
		std::vector<std::string> operChans;
		std::map<std::string, std::vector<std::string> > operTypeChans; // Channels specific to an oper type.
		bool override;

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
		ModuleOperjoin(InspIRCd* Me) : Module(Me)
		{
			OnRehash(NULL);
		Implementation eventlist[] = { I_OnPostOper, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		}


		virtual void OnRehash(User* user)
		{
			ConfigReader* conf = new ConfigReader(ServerInstance);

			operChan = conf->ReadValue("operjoin", "channel", 0);
			override = conf->ReadFlag("operjoin", "override", "0", 0);
			operChans.clear();
			if (!operChan.empty())
				tokenize(operChan,operChans);

			std::map<std::string, std::vector<std::string> >().swap(operTypeChans);

			int olines = conf->Enumerate("type");
			for (int index = 0; index < olines; ++index)
			{
				std::string chanList = conf->ReadValue("type", "autojoin", index);
				if (!chanList.empty())
				{
					tokenize(chanList, operTypeChans[conf->ReadValue("type", "name", index)]);
				}
			}

			delete conf;
		}

		virtual ~ModuleOperjoin()
		{
		}

		virtual Version GetVersion()
		{
			return Version("$Id$", VF_VENDOR, API_VERSION);
		}

		virtual void OnPostOper(User* user, const std::string &opertype, const std::string &opername)
		{
			if (!IS_LOCAL(user))
				return;

			for(std::vector<std::string>::iterator it = operChans.begin(); it != operChans.end(); it++)
				if (ServerInstance->IsChannel(it->c_str(), ServerInstance->Config->Limits.ChanMax))
					Channel::JoinUser(ServerInstance, user, it->c_str(), override, "", false, ServerInstance->Time());

			std::map<std::string, std::vector<std::string> >::iterator i = operTypeChans.find(user->oper);

			if (i != operTypeChans.end())
			{
				const std::vector<std::string>& list = i->second;
				for (std::vector<std::string>::const_iterator it = list.begin(); it != list.end(); ++it)
				{
					if (ServerInstance->IsChannel(it->c_str(), ServerInstance->Config->Limits.ChanMax))
					{
						Channel::JoinUser(ServerInstance, user, it->c_str(), override, "", false, ServerInstance->Time());
					}
				}
			}
		}

};

MODULE_INIT(ModuleOperjoin)
