#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include "inspircd.h"
#include "modules.h"
#include "hashcomp.h"
#include "configreader.h"
#include "xline.h"

/* $ModDesc: Provides the /shun command, which stops a user executing all commands except PING and PONG. */

class Shun : public XLine
{
public:
	std::string matchtext;

	Shun(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char *shunmask) : XLine(Instance, s_time, d, src, re, "SHUN")
	{
		this->matchtext = shunmask;
	}

	~Shun()
	{
	}

	bool Matches(User *u)
	{
		if (ServerInstance->MatchText(u->GetFullHost(), matchtext) || ServerInstance->MatchText(u->GetFullRealHost(), matchtext))
			return true;

		return false;
	}

	bool Matches(const std::string &s)
	{
		if (matchtext == s)
			return true;
		return false;
	}

	void Apply(User *u)
	{
		// Application is done by the module.
	}


	void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x',"Expiring timed shun %s (set by %s %ld seconds ago)", this->matchtext.c_str(), this->source, this->duration);
	}

	const char* Displayable()
	{
		return matchtext.c_str();
	}
};

/** An XLineFactory specialized to generate shun pointers
 */
class ShunFactory : public XLineFactory
{
 public:
	ShunFactory(InspIRCd* Instance) : XLineFactory(Instance, "SHUN") { }

	/** Generate a shun
 	*/
	XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
	{
		return new Shun(ServerInstance, set_time, duration, source, reason, xline_specific_mask);
	}

	virtual bool AutoApplyToUserList(XLine*)
	{
		// No, we don't want to be applied to users automagically.
		return false;
	}
};

//typedef std::vector<Shun> shunlist;

class cmd_shun : public Command
{
 private:
	InspIRCd *Srv;

 public:
	cmd_shun(InspIRCd* Me) : Command(Me, "SHUN", "o", 1), Srv(Me)
	{
		this->source = "m_shun.so";
	}

	CmdResult Handle(const char* const*parameters, int pcnt, User *user)
	{
		/* syntax: SHUN nick!user@host time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		if(pcnt == 1)
		{
			if (ServerInstance->XLines->DelLine(parameters[0], "S", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s Removed shun on %s.",user->nick,parameters[0]);
			}
			else
			{
				// XXX todo implement stats
				user->WriteServ("NOTICE %s :*** Shun %s not found in list, try /stats s.",user->nick,parameters[0]);
			}

			return CMD_SUCCESS;
		}
		else if (pcnt >= 2)
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..
			long duration = ServerInstance->Duration(parameters[1]);
			Shun *r = NULL;

			try
			{
				r = new Shun(ServerInstance, ServerInstance->Time(), duration, user->nick, parameters[2], parameters[0]);
			}
			catch (...)
			{
				; // Do nothing. If we get here, the regex was fucked up, and they already got told it fucked up.
			}

			if (r)
			{
				if (ServerInstance->XLines->AddLine(r, user))
				{
					if (!duration)
					{
						ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent shun for %s.", user->nick, parameters[0]);
					}
					else
					{
						time_t c_requires_crap = duration + ServerInstance->Time();
						ServerInstance->SNO->WriteToSnoMask('x', "%s added timed shun for %s, expires on %s", user->nick, parameters[0],
						ServerInstance->TimeString(c_requires_crap).c_str());
					}

					ServerInstance->XLines->ApplyLines();
				}
				else
				{
					delete r;
					user->WriteServ("NOTICE %s :*** Shun for %s already exists", user->nick, parameters[0]);
				}
			}
		}

		return CMD_FAILURE;
	}
};

class ModuleShun : public Module
{
	cmd_shun* mycommand;
	ShunFactory *f;

 public:
	ModuleShun(InspIRCd* Me) : Module(Me)
	{
		f = new ShunFactory(ServerInstance);
		ServerInstance->XLines->RegisterFactory(f);

		mycommand = new cmd_shun(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}

	virtual ~ModuleShun()
	{
		ServerInstance->XLines->UnregisterFactory(f);
	}

	void Implements(char* List)
	{
		List[I_OnPreCommand] = List[I_OnStats] = 1;
	}
	
	virtual int OnStats(char symbol, User* user, string_list& out)
	{
		// XXX write me
//format << Srv->Config->ServerName << " 223 " << user->nick << " :" << iter->banmask << " " << iter->set_on << " " << iter->length << " " << 
//iter->set_by << " " << iter->reason; 
		
		return 0;
	}

	virtual int OnPreCommand(const std::string &command, const char* const*parameters, int pcnt, User* user, bool validated, const std::string &original_line)
	{
		if (user->registered != REG_ALL)
			return 0;

		if((command != "PONG") && (command != "PING"))
		{
			// Don't let them issue cmd if they are shunned..
			XLine *rl = ServerInstance->XLines->MatchesLine("S", user);

			if (rl)
			{
				return 1;
			}
		}

		return 0;
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,0,API_VERSION);
	}
};

MODULE_INIT(ModuleShun)

