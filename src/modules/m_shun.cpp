#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include "inspircd.h"
#include "modules.h"
#include "hashcomp.h"
#include "configreader.h"

/* $ModDesc: Gives the /shun command, which stops a user executing all commands except PING and PONG. */
/* $ModAuthor: Om */
/* $ModAuthorMail: om@inspircd.org */

class Shun
{
public:
	std::string banmask;
	std::string set_by;
	time_t set_on;
	long length;
	std::string reason;

	Shun() { }

	Shun(std::string bm, std::string sb, time_t so, long ln, std::string rs) : banmask(bm), set_by(sb), set_on(so), length(ln), reason(rs) { }
	
	bool operator<(const Shun &ban2) const
	{
		return ((this->set_on + this->length) < (ban2.set_on + ban2.length));
	}
};

typedef std::vector<Shun> shunlist;

class ModuleShunBase
{
 public:
	/* shuns is declared here, as our type is right above. Don't try move it. */
	shunlist shuns;
 
 	InspIRCd* Srv;
	 
	std::string EncodeShun(const Shun &shun)
	{
		std::ostringstream stream;	
		stream << shun.banmask << " " << shun.set_by << " " << shun.set_on << " " << shun.length << " " << shun.reason;
		return stream.str();	
	}

	Shun DecodeShun(const std::string &data)
	{
		Shun res;
		std::istringstream stream(data);
		stream >> res.banmask;
		stream >> res.set_by;
		stream >> res.set_on;
		stream >> res.length;
		res.reason = stream.str();
	
		return res;
	}

	void ExpireBans()
	{
		while(shuns.size() && shuns.begin()->length && ((shuns.begin()->set_on + shuns.begin()->length) <= Srv->Time()))
		{
			shunlist::iterator iter = shuns.begin();
		
			Srv->SNO->WriteToSnoMask('X', "*** %ld second shun on '%s' (%s) set by %s %ld seconds ago expired", iter->length, iter->banmask.c_str(), iter->reason.c_str(), iter->set_by.c_str(), Srv->Time() - iter->set_on);
			shuns.erase(iter);
		}
	}
};

class cmd_shun : public Command
{
 private:
	InspIRCd *Srv;
	ModuleShunBase* base;

 public:
	cmd_shun(InspIRCd* Me, ModuleShunBase* b)
	: Command(Me, "SHUN", "o", 1), Srv(Me), base(b)
	{
		this->source = "m_shun.so";
	}

	CmdResult Handle(const char* const*parameters, int pcnt, User *user)
	{
		/* syntax: SHUN nick!user@host time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */
		
		base->ExpireBans();

		if(pcnt == 1)
		{
			/* form: SHUN mask removes a SHUN */
			for(shunlist::iterator iter = base->shuns.begin(); iter != base->shuns.end(); iter++)
			{
				if(parameters[0] == iter->banmask)
				{
					Srv->SNO->WriteToSnoMask('X', "*** %s removed shun '%s', set %ld seconds ago with reason '%s'", user->nick, iter->banmask.c_str(), Srv->Time() - iter->set_on, iter->reason.c_str());
					base->shuns.erase(iter);
					return CMD_SUCCESS;
				}
			}

			user->WriteServ("NOTICE %s :*** The mask %s is not currently shunned, try /stats s", user->nick, parameters[0]);
			return CMD_FAILURE;
		}
		else if (pcnt >= 2)
		{
			/* full form to add a shun */
			if(Srv->IsValidMask(parameters[0]))
			{
				// parameters[0] = Foamy!*@*
				// parameters[1] = 1h3m2s
				// parameters[2] = Tortoise abuser
				for(shunlist::iterator iter = base->shuns.begin(); iter != base->shuns.end(); iter++)
				{
					if (parameters[0] == iter->banmask)
					{
						user->WriteServ("NOTICE %s :*** Shun on %s already exists", user->nick, parameters[0]);
						return CMD_FAILURE;
					}
				}
				
				long length = Srv->Duration(parameters[1]);
				
				std::string reason = (pcnt > 2) ? parameters[2] : "No reason supplied";
				
				base->shuns.push_back(Shun(parameters[0], user->nick, Srv->Time(), length, reason));
					
				std::sort(base->shuns.begin(), base->shuns.end());
				
				if(length > 0)
					Srv->SNO->WriteToSnoMask('X', "*** %s added %ld second shun on '%s' (%s)", user->nick, length, parameters[0], reason.c_str());
				else
					Srv->SNO->WriteToSnoMask('X', "*** %s added permanent shun on '%s' (%s)", user->nick, parameters[0], reason.c_str());
				
				return CMD_SUCCESS;
			}
			else
			{
				user->WriteServ("NOTICE %s :*** Invalid ban mask (%s)", user->nick, parameters[0]);
			}
		}

		return CMD_FAILURE;
	}
};

class ModuleShun : public Module, public ModuleShunBase
{
	cmd_shun* mycommand;

 public:
	ModuleShun(InspIRCd* Me)
	: Module::Module(Me)
	{
		this->Srv = Me;
		mycommand = new cmd_shun(Srv, this);
		Srv->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnPreCommand] = List[I_OnSyncOtherMetaData] = List[I_OnDecodeMetaData] = List[I_OnStats] = 1;
	}
	
	virtual int OnStats(char symbol, User* user, string_list& out)
	{
		ExpireBans();
	
		if(symbol == 's')
		{
			for(shunlist::iterator iter = shuns.begin(); iter != shuns.end(); iter++)
			{
				std::ostringstream format;
				format << Srv->Config->ServerName << " 223 " << user->nick << " :" << iter->banmask << " " << iter->set_on << " " << iter->length << " " << iter->set_by << " " << iter->reason;
				out.push_back(format.str());
			}
		}
		
		return 0;
	}

	virtual int OnPreCommand(const std::string &command, const char* const*parameters, int pcnt, User* user, bool validated, const std::string &original_line)
	{
		if((command != "PONG") && (command != "PING"))
		{
			ExpireBans();
		
			for(shunlist::iterator iter = shuns.begin(); iter != shuns.end(); iter++)
				if(Srv->MatchText(user->GetFullHost(), iter->banmask) || Srv->MatchText(user->GetFullRealHost(), iter->banmask))
					return 1;
		}
		
		return 0;
	}

	virtual void OnSyncOtherMetaData(Module* proto, void* opaque, bool displayable)
	{
		for(shunlist::iterator iter = shuns.begin(); iter != shuns.end(); iter++)
		{
			proto->ProtoSendMetaData(opaque, TYPE_OTHER, NULL, "shun", EncodeShun(*iter));
		}
	}

	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		if((target_type == TYPE_OTHER) && (extname == "shun"))
		{
			shuns.push_back(DecodeShun(extdata));
			std::sort(shuns.begin(), shuns.end());
		}
	}

	virtual ~ModuleShun()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,0,API_VERSION);
	}
};

MODULE_INIT(ModuleShun)

