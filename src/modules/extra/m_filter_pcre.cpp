/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <pcre.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "m_filter.h"

/* $ModDesc: m_filter with regexps */
/* $CompileFlags: exec("pcre-config --cflags") */
/* $LinkerFlags: exec("pcre-config --libs") rpath("pcre-config --libs") -lpcre */
/* $ModDep: m_filter.h */

class PCREFilter : public FilterResult
{
 public:
	 pcre* regexp;

	 PCREFilter(pcre* r, const std::string &rea, const std::string &act, long gline_time, const std::string &pat)
		 : FilterResult::FilterResult(pat, rea, act, gline_time), regexp(r)
	 {
	 }

	 PCREFilter()
	 {
	 }
};

class ModuleFilterPCRE : public FilterBase
{
	std::vector<PCREFilter> filters;
	pcre *re;
	const char *error;
	int erroffset;
	PCREFilter fr;

 public:
	ModuleFilterPCRE(InspIRCd* Me)
	: FilterBase::FilterBase(Me, "m_filter_pcre.so")
	{
		OnRehash(NULL,"");
	}

	virtual ~ModuleFilterPCRE()
	{
	}

	virtual FilterResult* FilterMatch(const std::string &text)
	{
		for (std::vector<PCREFilter>::iterator index = filters.begin(); index != filters.end(); index++)
		{
			if (pcre_exec(index->regexp, NULL, text.c_str(), text.length(), 0, 0, NULL, 0) > -1)
			{
				fr = *index;
				if (index != filters.begin())
				{
					filters.erase(index);
					filters.insert(filters.begin(), fr);
				}
				return &fr;
			}
		}
		return NULL;
	}

	virtual bool DeleteFilter(const std::string &freeform)
	{
		for (std::vector<PCREFilter>::iterator i = filters.begin(); i != filters.end(); i++)
		{
			if (i->freeform == freeform)
			{
				pcre_free((*i).regexp);
				filters.erase(i);
				return true;
			}
		}
		return false;
	}

	virtual void SyncFilters(Module* proto, void* opaque)
	{
		for (std::vector<PCREFilter>::iterator i = filters.begin(); i != filters.end(); i++)
		{
			this->SendFilter(proto, opaque, &(*i));
		}
	}

	virtual std::pair<bool, std::string> AddFilter(const std::string &freeform, const std::string &type, const std::string &reason, long duration)
	{
		for (std::vector<PCREFilter>::iterator i = filters.begin(); i != filters.end(); i++)
		{
			if (i->freeform == freeform)
			{
				return std::make_pair(false, "Filter already exists");
			}
		}

		re = pcre_compile(freeform.c_str(),0,&error,&erroffset,NULL);

		if (!re)
		{
			ServerInstance->Log(DEFAULT,"Error in regular expression: %s at offset %d: %s\n", freeform.c_str(), erroffset, error);
			ServerInstance->Log(DEFAULT,"Regular expression %s not loaded.", freeform.c_str());
			return std::make_pair(false, "Error in regular expression at offset " + ConvToStr(erroffset) + ": "+error);
		}
		else
		{
			filters.push_back(PCREFilter(re, reason, type, duration, freeform));
			return std::make_pair(true, "");
		}
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{		
		ConfigReader MyConf(ServerInstance);

		for (int index = 0; index < MyConf.Enumerate("keyword"); index++)
		{
			this->DeleteFilter(MyConf.ReadValue("keyword", "pattern", index));

			std::string pattern = MyConf.ReadValue("keyword", "pattern", index);
			std::string reason = MyConf.ReadValue("keyword", "reason", index);
			std::string action = MyConf.ReadValue("keyword", "action", index);
			long gline_time = ServerInstance->Duration(MyConf.ReadValue("keyword", "duration", index).c_str());

			re = pcre_compile(pattern.c_str(),0,&error,&erroffset,NULL);

			if (!re)
			{
				ServerInstance->Log(DEFAULT,"Error in regular expression: %s at offset %d: %s\n", pattern.c_str(), erroffset, error);
				ServerInstance->Log(DEFAULT,"Regular expression %s not loaded.", pattern.c_str());
			}
			else
			{
				filters.push_back(PCREFilter(re, reason, action, gline_time, pattern));
				ServerInstance->Log(DEFAULT,"Regular expression %s loaded.", pattern.c_str());
			}
		}
	}

	virtual int OnStats(char symbol, userrec* user, string_list &results)
	{
		if (symbol == 's')
		{
			std::string sn = ServerInstance->Config->ServerName;
			for (std::vector<PCREFilter>::iterator i = filters.begin(); i != filters.end(); i++)
			{
				results.push_back(sn+" 223 "+user->nick+" :REGEXP:"+i->freeform+" "+i->action+" "+ConvToStr(i->gline_time)+" :"+i->reason);
			}
		}
		return 0;
	}
};
	

class ModuleFilterPCREFactory : public ModuleFactory
{
 public:
	ModuleFilterPCREFactory()
	{
	}
	
	~ModuleFilterPCREFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleFilterPCRE(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleFilterPCREFactory;
}
