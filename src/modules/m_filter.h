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

#include "xline.h"

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

	FilterResult(const std::string free, const std::string &rea, const std::string &act, long gt, const std::string &fla) : freeform(free), reason(rea),
									action(act), gline_time(gt), flags(fla)
	{
		this->FillFlags(flags);
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

class cmd_filter;

class FilterBase : public Module
{
	cmd_filter* filtcommand;
	int flags;
 public:
	FilterBase(InspIRCd* Me, const std::string &source);
	virtual ~FilterBase();
	virtual void Implements(char* List);
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list);
	virtual FilterResult* FilterMatch(userrec* user, const std::string &text, int flags) = 0;
	virtual bool DeleteFilter(const std::string &freeform) = 0;
	virtual void SyncFilters(Module* proto, void* opaque) = 0;
	virtual void SendFilter(Module* proto, void* opaque, FilterResult* iter);
	virtual std::pair<bool, std::string> AddFilter(const std::string &freeform, const std::string &type, const std::string &reason, long duration, const std::string &flags) = 0;
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list);
	virtual void OnRehash(userrec* user, const std::string &parameter);
	virtual Version GetVersion();
	std::string EncodeFilter(FilterResult* filter);
	FilterResult DecodeFilter(const std::string &data);
	virtual void OnSyncOtherMetaData(Module* proto, void* opaque, bool displayable = false);
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata);
	virtual int OnStats(char symbol, userrec* user, string_list &results) = 0;
	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line);
	bool AppliesToMe(userrec* user, FilterResult* filter, int flags);
};

class cmd_filter : public command_t
{
	FilterBase* Base;
 public:
	cmd_filter(FilterBase* f, InspIRCd* Me, const std::string &source) : command_t(Me, "FILTER", 'o', 1), Base(f)
	{
		this->source = source;
		this->syntax = "<filter-definition> <type> <flags> [<gline-duration>] :<reason>";
	}

	CmdResult Handle(const char** parameters, int pcnt, userrec *user)
	{
		if (pcnt == 1)
		{
			/* Deleting a filter */
			if (Base->DeleteFilter(parameters[0]))
			{
				user->WriteServ("NOTICE %s :*** Deleted filter '%s'", user->nick, parameters[0]);
				return CMD_SUCCESS;
			}
			else
			{
				user->WriteServ("NOTICE %s :*** Filter '%s' not found on list.", user->nick, parameters[0]);
				return CMD_FAILURE;
			}
		}
		else
		{
			/* Adding a filter */
			if (pcnt >= 4)
			{
				std::string freeform = parameters[0];
				std::string type = parameters[1];
				std::string flags = parameters[2];
				std::string reason;
				long duration = 0;


				if ((type != "gline") && (type != "none") && (type != "block") && (type != "kill") && (type != "silent"))
				{
					user->WriteServ("NOTICE %s :*** Invalid filter type '%s'. Supported types are 'gline', 'none', 'block', 'silent' and 'kill'.", user->nick, freeform.c_str());
					return CMD_FAILURE;
				}

				if (type == "gline")
				{
					if (pcnt >= 5)
					{
						duration = ServerInstance->Duration(parameters[3]);
						reason = parameters[4];
					}
					else
					{
						this->TooFewParams(user, " When setting a gline type filter, a gline duration must be specified as the third parameter.");
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
					user->WriteServ("NOTICE %s :*** Added filter '%s', type '%s'%s%s, flags '%s', reason: '%s'", user->nick, freeform.c_str(),
							type.c_str(), (duration ? " duration: " : ""), (duration ? parameters[3] : ""),
							flags.c_str(), reason.c_str());
					return CMD_SUCCESS;
				}
				else
				{
					user->WriteServ("NOTICE %s :*** Filter '%s' could not be added: %s", user->nick, freeform.c_str(), result.second.c_str());
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

	void TooFewParams(userrec* user, const std::string &extra_text)
	{
		user->WriteServ("NOTICE %s :*** Not enough parameters%s", user->nick, extra_text.c_str());
	}
};

bool FilterBase::AppliesToMe(userrec* user, FilterResult* filter, int flags)
{
	if ((filter->flag_no_opers) && IS_OPER(user))
		return false;
	if ((flags & FLAG_PRIVMSG) && (!filter->flag_privmsg))
		return false;
	if ((flags & FLAG_NOTICE) && (!filter->flag_notice))
		return false;
	if ((flags & FLAG_QUIT)   && (!filter->flag_quit_message))
		return false;
	if ((flags & FLAG_PART)   && (!filter->flag_part_message))
		return false;
	return true;
}

FilterBase::FilterBase(InspIRCd* Me, const std::string &source) : Module(Me)
{
	filtcommand = new cmd_filter(this, Me, source);
	ServerInstance->AddCommand(filtcommand);
}

FilterBase::~FilterBase()
{
}

void FilterBase::Implements(char* List)
{
	List[I_OnPreCommand] = List[I_OnStats] = List[I_OnSyncOtherMetaData] = List[I_OnDecodeMetaData] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnRehash] = 1;
}

int FilterBase::OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
{
	flags = FLAG_PRIVMSG;
	return OnUserPreNotice(user,dest,target_type,text,status,exempt_list);
}

int FilterBase::OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
{
	if (!flags)
		flags = FLAG_NOTICE;

	/* Leave ulines alone */
	if ((ServerInstance->ULine(user->server)) || (!IS_LOCAL(user)))
		return 0;

	FilterResult* f = this->FilterMatch(user, text, flags);
	if (f)
	{
		std::string target = "";
		if (target_type == TYPE_USER)
		{
			userrec* t = (userrec*)dest;
			target = std::string(t->nick);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			chanrec* t = (chanrec*)dest;
			target = std::string(t->name);
		}
		if (f->action == "block")
		{	
			ServerInstance->WriteOpers(std::string("FILTER: ")+user->nick+" had their message filtered, target was "+target+": "+f->reason);
			user->WriteServ("NOTICE "+std::string(user->nick)+" :Your message has been filtered and opers notified: "+f->reason);
		}
		if (f->action == "silent")
		{
			user->WriteServ("NOTICE "+std::string(user->nick)+" :Your message has been filtered: "+f->reason);
		}
		if (f->action == "kill")
		{
			userrec::QuitUser(ServerInstance,user,"Filtered: "+f->reason);
		}
		if (f->action == "gline")
		{
			if (ServerInstance->XLines->add_gline(f->gline_time, ServerInstance->Config->ServerName, f->reason.c_str(), user->MakeHostIP()))
			{
				ServerInstance->XLines->apply_lines(APPLY_GLINES);
				FOREACH_MOD(I_OnAddGLine,OnAddGLine(f->gline_time, NULL, f->reason, user->MakeHostIP()));
			}
		}

		ServerInstance->Log(DEFAULT,"FILTER: "+std::string(user->nick)+std::string(" had their message filtered, target was ")+target+": "+f->reason+" Action: "+f->action);
		return 1;
	}
	return 0;
}

int FilterBase::OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
{
	flags = 0;
	if ((validated == 1) && (IS_LOCAL(user)))
	{
		std::string checkline;
		int replacepoint = 0;
		bool parting = false;
	
		if (command == "QUIT")
		{
			/* QUIT with no reason: nothing to do */
			if (pcnt < 1)
				return 0;

			checkline = parameters[0];
			replacepoint = 0;
			parting = false;
			flags = FLAG_QUIT;
		}
		else if (command == "PART")
		{
			/* PART with no reason: nothing to do */
			if (pcnt < 2)
				return 0;

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
		command_t* c = ServerInstance->Parser->GetHandler(command);
		if (c)
		{
			const char* params[127];
			for (int item = 0; item < pcnt; item++)
				params[item] = parameters[item];
			params[replacepoint] = "Reason filtered";

			/* We're blocking, OR theyre quitting and its a KILL action
			 * (we cant kill someone whos already quitting, so filter them anyway)
			 */
			if ((f->action == "block") || (((!parting) && (f->action == "kill"))) || (f->action == "silent"))
			{
				c->Handle(params, pcnt, user);
				return 1;
			}
			else
			{
				/* Are they parting, if so, kill is applicable */
				if ((parting) && (f->action == "kill"))
				{
					user->SetWriteError("Filtered: "+f->reason);
					/* This WriteServ causes the write error to be applied.
					 * Its not safe to kill here with QuitUser in a PreCommand handler,
					 * so we do it this way, which is safe just about anywhere.
					 */
					user->WriteServ("NOTICE %s :*** Your PART message was filtered: %s", user->nick, f->reason.c_str());
				}
				if (f->action == "gline")
				{
					/* Note: We gline *@IP so that if their host doesnt resolve the gline still applies. */
					std::string wild = "*@";
					wild.append(user->GetIPString());

					if (ServerInstance->XLines->add_gline(f->gline_time, ServerInstance->Config->ServerName, f->reason.c_str(), wild.c_str()))
					{
						ServerInstance->XLines->apply_lines(APPLY_GLINES);
						FOREACH_MOD(I_OnAddGLine,OnAddGLine(f->gline_time, NULL, f->reason, user->MakeHostIP()));
					}
				}
				return 1;
			}
		}
		return 0;
	}
	return 0;
}

void FilterBase::OnRehash(userrec* user, const std::string &parameter)
{
}
	
Version FilterBase::GetVersion()
{
	return Version(1,1,0,2,VF_VENDOR|VF_COMMON,API_VERSION);
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

