/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"
#include "m_regex.h"

/* $ModDesc: Text (spam) filtering */

class ModuleFilter;

enum FilterFlags
{
	FLAG_PART = 2,
	FLAG_QUIT = 4,
	FLAG_PRIVMSG = 8,
	FLAG_NOTICE = 16
};

class FilterResult
{
 public:
	std::string freeform;
	std::string reason;
	std::string action;
	long gline_time;
	std::string flags;

	bool flag_no_opers;
	bool flag_part_message;
	bool flag_quit_message;
	bool flag_privmsg;
	bool flag_notice;

	FilterResult(const std::string free, const std::string &rea, const std::string &act, long gt, const std::string &fla) :
			freeform(free), reason(rea), action(act), gline_time(gt), flags(fla)
	{
		this->FillFlags(fla);
	}

	int FillFlags(const std::string &fl)
	{
		flags = fl;
		flag_no_opers = flag_part_message = flag_quit_message = flag_privmsg = flag_notice = false;
		size_t x = 0;

		for (std::string::const_iterator n = flags.begin(); n != flags.end(); ++n, ++x)
		{
			switch (*n)
			{
				case 'o':
					flag_no_opers = true;
				break;
				case 'P':
					flag_part_message = true;
				break;
				case 'q':
					flag_quit_message = true;
				break;
				case 'p':
					flag_privmsg = true;
				break;
				case 'n':
					flag_notice = true;
				break;
				case '*':
					flag_no_opers = flag_part_message = flag_quit_message =
						flag_privmsg = flag_notice = true;
				break;
				default:
					return x;
				break;
			}
		}
		return 0;
	}

	FilterResult()
	{
	}

	~FilterResult()
	{
	}
};

class CommandFilter : public Command
{
 public:
	CommandFilter(Module* f)
		: Command(f, "FILTER", 1, 5)
	{
		flags_needed = 'o';
		this->syntax = "<filter-definition> <action> <flags> [<gline-duration>] :<reason>";
	}
	CmdResult Handle(const std::vector<std::string>&, User*);

	void TooFewParams(User* user, const std::string &extra_text)
	{
		user->WriteServ("NOTICE %s :*** Not enough parameters%s", user->nick.c_str(), extra_text.c_str());
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ImplFilter : public FilterResult
{
 public:
	Regex* regex;

	ImplFilter(ModuleFilter* mymodule, const std::string &rea, const std::string &act, long glinetime, const std::string &pat, const std::string &flgs);
};


class ModuleFilter : public Module
{
 public:
	CommandFilter filtcommand;
	dynamic_reference<RegexFactory> RegexEngine;

	std::vector<ImplFilter> filters;
	const char *error;
	int erroffset;
	int flags;

	std::vector<std::string> exemptfromfilter; // List of channel names excluded from filtering.

	ModuleFilter();

	~ModuleFilter();
	ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list);
	FilterResult* FilterMatch(User* user, const std::string &text, int flags);
	bool DeleteFilter(const std::string &freeform);
	void SyncFilters(Module* proto, void* opaque);
	void SendFilter(Module* proto, void* opaque, FilterResult* iter);
	std::pair<bool, std::string> AddFilter(const std::string &freeform, const std::string &type, const std::string &reason, long duration, const std::string &flags);
	ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list);
	void OnRehash(User* user);
	Version GetVersion();
	std::string EncodeFilter(FilterResult* filter);
	FilterResult DecodeFilter(const std::string &data);
	void OnSyncNetwork(Module* proto, void* opaque);
	void OnDecodeMetaData(Extensible* target, const std::string &extname, const std::string &extdata);
	ModResult OnStats(char symbol, User* user, string_list &results);
	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line);
	bool AppliesToMe(User* user, FilterResult* filter, int flags);
	void ReadFilters(ConfigReader &MyConf);
};

CmdResult CommandFilter::Handle(const std::vector<std::string> &parameters, User *user)
{
	if (parameters.size() == 1)
	{
		/* Deleting a filter */
		if (static_cast<ModuleFilter&>(*creator).DeleteFilter(parameters[0]))
		{
			user->WriteServ("NOTICE %s :*** Removed filter '%s'", user->nick.c_str(), parameters[0].c_str());
			ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'a' : 'A', std::string("FILTER: ")+user->nick+" removed filter '"+parameters[0]+"'");
			return CMD_SUCCESS;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Filter '%s' not found in list, try /stats s.", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
	}
	else
	{
		/* Adding a filter */
		if (parameters.size() >= 4)
		{
			std::string freeform = parameters[0];
			std::string type = parameters[1];
			std::string flags = parameters[2];
			std::string reason;
			long duration = 0;


			if ((type != "gline") && (type != "none") && (type != "block") && (type != "kill") && (type != "silent"))
			{
				user->WriteServ("NOTICE %s :*** Invalid filter type '%s'. Supported types are 'gline', 'none', 'block', 'silent' and 'kill'.", user->nick.c_str(), type.c_str());
				return CMD_FAILURE;
			}

			if (type == "gline")
			{
				if (parameters.size() >= 5)
				{
					duration = ServerInstance->Duration(parameters[3]);
					reason = parameters[4];
				}
				else
				{
					this->TooFewParams(user, ": When setting a gline type filter, a gline duration must be specified as the third parameter.");
					return CMD_FAILURE;
				}
			}
			else
			{
				reason = parameters[3];
			}
			std::pair<bool, std::string> result = static_cast<ModuleFilter&>(*creator).AddFilter(freeform, type, reason, duration, flags);
			if (result.first)
			{
				user->WriteServ("NOTICE %s :*** Added filter '%s', type '%s'%s%s, flags '%s', reason: '%s'", user->nick.c_str(), freeform.c_str(),
						type.c_str(), (duration ? ", duration " : ""), (duration ? parameters[3].c_str() : ""),
						flags.c_str(), reason.c_str());

				ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'a' : 'A', std::string("FILTER: ")+user->nick+" added filter '"+freeform+"', type '"+type+"', "+(duration ? "duration "+parameters[3]+", " : "")+"flags '"+flags+"', reason: "+reason);

				return CMD_SUCCESS;
			}
			else
			{
				user->WriteServ("NOTICE %s :*** Filter '%s' could not be added: %s", user->nick.c_str(), freeform.c_str(), result.second.c_str());
				return CMD_FAILURE;
			}
		}
		else
		{
			this->TooFewParams(user, ".");
			return CMD_FAILURE;
		}

	}
}

bool ModuleFilter::AppliesToMe(User* user, FilterResult* filter, int iflags)
{
	if ((filter->flag_no_opers) && IS_OPER(user))
		return false;
	if ((iflags & FLAG_PRIVMSG) && (!filter->flag_privmsg))
		return false;
	if ((iflags & FLAG_NOTICE) && (!filter->flag_notice))
		return false;
	if ((iflags & FLAG_QUIT)   && (!filter->flag_quit_message))
		return false;
	if ((iflags & FLAG_PART)   && (!filter->flag_part_message))
		return false;
	return true;
}

ModuleFilter::ModuleFilter() : filtcommand(this), RegexEngine(this, "regex")
{
	ServerInstance->AddCommand(&filtcommand);
	Implementation eventlist[] = { I_OnPreCommand, I_OnStats, I_OnSyncNetwork, I_OnDecodeMetaData, I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash };
	ServerInstance->Modules->Attach(eventlist, this, 7);
	OnRehash(NULL);
}

ModuleFilter::~ModuleFilter()
{
}

ModResult ModuleFilter::OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
{
	if (!IS_LOCAL(user))
		return MOD_RES_PASSTHRU;

	flags = FLAG_PRIVMSG;
	return OnUserPreNotice(user,dest,target_type,text,status,exempt_list);
}

ModResult ModuleFilter::OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
{
	/* Leave ulines alone */
	if ((ServerInstance->ULine(user->server)) || (!IS_LOCAL(user)))
		return MOD_RES_PASSTHRU;

	if (!flags)
		flags = FLAG_NOTICE;

	FilterResult* f = this->FilterMatch(user, text, flags);
	if (f)
	{
		std::string target = "";
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			target = std::string(t->nick);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			Channel* t = (Channel*)dest;
			target = std::string(t->name);
			std::vector<std::string>::iterator i = find(exemptfromfilter.begin(), exemptfromfilter.end(), target);
			if (i != exemptfromfilter.end()) return MOD_RES_PASSTHRU;
		}
		if (f->action == "block")
		{
			ServerInstance->SNO->WriteGlobalSno('a', std::string("FILTER: ")+user->nick+" had their message filtered, target was "+target+": "+f->reason);
			if (target_type == TYPE_CHANNEL)
				user->WriteNumeric(404, "%s %s :Message to channel blocked and opers notified (%s)",user->nick.c_str(), target.c_str(), f->reason.c_str());
			else
				user->WriteServ("NOTICE "+std::string(user->nick)+" :Your message to "+target+" was blocked and opers notified: "+f->reason);
		}
		if (f->action == "silent")
		{
			if (target_type == TYPE_CHANNEL)
				user->WriteNumeric(404, "%s %s :Message to channel blocked (%s)",user->nick.c_str(), target.c_str(), f->reason.c_str());
			else
				user->WriteServ("NOTICE "+std::string(user->nick)+" :Your message to "+target+" was blocked: "+f->reason);
		}
		if (f->action == "kill")
		{
			ServerInstance->Users->QuitUser(user, "Filtered: " + f->reason);
		}
		if (f->action == "gline")
		{
			GLine* gl = new GLine(ServerInstance->Time(), f->gline_time, ServerInstance->Config->ServerName.c_str(), f->reason.c_str(), "*", user->GetIPString());
			if (ServerInstance->XLines->AddLine(gl,NULL))
			{
				ServerInstance->XLines->ApplyLines();
			}
			else
				delete gl;
		}

		ServerInstance->Logs->Log("FILTER",DEFAULT,"FILTER: "+ user->nick + " had their message filtered, target was " + target + ": " + f->reason + " Action: " + f->action);
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}

ModResult ModuleFilter::OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
{
	flags = 0;
	if (validated && IS_LOCAL(user))
	{
		std::string checkline;
		int replacepoint = 0;
		bool parting = false;

		if (command == "QUIT")
		{
			/* QUIT with no reason: nothing to do */
			if (parameters.size() < 1)
				return MOD_RES_PASSTHRU;

			checkline = parameters[0];
			replacepoint = 0;
			parting = false;
			flags = FLAG_QUIT;
		}
		else if (command == "PART")
		{
			/* PART with no reason: nothing to do */
			if (parameters.size() < 2)
				return MOD_RES_PASSTHRU;

			std::vector<std::string>::iterator i = find(exemptfromfilter.begin(), exemptfromfilter.end(), parameters[0]);
			if (i != exemptfromfilter.end()) return MOD_RES_PASSTHRU;
			checkline = parameters[1];
			replacepoint = 1;
			parting = true;
			flags = FLAG_PART;
		}
		else
			/* We're only messing with PART and QUIT */
			return MOD_RES_PASSTHRU;

		FilterResult* f = NULL;

		if (flags)
			f = this->FilterMatch(user, checkline, flags);

		if (!f)
			/* PART or QUIT reason doesnt match a filter */
			return MOD_RES_PASSTHRU;

		/* We cant block a part or quit, so instead we change the reason to 'Reason filtered' */
		Command* c = ServerInstance->Parser->GetHandler(command);
		if (c)
		{
			std::vector<std::string> params;
			for (int item = 0; item < (int)parameters.size(); item++)
				params.push_back(parameters[item]);
			params[replacepoint] = "Reason filtered";

			/* We're blocking, OR theyre quitting and its a KILL action
			 * (we cant kill someone whos already quitting, so filter them anyway)
			 */
			if ((f->action == "block") || (((!parting) && (f->action == "kill"))) || (f->action == "silent"))
			{
				c->Handle(params, user);
				return MOD_RES_DENY;
			}
			else
			{
				/* Are they parting, if so, kill is applicable */
				if ((parting) && (f->action == "kill"))
				{
					user->WriteServ("NOTICE %s :*** Your PART message was filtered: %s", user->nick.c_str(), f->reason.c_str());
					ServerInstance->Users->QuitUser(user, "Filtered: " + f->reason);
				}
				if (f->action == "gline")
				{
					/* Note: We gline *@IP so that if their host doesnt resolve the gline still applies. */
					GLine* gl = new GLine(ServerInstance->Time(), f->gline_time, ServerInstance->Config->ServerName.c_str(), f->reason.c_str(), "*", user->GetIPString());
					if (ServerInstance->XLines->AddLine(gl,NULL))
					{
						ServerInstance->XLines->ApplyLines();
					}
					else
						delete gl;
				}
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
	return MOD_RES_PASSTHRU;
}

void ModuleFilter::OnRehash(User* user)
{
	ConfigReader MyConf;
	std::vector<std::string>().swap(exemptfromfilter);
	for (int index = 0; index < MyConf.Enumerate("exemptfromfilter"); ++index)
	{
		std::string chan = MyConf.ReadValue("exemptfromfilter", "channel", index);
		if (!chan.empty()) {
			exemptfromfilter.push_back(chan);
		}
	}
	std::string newrxengine = "regex/" + MyConf.ReadValue("filteropts", "engine", 0);
	if (RegexEngine.GetProvider() == newrxengine)
		return;

	//ServerInstance->SNO->WriteGlobalSno('a', "Dumping all filters due to regex engine change (was '%s', now '%s')", RegexEngine.GetProvider().c_str(), newrxengine.c_str());
	//ServerInstance->XLines->DelAll("R");

	RegexEngine.SetProvider(newrxengine);
	if (!RegexEngine)
	{
		ServerInstance->SNO->WriteGlobalSno('a', "WARNING: Regex engine '%s' is not loaded - Filter functionality disabled until this is corrected.", RegexEngine.GetProvider().c_str());
	}
	ReadFilters(MyConf);
}

Version ModuleFilter::GetVersion()
{
	return Version("Text (spam) filtering", VF_VENDOR | VF_COMMON, RegexEngine);
}


std::string ModuleFilter::EncodeFilter(FilterResult* filter)
{
	std::ostringstream stream;
	std::string x = filter->freeform;

	/* Hax to allow spaces in the freeform without changing the design of the irc protocol */
	for (std::string::iterator n = x.begin(); n != x.end(); n++)
		if (*n == ' ')
			*n = '\7';

	stream << x << " " << filter->action << " " << (filter->flags.empty() ? "-" : filter->flags) << " " << filter->gline_time << " :" << filter->reason;
	return stream.str();
}

FilterResult ModuleFilter::DecodeFilter(const std::string &data)
{
	FilterResult res;
	irc::tokenstream tokens(data);
	tokens.GetToken(res.freeform);
	tokens.GetToken(res.action);
	tokens.GetToken(res.flags);
	if (res.flags == "-")
		res.flags = "";
	res.FillFlags(res.flags);
	tokens.GetToken(res.gline_time);
	tokens.GetToken(res.reason);

	/* Hax to allow spaces in the freeform without changing the design of the irc protocol */
	for (std::string::iterator n = res.freeform.begin(); n != res.freeform.end(); n++)
		if (*n == '\7')
			*n = ' ';

	return res;
}

void ModuleFilter::OnSyncNetwork(Module* proto, void* opaque)
{
	this->SyncFilters(proto, opaque);
}

void ModuleFilter::SendFilter(Module* proto, void* opaque, FilterResult* iter)
{
	proto->ProtoSendMetaData(opaque, NULL, "filter", EncodeFilter(iter));
}

void ModuleFilter::OnDecodeMetaData(Extensible* target, const std::string &extname, const std::string &extdata)
{
	if ((target == NULL) && (extname == "filter"))
	{
		FilterResult data = DecodeFilter(extdata);
		this->AddFilter(data.freeform, data.action, data.reason, data.gline_time, data.flags);
	}
}

ImplFilter::ImplFilter(ModuleFilter* mymodule, const std::string &rea, const std::string &act, long glinetime, const std::string &pat, const std::string &flgs)
		: FilterResult(pat, rea, act, glinetime, flgs)
{
	if (!mymodule->RegexEngine)
		throw ModuleException("Regex module implementing '"+mymodule->RegexEngine.GetProvider()+"' is not loaded!");
	regex = mymodule->RegexEngine->Create(pat);
}

FilterResult* ModuleFilter::FilterMatch(User* user, const std::string &text, int flgs)
{
	for (std::vector<ImplFilter>::iterator index = filters.begin(); index != filters.end(); index++)
	{
		/* Skip ones that dont apply to us */
		if (!AppliesToMe(user, dynamic_cast<FilterResult*>(&(*index)), flgs))
			continue;

		//ServerInstance->Logs->Log("m_filter", DEBUG, "Match '%s' against '%s'", text.c_str(), index->freeform.c_str());
		if (index->regex->Matches(text))
		{
			//ServerInstance->Logs->Log("m_filter", DEBUG, "MATCH");
			ImplFilter fr = *index;
			if (index != filters.begin())
			{
				/* Move to head of list for efficiency */
				filters.erase(index);
				filters.insert(filters.begin(), fr);
			}
			return &*filters.begin();
		}
		//ServerInstance->Logs->Log("m_filter", DEBUG, "NO MATCH");
	}
	return NULL;
}

bool ModuleFilter::DeleteFilter(const std::string &freeform)
{
	for (std::vector<ImplFilter>::iterator i = filters.begin(); i != filters.end(); i++)
	{
		if (i->freeform == freeform)
		{
			delete i->regex;
			filters.erase(i);
			return true;
		}
	}
	return false;
}

void ModuleFilter::SyncFilters(Module* proto, void* opaque)
{
	for (std::vector<ImplFilter>::iterator i = filters.begin(); i != filters.end(); i++)
	{
		this->SendFilter(proto, opaque, &(*i));
	}
}

std::pair<bool, std::string> ModuleFilter::AddFilter(const std::string &freeform, const std::string &type, const std::string &reason, long duration, const std::string &flgs)
{
	for (std::vector<ImplFilter>::iterator i = filters.begin(); i != filters.end(); i++)
	{
		if (i->freeform == freeform)
		{
			return std::make_pair(false, "Filter already exists");
		}
	}

	try
	{
		filters.push_back(ImplFilter(this, reason, type, duration, freeform, flgs));
	}
	catch (ModuleException &e)
	{
		ServerInstance->Logs->Log("m_filter", DEFAULT, "Error in regular expression '%s': %s", freeform.c_str(), e.GetReason());
		return std::make_pair(false, e.GetReason());
	}
	return std::make_pair(true, "");
}

void ModuleFilter::ReadFilters(ConfigReader &MyConf)
{
	for (int index = 0; index < MyConf.Enumerate("keyword"); index++)
	{
		this->DeleteFilter(MyConf.ReadValue("keyword", "pattern", index));

		std::string pattern = MyConf.ReadValue("keyword", "pattern", index);
		std::string reason = MyConf.ReadValue("keyword", "reason", index);
		std::string action = MyConf.ReadValue("keyword", "action", index);
		std::string flgs = MyConf.ReadValue("keyword", "flags", index);
		long gline_time = ServerInstance->Duration(MyConf.ReadValue("keyword", "duration", index));
		if (action.empty())
			action = "none";
		if (flgs.empty())
			flgs = "*";

		try
		{
			filters.push_back(ImplFilter(this, reason, action, gline_time, pattern, flgs));
			ServerInstance->Logs->Log("m_filter", DEFAULT, "Regular expression %s loaded.", pattern.c_str());
		}
		catch (ModuleException &e)
		{
			ServerInstance->Logs->Log("m_filter", DEFAULT, "Error in regular expression '%s': %s", pattern.c_str(), e.GetReason());
		}
	}
}

ModResult ModuleFilter::OnStats(char symbol, User* user, string_list &results)
{
	if (symbol == 's')
	{
		std::string sn = ServerInstance->Config->ServerName;
		for (std::vector<ImplFilter>::iterator i = filters.begin(); i != filters.end(); i++)
		{
			results.push_back(sn+" 223 "+user->nick+" :"+RegexEngine.GetProvider()+":"+i->freeform+" "+i->flags+" "+i->action+" "+ConvToStr(i->gline_time)+" :"+i->reason);
		}
		for (std::vector<std::string>::iterator i = exemptfromfilter.begin(); i != exemptfromfilter.end(); ++i)
		{
			results.push_back(sn+" 223 "+user->nick+" :EXEMPT "+(*i));
		}
	}
	return MOD_RES_PASSTHRU;
}

MODULE_INIT(ModuleFilter)
