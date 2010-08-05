/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Forces opers to join the specified channel(s) on oper-up */

class ModuleOperjoin : public Module
{
	private:
		std::string operChan;
		std::vector<std::string> operChans;
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
		void init()
		{
			OnRehash(NULL);
			Implementation eventlist[] = { I_OnPostOper, I_OnRehash };
			ServerInstance->Modules->Attach(eventlist, this, 2);
		}


		virtual void OnRehash(User* user)
		{
			ConfigTag* tag = ServerInstance->Config->ConfValue("operjoin");

			operChan = tag->getString("channel", 0);
			override = tag->getBool("override");
			operChans.clear();
			if (!operChan.empty())
				tokenize(operChan,operChans);
		}

		virtual ~ModuleOperjoin()
		{
		}

		virtual Version GetVersion()
		{
			return Version("Forces opers to join the specified channel(s) on oper-up", VF_VENDOR);
		}

		virtual void OnPostOper(User* user, const std::string &opertype, const std::string &opername)
		{
			if (!IS_LOCAL(user))
				return;

			for(std::vector<std::string>::iterator it = operChans.begin(); it != operChans.end(); it++)
				if (ServerInstance->IsChannel(it->c_str(), ServerInstance->Config->Limits.ChanMax))
					Channel::JoinUser(user, it->c_str(), override, "", false, ServerInstance->Time());

			std::string chanList = IS_OPER(user)->getConfig("autojoin");
			if (!chanList.empty())
			{
				std::vector<std::string> typechans;
				tokenize(chanList, typechans);
				for (std::vector<std::string>::const_iterator it = typechans.begin(); it != typechans.end(); ++it)
				{
					if (ServerInstance->IsChannel(it->c_str(), ServerInstance->Config->Limits.ChanMax))
					{
						Channel::JoinUser(user, it->c_str(), override, "", false, ServerInstance->Time());
					}
				}
			}
		}
};

MODULE_INIT(ModuleOperjoin)
