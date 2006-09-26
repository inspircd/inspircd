/*      +------------------------------------+
 *      | Inspire Internet Relay Chat Daemon |
 *      +------------------------------------+
 *
 * InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                    E-mail:
 *              <brain@chatspike.net>
 *              <Craig@chatspike.net>
 *                <omster@gmail.com>
 * 
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 * the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <algorithm>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "configreader.h"
#include "inspircd.h"

/* $ModDesc: Implements SVSHOLD. Like Q:Lines, but can only be added/removed by Services. */

/** Holds a SVSHold item
 */
class SVSHold : public classbase
{
public:
	std::string nickname;
	std::string set_by;
	time_t set_on;
	long length;
	std::string reason;

	SVSHold()
	{
	}

	SVSHold(std::string nn, std::string sb, time_t so, long ln, std::string rs) : nickname(nn), set_by(sb), set_on(so), length(ln), reason(rs)
	{
	}
};


bool SVSHoldComp(const SVSHold &ban1, const SVSHold &ban2);

typedef std::vector<SVSHold> SVSHoldlist;

/* SVSHolds is declared here, as our type is right above. Don't try move it. */
SVSHoldlist SVSHolds;

/** Handle /SVSHold
 */
class cmd_svshold : public command_t
{
 public:
	cmd_svshold(InspIRCd* Me) : command_t(Me, "SVSHOLD", 'o', 1)
	{
		this->source = "m_svshold.so";
		this->
		syntax = "<nickname> [<duration> :<reason>]";
	}

	CmdResult Handle(const char** parameters, int pcnt, userrec *user)
	{
		/* syntax: svshold nickname time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		if (pcnt == 1)
		{
			/* form: svshold nickname removes a hold. */
			for (SVSHoldlist::iterator iter = SVSHolds.begin(); iter != SVSHolds.end(); iter++)
			{
				if (irc::string(parameters[0]) == irc::string(iter->nickname.c_str()))
				{
					unsigned long remaining = (iter->set_on + iter->length) - ServerInstance->Time();
					user->WriteServ( "386 %s %s :Removed SVSHOLD with %lu seconds left before expiry (%s)", user->nick, iter->nickname.c_str(), remaining, iter->reason.c_str());
					SVSHolds.erase(iter);
					break;
				}
			}
		}
		else if (pcnt >= 2)
		{
			/* full form to add a SVSHold */
			if (ServerInstance->IsNick(parameters[0]))
			{
				// parameters[0] = w00t
				// parameters[1] = 1h3m2s
				// parameters[2] = Registered nickname
				long length = ServerInstance->Duration(parameters[1]);
				std::string reason = (pcnt > 2) ? parameters[2] : "No reason supplied";
				
				SVSHolds.push_back(SVSHold(parameters[0], user->nick, ServerInstance->Time(), length, reason));
					
				std::sort(SVSHolds.begin(), SVSHolds.end(), SVSHoldComp);
				
				if(length > 0)
				{
					user->WriteServ( "385 %s %s :Added %lu second SVSHOLD (%s)", user->nick, parameters[0], length, reason.c_str());
					ServerInstance->WriteOpers("*** %s added %lu second SVSHOLD on %s (%s)", user->nick, length, parameters[0], reason.c_str());
				}
				else
				{
					user->WriteServ( "385 %s %s :Added permanent SVSHOLD on %s (%s)", user->nick, parameters[0], reason.c_str());
					ServerInstance->WriteOpers("*** %s added permanent SVSHOLD on %s (%s)", user->nick, parameters[0], reason.c_str());
				}
			}
			else
			{
				/* as this is primarily a Services command, do not provide an error */
				return CMD_FAILURE;
			}
		}

		return CMD_SUCCESS;
	}
};

bool SVSHoldComp(const SVSHold &ban1, const SVSHold &ban2)
{
	return ((ban1.set_on + ban1.length) < (ban2.set_on + ban2.length));
}

class ModuleSVSHold : public Module
{
	cmd_svshold *mycommand;
	

 public:
	ModuleSVSHold(InspIRCd* Me) : Module::Module(Me)
	{
		mycommand = new cmd_svshold(Me);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreNick] = List[I_OnSyncOtherMetaData] = List[I_OnDecodeMetaData] = List[I_OnStats] = 1;
	}
	
	virtual int OnStats(char symbol, userrec* user, string_list &results)
	{
		ExpireBans();
	
		if(symbol == 'S')
		{
			for(SVSHoldlist::iterator iter = SVSHolds.begin(); iter != SVSHolds.end(); iter++)
			{
				unsigned long remaining = (iter->set_on + iter->length) - ServerInstance->Time();
				results.push_back(std::string(ServerInstance->Config->ServerName)+" 210 "+user->nick+" "+iter->nickname.c_str()+" "+iter->set_by+" "+ConvToStr(iter->set_on)+" "+ConvToStr(iter->length)+" "+ConvToStr(remaining)+" :"+iter->reason);
			}
		}
		
		return 0;
	}

	virtual int OnUserPreNick(userrec *user, const std::string &newnick)
	{
		ExpireBans();
	
		/* check SVSHolds in here, and apply as necessary. */
		for(SVSHoldlist::iterator iter = SVSHolds.begin(); iter != SVSHolds.end(); iter++)
		{
			if (irc::string(iter->nickname.c_str()) == irc::string(newnick.c_str()))
			{
				// nope, boned.
				user->WriteServ( "432 %s %s :Reserved nickname: %s", user->nick, newnick.c_str(), iter->reason.c_str());
				return 1;
			}
		}
		return 0;
	}
	
	virtual void OnSyncOtherMetaData(Module* proto, void* opaque)
	{
		for(SVSHoldlist::iterator iter = SVSHolds.begin(); iter != SVSHolds.end(); iter++)
		{
			proto->ProtoSendMetaData(opaque, TYPE_OTHER, NULL, "SVSHold", EncodeSVSHold(*iter));
		}
	}
	
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		if((target_type == TYPE_OTHER) && (extname == "SVSHold"))
		{
			SVSHolds.push_back(DecodeSVSHold(extdata));
			std::sort(SVSHolds.begin(), SVSHolds.end(), SVSHoldComp);
		}
	}

	virtual ~ModuleSVSHold()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}

	std::string EncodeSVSHold(const SVSHold &ban)
	{
		std::ostringstream stream;	
		stream << ban.nickname << " " << ban.set_by << " " << ban.set_on << " " << ban.length << " " << ban.reason;
		return stream.str();	
	}

	SVSHold DecodeSVSHold(const std::string &data)
	{
		SVSHold res;
		std::istringstream stream(data);
		stream >> res.nickname;
		stream >> res.set_by;
		stream >> res.set_on;
		stream >> res.length;
		res.reason = stream.str();
	
		return res;
	}

	void ExpireBans()
	{
		bool go_again = true;

		while (go_again)
		{
			go_again = false;
	
			for (SVSHoldlist::iterator iter = SVSHolds.begin(); iter != SVSHolds.end(); iter++)
			{
				/* 0 == permanent, don't mess with them! -- w00t */
				if (iter->length != 0)
				{
					if (iter->set_on + iter->length <= ServerInstance->Time())
					{
						ServerInstance->Log(DEBUG, "m_svshold.so: hold on %s expired, removing...", iter->nickname.c_str());
						ServerInstance->WriteOpers("*** %li second SVSHOLD on %s (%s) set %u seconds ago expired", iter->length, iter->nickname.c_str(), iter->reason.c_str(), ServerInstance->Time() - iter->set_on);
						SVSHolds.erase(iter);
						go_again = true;
					}
				}
	
				if (go_again == true)
					break;
			}
		}
	}
};

class ModuleSVSHoldFactory : public ModuleFactory
{
 public:
	ModuleSVSHoldFactory()
	{
	}
	
	~ModuleSVSHoldFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSVSHold(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSVSHoldFactory;
}
