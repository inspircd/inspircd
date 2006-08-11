// operjoin module by typobox43

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Forces opers to join the specified channel(s) on oper-up */



class ModuleOperjoin : public Module
{
	private:
		std::string operChan;
		ConfigReader* conf;
		

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
		ModuleOperjoin(InspIRCd* Me)
			: Module::Module(Me)
		{
			
			conf = new ConfigReader;
			operChan = conf->ReadValue("operjoin", "channel", 0);
		}

		void Implements(char* List)
		{
			List[I_OnPostOper] = List[I_OnRehash] = 1;
		}

		virtual void OnRehash(const std::string &parameter)
		{
			DELETE(conf);
			conf = new ConfigReader;
		}

		virtual ~ModuleOperjoin()
		{
			DELETE(conf);
		}

		virtual Version GetVersion()
		{
			return Version(1,0,0,1,VF_VENDOR);
		}

		virtual void OnPostOper(userrec* user, const std::string &opertype)
		{
			if(operChan != "")
			{
				std::vector<std::string> operChans;
				tokenize(operChan,operChans);
				for(std::vector<std::string>::iterator it = operChans.begin(); it != operChans.end(); it++)
					chanrec::JoinUser(ServerInstance, user, it->c_str(), false);
			}

		}

};

class ModuleOperjoinFactory : public ModuleFactory
{
	public:
		ModuleOperjoinFactory()
		{
		}

		~ModuleOperjoinFactory()
		{
		}

		virtual Module * CreateModule(InspIRCd* Me)
		{
			return new ModuleOperjoin(Me);
		}
};

extern "C" void * init_module( void )
{
	return new ModuleOperjoinFactory;
}
