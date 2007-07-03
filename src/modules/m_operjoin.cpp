/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Forces opers to join the specified channel(s) on oper-up */

class ModuleOperjoin : public Module
{
	private:
		std::string operChan;
		std::vector<std::string> operChans;		

		int tokenize(const string &str, std::vector<std::string> &tokens)
		{
			// skip delimiters at beginning.
			string::size_type lastPos = str.find_first_not_of(",", 0);
			// find first "non-delimiter".
			string::size_type pos = str.find_first_of(",", lastPos);

			while (string::npos != pos || string::npos != lastPos)
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
			OnRehash(NULL, "");
		}

		void Implements(char* List)
		{
			List[I_OnPostOper] = List[I_OnRehash] = 1;
		}

		virtual void OnRehash(userrec* user, const std::string &parameter)
		{
			ConfigReader* conf = new ConfigReader(ServerInstance);

			operChan = conf->ReadValue("operjoin", "channel", 0);
			operChans.clear();
			if (!operChan.empty())
				tokenize(operChan,operChans);

			DELETE(conf);
		}

		virtual ~ModuleOperjoin()
		{
		}

		virtual Version GetVersion()
		{
			return Version(1,1,0,1,VF_VENDOR,API_VERSION);
		}

		virtual void OnPostOper(userrec* user, const std::string &opertype)
		{
			if (!IS_LOCAL(user))
				return;

			for(std::vector<std::string>::iterator it = operChans.begin(); it != operChans.end(); it++)
				if (ServerInstance->IsChannel(it->c_str()))
					chanrec::JoinUser(ServerInstance, user, it->c_str(), false, "", ServerInstance->Time(true));
		}

};

MODULE_INIT(ModuleOperjoin)
