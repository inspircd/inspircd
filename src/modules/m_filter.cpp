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
#include "m_filter.h"

/* $ModDesc: An advanced spam filtering module */
/* $ModDep: m_filter.h */

typedef std::map<std::string,FilterResult*> filter_t;

class ModuleFilter : public FilterBase
{
 
 filter_t filters;

 public:
	ModuleFilter(InspIRCd* Me)
	: FilterBase(Me, "m_filter.so")
	{
		OnRehash(NULL,"");
	}
	
	virtual ~ModuleFilter()
	{
	}

	virtual FilterResult* FilterMatch(userrec* user, const std::string &text, int flags)
	{
		for (filter_t::iterator index = filters.begin(); index != filters.end(); index++)
		{

			/* Skip ones that dont apply to us */
			if (!FilterBase::AppliesToMe(user, index->second, flags))
				continue;

			if (ServerInstance->MatchText(text,index->first))
			{
				FilterResult* fr = index->second;
				if (index != filters.begin())
				{
					std::string pat = index->first;
					filters.erase(index);
					filters.insert(filters.begin(), std::make_pair(pat,fr));
				}
				return fr;
			}
		}
		return NULL;
	}

	virtual bool DeleteFilter(const std::string &freeform)
	{
		if (filters.find(freeform) != filters.end())
		{
			delete (filters.find(freeform))->second;
			filters.erase(filters.find(freeform));
			return true;
		}
		return false;
	}

	virtual std::pair<bool, std::string> AddFilter(const std::string &freeform, const std::string &type, const std::string &reason, long duration, const std::string &flags)
	{
		if (filters.find(freeform) != filters.end())
		{
			return std::make_pair(false, "Filter already exists");
		}

		FilterResult* x = new FilterResult(freeform, reason, type, duration, flags);
		filters[freeform] = x;

		return std::make_pair(true, "");
	}

	virtual void SyncFilters(Module* proto, void* opaque)
	{
		for (filter_t::iterator n = filters.begin(); n != filters.end(); n++)
		{
			this->SendFilter(proto, opaque, n->second);
		}
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ConfigReader* MyConf = new ConfigReader(ServerInstance);

		for (int index = 0; index < MyConf->Enumerate("keyword"); index++)
		{
			this->DeleteFilter(MyConf->ReadValue("keyword","pattern",index));

			std::string pattern = MyConf->ReadValue("keyword","pattern",index);
			std::string reason = MyConf->ReadValue("keyword","reason",index);
			std::string do_action = MyConf->ReadValue("keyword","action",index);
			std::string flags = MyConf->ReadValue("keyword","flags",index);
			long gline_time = ServerInstance->Duration(MyConf->ReadValue("keyword","duration",index));
			if (do_action.empty())
				do_action = "none";
			if (flags.empty())
				flags = "*";
			FilterResult* x = new FilterResult(pattern, reason, do_action, gline_time, flags);
			filters[pattern] = x;
		}
		DELETE(MyConf);
	}

	virtual int OnStats(char symbol, userrec* user, string_list &results)
	{
		if (symbol == 's')
		{
			std::string sn = ServerInstance->Config->ServerName;
			for (filter_t::iterator n = filters.begin(); n != filters.end(); n++)
			{
				results.push_back(sn+" 223 "+user->nick+" :GLOB:"+n->second->freeform+" "+n->second->flags+" "+n->second->action+" "+ConvToStr(n->second->gline_time)+" :"+n->second->reason);
			}
		}
		return 0;
	}
};


MODULE_INIT(ModuleFilter)
