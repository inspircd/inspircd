/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2004, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

enum FilterAction
{
	FA_GLINE,
	FA_BLOCK,
	FA_SILENT,
	FA_KILL,
	FA_NONE
};

class FilterResult
{
 public:
	std::string freeform;
	std::string reason;
	FilterAction action;
	long gline_time;

	bool flag_no_opers;
	bool flag_part_message;
	bool flag_quit_message;
	bool flag_privmsg;
	bool flag_notice;
	bool flag_strip_color;

	FilterResult(const std::string& free, const std::string& rea, FilterAction act, long gt, const std::string& fla) :
			freeform(free), reason(rea), action(act), gline_time(gt)
	{
		this->FillFlags(fla);
	}

	char FillFlags(const std::string &fl)
	{
		flag_no_opers = flag_part_message = flag_quit_message = flag_privmsg =
			flag_notice = flag_strip_color = false;

		for (std::string::const_iterator n = fl.begin(); n != fl.end(); ++n)
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
				case 'c':
					flag_strip_color = true;
				break;
				case '*':
					flag_no_opers = flag_part_message = flag_quit_message =
						flag_privmsg = flag_notice = flag_strip_color = true;
				break;
				default:
					return *n;
				break;
			}
		}
		return 0;
	}

	std::string GetFlags()
	{
		std::string flags;
		if (flag_no_opers)
			flags.push_back('o');
		if (flag_part_message)
			flags.push_back('P');
		if (flag_quit_message)
			flags.push_back('q');
		if (flag_privmsg)
			flags.push_back('p');
		if (flag_notice)
			flags.push_back('n');

		/* Order is important here, 'c' must be the last char in the string as it is unsupported
		 * on < 2.0.10, and the logic in FillFlags() stops parsing when it ecounters an unknown
		 * character.
		 */
		if (flag_strip_color)
			flags.push_back('c');

		if (flags.empty())
			flags.push_back('-');

		return flags;
	}

	FilterResult()
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

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ImplFilter : public FilterResult
{
 public:
	Regex* regex;

	ImplFilter(ModuleFilter* mymodule, const std::string &rea, FilterAction act, long glinetime, const std::string &pat, const std::string &flgs);
};


class ModuleFilter : public Module
{
	bool initing;
	RegexFactory* factory;
	void FreeFilters();

 public:
	CommandFilter filtcommand;
	dynamic_reference<RegexFactory> RegexEngine;

	std::vector<ImplFilter> filters;
	int flags;

	std::set<std::string> exemptfromfilter; // List of channel names excluded from filtering.

	ModuleFilter();
	void init();
	CullResult cull();
	ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list);
	FilterResult* FilterMatch(User* user, const std::string &text, int flags);
	bool DeleteFilter(const std::string &freeform);
	std::pair<bool, std::string> AddFilter(const std::string &freeform, FilterAction type, const std::string &reason, long duration, const std::string &flags);
	ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list);
	void OnRehash(User* user);
	Version GetVersion();
	std::string EncodeFilter(FilterResult* filter);
	FilterResult DecodeFilter(const std::string &data);
	void OnSyncNetwork(Module* proto, void* opaque);
	void OnDecodeMetaData(Extensible* target, const std::string &extname, const std::string &extdata);
	ModResult OnStats(char symbol, User* user, string_list &results);
	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line);
	void OnUnloadModule(Module* mod);
	bool AppliesToMe(User* user, FilterResult* filter, int flags);
	void ReadFilters();
	static bool StringToFilterAction(const std::string& str, FilterAction& fa);
	static std::string FilterActionToString(FilterAction fa);
};

CmdResult CommandFilter::Handle(const std::vector<std::string> &parameters, User *user)
{
	if (parameters.size() == 1)
	{
		/* Deleting a filter */
		Module *me = creator;
		if (static_cast<ModuleFilter *>(me)->DeleteFilter(parameters[0]))
		{
			user->WriteServ("NOTICE %s :*** Removed filter '%s'", user->nick.c_str(), parameters[0].c_str());
			ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'a' : 'A', "FILTER: "+user->nick+" removed filter '"+parameters[0]+"'");
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
			const std::string& freeform = parameters[0];
			FilterAction type;
			const std::string& flags = parameters[2];
			unsigned int reasonindex;
			long duration = 0;

			if (!ModuleFilter::StringToFilterAction(parameters[1], type))
			{
				user->WriteServ("NOTICE %s :*** Invalid filter type '%s'. Supported types are 'gline', 'none', 'block', 'silent' and 'kill'.", user->nick.c_str(), parameters[1].c_str());
				return CMD_FAILURE;
			}

			if (type == FA_GLINE)
			{
				if (parameters.size() >= 5)
				{
					duration = ServerInstance->Duration(parameters[3]);
					reasonindex = 4;
				}
				else
				{
					user->WriteServ("NOTICE %s :*** Not enough parameters: When setting a gline type filter, a gline duration must be specified as the third parameter.", user->nick.c_str());
					return CMD_FAILURE;
				}
			}
			else
			{
				reasonindex = 3;
			}

			Module *me = creator;
			std::pair<bool, std::string> result = static_cast<ModuleFilter *>(me)->AddFilter(freeform, type, parameters[reasonindex], duration, flags);
			if (result.first)
			{
				user->WriteServ("NOTICE %s :*** Added filter '%s', type '%s'%s%s, flags '%s', reason: '%s'", user->nick.c_str(), freeform.c_str(),
						parameters[1].c_str(), (duration ? ", duration " : ""), (duration ? parameters[3].c_str() : ""),
						flags.c_str(), parameters[reasonindex].c_str());

				ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'a' : 'A', "FILTER: "+user->nick+" added filter '"+freeform+"', type '"+parameters[1]+"', "+(duration ? "duration "+parameters[3]+", " : "")+"flags '"+flags+"', reason: "+parameters[reasonindex]);

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
			user->WriteServ("NOTICE %s :*** Not enough parameters.", user->nick.c_str());
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

ModuleFilter::ModuleFilter()
	: initing(true), filtcommand(this), RegexEngine(this, "regex")
{
}

void ModuleFilter::init()
{
	ServerInstance->Modules->AddService(filtcommand);
	Implementation eventlist[] = { I_OnPreCommand, I_OnStats, I_OnSyncNetwork, I_OnDecodeMetaData, I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash, I_OnUnloadModule };
	ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	OnRehash(NULL);
}

CullResult ModuleFilter::cull()
{
	FreeFilters();
	return Module::cull();
}

void ModuleFilter::FreeFilters()
{
	for (std::vector<ImplFilter>::const_iterator i = filters.begin(); i != filters.end(); ++i)
		delete i->regex;

	filters.clear();
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
		std::string target;
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			target = t->nick;
		}
		else if (target_type == TYPE_CHANNEL)
		{
			Channel* t = (Channel*)dest;
			if (exemptfromfilter.find(t->name) != exemptfromfilter.end())
				return MOD_RES_PASSTHRU;

			target = t->name;
		}
		if (f->action == FA_BLOCK)
		{
			ServerInstance->SNO->WriteGlobalSno('a', "FILTER: "+user->nick+" had their message filtered, target was "+target+": "+f->reason);
			if (target_type == TYPE_CHANNEL)
				user->WriteNumeric(404, "%s %s :Message to channel blocked and opers notified (%s)",user->nick.c_str(), target.c_str(), f->reason.c_str());
			else
				user->WriteServ("NOTICE "+user->nick+" :Your message to "+target+" was blocked and opers notified: "+f->reason);
		}
		else if (f->action == FA_SILENT)
		{
			if (target_type == TYPE_CHANNEL)
				user->WriteNumeric(404, "%s %s :Message to channel blocked (%s)",user->nick.c_str(), target.c_str(), f->reason.c_str());
			else
				user->WriteServ("NOTICE "+user->nick+" :Your message to "+target+" was blocked: "+f->reason);
		}
		else if (f->action == FA_KILL)
		{
			ServerInstance->Users->QuitUser(user, "Filtered: " + f->reason);
		}
		else if (f->action == FA_GLINE)
		{
			GLine* gl = new GLine(ServerInstance->Time(), f->gline_time, ServerInstance->Config->ServerName.c_str(), f->reason.c_str(), "*", user->GetIPString());
			if (ServerInstance->XLines->AddLine(gl,NULL))
			{
				ServerInstance->XLines->ApplyLines();
			}
			else
				delete gl;
		}

		ServerInstance->Logs->Log("FILTER",DEFAULT,"FILTER: "+ user->nick + " had their message filtered, target was " + target + ": " + f->reason + " Action: " + ModuleFilter::FilterActionToString(f->action));
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}

ModResult ModuleFilter::OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
{
	if (validated && IS_LOCAL(user))
	{
		flags = 0;
		bool parting;

		if (command == "QUIT")
		{
			/* QUIT with no reason: nothing to do */
			if (parameters.size() < 1)
				return MOD_RES_PASSTHRU;

			parting = false;
			flags = FLAG_QUIT;
		}
		else if (command == "PART")
		{
			/* PART with no reason: nothing to do */
			if (parameters.size() < 2)
				return MOD_RES_PASSTHRU;

			if (exemptfromfilter.find(parameters[0]) != exemptfromfilter.end())
				return MOD_RES_PASSTHRU;

			parting = true;
			flags = FLAG_PART;
		}
		else
			/* We're only messing with PART and QUIT */
			return MOD_RES_PASSTHRU;

		FilterResult* f = this->FilterMatch(user, parameters[parting ? 1 : 0], flags);
		if (!f)
			/* PART or QUIT reason doesnt match a filter */
			return MOD_RES_PASSTHRU;

		/* We cant block a part or quit, so instead we change the reason to 'Reason filtered' */
		parameters[parting ? 1 : 0] = "Reason filtered";

		/* We're blocking, OR theyre quitting and its a KILL action
		 * (we cant kill someone whos already quitting, so filter them anyway)
		 */
		if ((f->action == FA_BLOCK) || (((!parting) && (f->action == FA_KILL))) || (f->action == FA_SILENT))
		{
			return MOD_RES_PASSTHRU;
		}
		else
		{
			/* Are they parting, if so, kill is applicable */
			if ((parting) && (f->action == FA_KILL))
			{
				user->WriteServ("NOTICE %s :*** Your PART message was filtered: %s", user->nick.c_str(), f->reason.c_str());
				ServerInstance->Users->QuitUser(user, "Filtered: " + f->reason);
			}
			if (f->action == FA_GLINE)
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

void ModuleFilter::OnRehash(User* user)
{
	ConfigTagList tags = ServerInstance->Config->ConfTags("exemptfromfilter");
	exemptfromfilter.clear();
	for (ConfigIter i = tags.first; i != tags.second; ++i)
	{
		std::string chan = i->second->getString("channel");
		if (!chan.empty())
			exemptfromfilter.insert(chan);
	}

	std::string newrxengine = ServerInstance->Config->ConfValue("filteropts")->getString("engine");

	factory = RegexEngine ? (RegexEngine.operator->()) : NULL;

	if (newrxengine.empty())
		RegexEngine.SetProvider("regex");
	else
		RegexEngine.SetProvider("regex/" + newrxengine);

	if (!RegexEngine)
	{
		if (newrxengine.empty())
			ServerInstance->SNO->WriteGlobalSno('a', "WARNING: No regex engine loaded - Filter functionality disabled until this is corrected.");
		else
			ServerInstance->SNO->WriteGlobalSno('a', "WARNING: Regex engine '%s' is not loaded - Filter functionality disabled until this is corrected.", newrxengine.c_str());

		initing = false;
		FreeFilters();
		return;
	}

	if ((!initing) && (RegexEngine.operator->() != factory))
	{
		ServerInstance->SNO->WriteGlobalSno('a', "Dumping all filters due to regex engine change");
		FreeFilters();
	}

	initing = false;
	ReadFilters();
}

Version ModuleFilter::GetVersion()
{
	return Version("Text (spam) filtering", VF_VENDOR | VF_COMMON, RegexEngine ? RegexEngine->name : "");
}

std::string ModuleFilter::EncodeFilter(FilterResult* filter)
{
	std::ostringstream stream;
	std::string x = filter->freeform;

	/* Hax to allow spaces in the freeform without changing the design of the irc protocol */
	for (std::string::iterator n = x.begin(); n != x.end(); n++)
		if (*n == ' ')
			*n = '\7';

	stream << x << " " << FilterActionToString(filter->action) << " " << filter->GetFlags() << " " << filter->gline_time << " :" << filter->reason;
	return stream.str();
}

FilterResult ModuleFilter::DecodeFilter(const std::string &data)
{
	std::string filteraction;
	FilterResult res;
	irc::tokenstream tokens(data);
	tokens.GetToken(res.freeform);
	tokens.GetToken(filteraction);
	if (!StringToFilterAction(filteraction, res.action))
		throw ModuleException("Invalid action: " + filteraction);

	std::string filterflags;
	tokens.GetToken(filterflags);
	char c = res.FillFlags(filterflags);
	if (c != 0)
		throw ModuleException("Invalid flag: '" + std::string(1, c) + "'");

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
	for (std::vector<ImplFilter>::iterator i = filters.begin(); i != filters.end(); ++i)
	{
		proto->ProtoSendMetaData(opaque, NULL, "filter", EncodeFilter(&(*i)));
	}
}

void ModuleFilter::OnDecodeMetaData(Extensible* target, const std::string &extname, const std::string &extdata)
{
	if ((target == NULL) && (extname == "filter"))
	{
		try
		{
			FilterResult data = DecodeFilter(extdata);
			this->AddFilter(data.freeform, data.action, data.reason, data.gline_time, data.GetFlags());
		}
		catch (ModuleException& e)
		{
			ServerInstance->Logs->Log("m_filter", DEBUG, "Error when unserializing filter: " + std::string(e.GetReason()));
		}
	}
}

ImplFilter::ImplFilter(ModuleFilter* mymodule, const std::string &rea, FilterAction act, long glinetime, const std::string &pat, const std::string &flgs)
		: FilterResult(pat, rea, act, glinetime, flgs)
{
	if (!mymodule->RegexEngine)
		throw ModuleException("Regex module implementing '"+mymodule->RegexEngine.GetProvider()+"' is not loaded!");
	regex = mymodule->RegexEngine->Create(pat);
}

FilterResult* ModuleFilter::FilterMatch(User* user, const std::string &text, int flgs)
{
	static std::string stripped_text;
	stripped_text.clear();

	for (std::vector<ImplFilter>::iterator index = filters.begin(); index != filters.end(); index++)
	{
		FilterResult* filter = dynamic_cast<FilterResult*>(&(*index));

		/* Skip ones that dont apply to us */
		if (!AppliesToMe(user, filter, flgs))
			continue;

		if ((filter->flag_strip_color) && (stripped_text.empty()))
		{
			stripped_text = text;
			InspIRCd::StripColor(stripped_text);
		}

		//ServerInstance->Logs->Log("m_filter", DEBUG, "Match '%s' against '%s'", text.c_str(), index->freeform.c_str());
		if (index->regex->Matches(filter->flag_strip_color ? stripped_text : text))
		{
			//ServerInstance->Logs->Log("m_filter", DEBUG, "MATCH");
			return &*index;
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

std::pair<bool, std::string> ModuleFilter::AddFilter(const std::string &freeform, FilterAction type, const std::string &reason, long duration, const std::string &flgs)
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

bool ModuleFilter::StringToFilterAction(const std::string& str, FilterAction& fa)
{
	irc::string s(str.c_str());

	if (s == "gline")
		fa = FA_GLINE;
	else if (s == "block")
		fa = FA_BLOCK;
	else if (s == "silent")
		fa = FA_SILENT;
	else if (s == "kill")
		fa = FA_KILL;
	else if (s == "none")
		fa = FA_NONE;
	else
		return false;

	return true;
}

std::string ModuleFilter::FilterActionToString(FilterAction fa)
{
	switch (fa)
	{
		case FA_GLINE:  return "gline";
		case FA_BLOCK:  return "block";
		case FA_SILENT: return "silent";
		case FA_KILL:   return "kill";
		default:		return "none";
	}
}

void ModuleFilter::ReadFilters()
{
	ConfigTagList tags = ServerInstance->Config->ConfTags("keyword");
	for (ConfigIter i = tags.first; i != tags.second; ++i)
	{
		std::string pattern = i->second->getString("pattern");
		this->DeleteFilter(pattern);

		std::string reason = i->second->getString("reason");
		std::string action = i->second->getString("action");
		std::string flgs = i->second->getString("flags");
		long gline_time = ServerInstance->Duration(i->second->getString("duration"));
		if (flgs.empty())
			flgs = "*";

		FilterAction fa;
		if (!StringToFilterAction(action, fa))
			fa = FA_NONE;

		try
		{
			filters.push_back(ImplFilter(this, reason, fa, gline_time, pattern, flgs));
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
		for (std::vector<ImplFilter>::iterator i = filters.begin(); i != filters.end(); i++)
		{
			results.push_back(ServerInstance->Config->ServerName+" 223 "+user->nick+" :"+RegexEngine.GetProvider()+":"+i->freeform+" "+i->GetFlags()+" "+FilterActionToString(i->action)+" "+ConvToStr(i->gline_time)+" :"+i->reason);
		}
		for (std::set<std::string>::iterator i = exemptfromfilter.begin(); i != exemptfromfilter.end(); ++i)
		{
			results.push_back(ServerInstance->Config->ServerName+" 223 "+user->nick+" :EXEMPT "+(*i));
		}
	}
	return MOD_RES_PASSTHRU;
}

void ModuleFilter::OnUnloadModule(Module* mod)
{
	// If the regex engine became unavailable or has changed, remove all filters
	if (!RegexEngine)
	{
		FreeFilters();
	}
	else if (RegexEngine.operator->() != factory)
	{
		factory = RegexEngine.operator->();
		FreeFilters();
	}
}

MODULE_INIT(ModuleFilter)
