/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

static std::string RegexEngine = "";
static Module* rxengine = NULL;

enum FilterFlags
{
	FLAG_PART = 2,
	FLAG_QUIT = 4,
	FLAG_PRIVMSG = 8,
	FLAG_NOTICE = 16
};

class FilterResult : public classbase
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

	virtual ~FilterResult()
	{
	}
};

class CommandFilter;

class FilterBase : public Module
{
	CommandFilter* filtcommand;
	int flags;
protected:
	std::vector<std::string> exemptfromfilter; // List of channel names excluded from filtering.
 public:
	FilterBase(InspIRCd* Me, const std::string &source);
	virtual ~FilterBase();
	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list);
	virtual FilterResult* FilterMatch(User* user, const std::string &text, int flags) = 0;
	virtual bool DeleteFilter(const std::string &freeform) = 0;
	virtual void SyncFilters(Module* proto, void* opaque) = 0;
	virtual void SendFilter(Module* proto, void* opaque, FilterResult* iter);
	virtual std::pair<bool, std::string> AddFilter(const std::string &freeform, const std::string &type, const std::string &reason, long duration, const std::string &flags) = 0;
	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list);
	virtual void OnRehash(User* user);
	virtual Version GetVersion();
	std::string EncodeFilter(FilterResult* filter);
	FilterResult DecodeFilter(const std::string &data);
	virtual void OnSyncOtherMetaData(Module* proto, void* opaque, bool displayable = false);
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata);
	virtual int OnStats(char symbol, User* user, string_list &results) = 0;
	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line);
	bool AppliesToMe(User* user, FilterResult* filter, int flags);
	void OnLoadModule(Module* mod, const std::string& name);
	virtual void ReadFilters(ConfigReader &MyConf) = 0;
};

class CommandFilter : public Command
{
	FilterBase* Base;
 public:
	CommandFilter(FilterBase* f, InspIRCd* Me, const std::string &ssource) : Command(Me, "FILTER", "o", 1, 5), Base(f)
	{
		this->source = ssource;
		this->syntax = "<filter-definition> <action> <flags> [<gline-duration>] :<reason>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		if (parameters.size() == 1)
		{
			/* Deleting a filter */
			if (Base->DeleteFilter(parameters[0]))
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
				std::pair<bool, std::string> result = Base->AddFilter(freeform, type, reason, duration, flags);
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

	void TooFewParams(User* user, const std::string &extra_text)
	{
		user->WriteServ("NOTICE %s :*** Not enough parameters%s", user->nick.c_str(), extra_text.c_str());
	}
};

bool FilterBase::AppliesToMe(User* user, FilterResult* filter, int iflags)
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

FilterBase::FilterBase(InspIRCd* Me, const std::string &source) : Module(Me)
{
	Me->Modules->UseInterface("RegularExpression");
	filtcommand = new CommandFilter(this, Me, source);
	ServerInstance->AddCommand(filtcommand);
	Implementation eventlist[] = { I_OnPreCommand, I_OnStats, I_OnSyncOtherMetaData, I_OnDecodeMetaData, I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash, I_OnLoadModule };
	ServerInstance->Modules->Attach(eventlist, this, 8);
}

FilterBase::~FilterBase()
{
	ServerInstance->Modules->DoneWithInterface("RegularExpression");
}

int FilterBase::OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
{
	if (!IS_LOCAL(user))
		return 0;

	flags = FLAG_PRIVMSG;
	return OnUserPreNotice(user,dest,target_type,text,status,exempt_list);
}

int FilterBase::OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
{
	/* Leave ulines alone */
	if ((ServerInstance->ULine(user->server)) || (!IS_LOCAL(user)))
		return 0;

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
			if (i != exemptfromfilter.end()) return 0;
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
			GLine* gl = new GLine(ServerInstance, ServerInstance->Time(), f->gline_time, ServerInstance->Config->ServerName, f->reason.c_str(), "*", user->GetIPString());
			if (ServerInstance->XLines->AddLine(gl,NULL))
			{
				ServerInstance->XLines->ApplyLines();
			}
			else
				delete gl;
		}

		ServerInstance->Logs->Log("FILTER",DEFAULT,"FILTER: "+ user->nick + " had their message filtered, target was " + target + ": " + f->reason + " Action: " + f->action);
		return 1;
	}
	return 0;
}

int FilterBase::OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
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
				return 0;

			checkline = parameters[0];
			replacepoint = 0;
			parting = false;
			flags = FLAG_QUIT;
		}
		else if (command == "PART")
		{
			/* PART with no reason: nothing to do */
			if (parameters.size() < 2)
				return 0;

			std::vector<std::string>::iterator i = find(exemptfromfilter.begin(), exemptfromfilter.end(), parameters[0]);
			if (i != exemptfromfilter.end()) return 0;
			checkline = parameters[1];
			replacepoint = 1;
			parting = true;
			flags = FLAG_PART;
		}
		else
			/* We're only messing with PART and QUIT */
			return 0;

		FilterResult* f = NULL;

		if (flags)
			f = this->FilterMatch(user, checkline, flags);

		if (!f)
			/* PART or QUIT reason doesnt match a filter */
			return 0;

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
				return 1;
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
					GLine* gl = new GLine(ServerInstance, ServerInstance->Time(), f->gline_time, ServerInstance->Config->ServerName, f->reason.c_str(), "*", user->GetIPString());
					if (ServerInstance->XLines->AddLine(gl,NULL))
					{
						ServerInstance->XLines->ApplyLines();
					}
					else
						delete gl;
				}
				return 1;
			}
		}
		return 0;
	}
	return 0;
}

void FilterBase::OnRehash(User* user)
{
	ConfigReader MyConf(ServerInstance);
	std::vector<std::string>().swap(exemptfromfilter);
	for (int index = 0; index < MyConf.Enumerate("exemptfromfilter"); ++index)
	{
		std::string chan = MyConf.ReadValue("exemptfromfilter", "channel", index);
		if (!chan.empty()) {
			exemptfromfilter.push_back(chan);
		}
	}
	std::string newrxengine = MyConf.ReadValue("filteropts", "engine", 0);
	if (!RegexEngine.empty())
	{
		if (RegexEngine == newrxengine)
			return;

		ServerInstance->SNO->WriteGlobalSno('a', "Dumping all filters due to regex engine change (was '%s', now '%s')", RegexEngine.c_str(), newrxengine.c_str());
		//ServerInstance->XLines->DelAll("R");
	}
	rxengine = NULL;

	RegexEngine = newrxengine;
	modulelist* ml = ServerInstance->Modules->FindInterface("RegularExpression");
	if (ml)
	{
		for (modulelist::iterator i = ml->begin(); i != ml->end(); ++i)
		{
			if (RegexNameRequest(this, *i).Send() == newrxengine)
			{
				ServerInstance->SNO->WriteGlobalSno('a', "Filter now using engine '%s'", RegexEngine.c_str());
				rxengine = *i;
			}
		}
	}
	if (!rxengine)
	{
		ServerInstance->SNO->WriteGlobalSno('a', "WARNING: Regex engine '%s' is not loaded - Filter functionality disabled until this is corrected.", RegexEngine.c_str());
	}
}

void FilterBase::OnLoadModule(Module* mod, const std::string& name)
{
	if (ServerInstance->Modules->ModuleHasInterface(mod, "RegularExpression"))
	{
		std::string rxname = RegexNameRequest(this, mod).Send();
		if (rxname == RegexEngine)
		{
			rxengine = mod;
			/* Force a rehash to make sure that any filters that couldnt be applied from the conf
			 * on startup or on load are applied right now.
			 */
			ConfigReader Config(ServerInstance);
			ServerInstance->SNO->WriteGlobalSno('a', "Found and activated regex module '%s' for m_filter.so.", RegexEngine.c_str());
			ReadFilters(Config);
		}
	}
}


Version FilterBase::GetVersion()
{
	return Version("$Id$", VF_VENDOR | VF_COMMON, API_VERSION);
}


std::string FilterBase::EncodeFilter(FilterResult* filter)
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

FilterResult FilterBase::DecodeFilter(const std::string &data)
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

void FilterBase::OnSyncOtherMetaData(Module* proto, void* opaque, bool displayable)
{
	this->SyncFilters(proto, opaque);
}

void FilterBase::SendFilter(Module* proto, void* opaque, FilterResult* iter)
{
	proto->ProtoSendMetaData(opaque, TYPE_OTHER, NULL, "filter", EncodeFilter(iter));
}

void FilterBase::OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
{
	if ((target_type == TYPE_OTHER) && (extname == "filter"))
	{
		FilterResult data = DecodeFilter(extdata);
		this->AddFilter(data.freeform, data.action, data.reason, data.gline_time, data.flags);
	}
}

class ImplFilter : public FilterResult
{
 public:
	Regex* regex;

	ImplFilter(Module* mymodule, const std::string &rea, const std::string &act, long glinetime, const std::string &pat, const std::string &flgs)
		: FilterResult(pat, rea, act, glinetime, flgs)
	{
		if (!rxengine)
			throw ModuleException("Regex module implementing '"+RegexEngine+"' is not loaded!");

		regex = RegexFactoryRequest(mymodule, rxengine, pat).Create();
	}

	ImplFilter()
	{
	}
};

class ModuleFilter : public FilterBase
{
	std::vector<ImplFilter> filters;
	const char *error;
	int erroffset;
	ImplFilter fr;

 public:
	ModuleFilter(InspIRCd* Me)
	: FilterBase(Me, "m_filter.so")
	{
		OnRehash(NULL);
	}

	virtual ~ModuleFilter()
	{
	}

	virtual FilterResult* FilterMatch(User* user, const std::string &text, int flgs)
	{
		for (std::vector<ImplFilter>::iterator index = filters.begin(); index != filters.end(); index++)
		{
			/* Skip ones that dont apply to us */
			if (!FilterBase::AppliesToMe(user, dynamic_cast<FilterResult*>(&(*index)), flgs))
				continue;

			//ServerInstance->Logs->Log("m_filter", DEBUG, "Match '%s' against '%s'", text.c_str(), index->freeform.c_str());
			if (index->regex->Matches(text))
			{
				//ServerInstance->Logs->Log("m_filter", DEBUG, "MATCH");
				fr = *index;
				if (index != filters.begin())
				{
					/* Move to head of list for efficiency */
					filters.erase(index);
					filters.insert(filters.begin(), fr);
				}
				return &fr;
			}
			//ServerInstance->Logs->Log("m_filter", DEBUG, "NO MATCH");
		}
		return NULL;
	}

	virtual bool DeleteFilter(const std::string &freeform)
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

	virtual void SyncFilters(Module* proto, void* opaque)
	{
		for (std::vector<ImplFilter>::iterator i = filters.begin(); i != filters.end(); i++)
		{
			this->SendFilter(proto, opaque, &(*i));
		}
	}

	virtual std::pair<bool, std::string> AddFilter(const std::string &freeform, const std::string &type, const std::string &reason, long duration, const std::string &flgs)
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

	virtual void OnRehash(User* user)
	{
		ConfigReader MyConf(ServerInstance);
		FilterBase::OnRehash(user);
		ReadFilters(MyConf);
	}

	void ReadFilters(ConfigReader &MyConf)
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

	virtual int OnStats(char symbol, User* user, string_list &results)
	{
		if (symbol == 's')
		{
			std::string sn = ServerInstance->Config->ServerName;
			for (std::vector<ImplFilter>::iterator i = filters.begin(); i != filters.end(); i++)
			{
				results.push_back(sn+" 223 "+user->nick+" :"+RegexEngine+":"+i->freeform+" "+i->flags+" "+i->action+" "+ConvToStr(i->gline_time)+" :"+i->reason);
			}
			for (std::vector<std::string>::iterator i = exemptfromfilter.begin(); i != exemptfromfilter.end(); ++i)
			{
				results.push_back(sn+" 223 "+user->nick+" :EXEMPT "+(*i));
			}
		}
		return 0;
	}
};

MODULE_INIT(ModuleFilter)
